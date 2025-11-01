#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstdint>
#ifdef GWATCH_PROFILE
#include <chrono>
#include "../include/Profiling.h"
#endif

#include "ProcessLauncher.h"
#include "../include/WinUtil.h"

namespace gwatch
{
	namespace
	{
		constexpr DWORD kWaitMs = INFINITE;
	}

	WindowsProcessLauncher::WindowsProcessLauncher() = default;

	WindowsProcessLauncher::~WindowsProcessLauncher()
	{
		if (m_hThread)
		{
			CloseHandle(m_hThread);
			m_hThread = nullptr;
		}
		if (m_hProcess)
		{
			CloseHandle(m_hProcess);
			m_hProcess = nullptr;
		}
	}

	void WindowsProcessLauncher::launch(const LaunchConfig& cfg)
	{
		if (m_launched)
		{
			throw ProcessError("Process already launched with this WindowsProcessLauncher instance.");
		}

		std::wstring cmd = build_command_line(cfg);
		const std::wstring wdir = to_wstring(cfg.workdir.value_or(""));

		DWORD creationFlags = 0;
		creationFlags |= cfg.debug_children ? DEBUG_PROCESS : DEBUG_ONLY_THIS_PROCESS;
		if (cfg.new_console)
			creationFlags |= CREATE_NEW_CONSOLE;
		if (cfg.suspended)
			creationFlags |= CREATE_SUSPENDED;

		STARTUPINFOW si{};
		si.cb = sizeof(si);

		PROCESS_INFORMATION pi{};
		const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr,
		                                 nullptr, cfg.inherit_handles ? TRUE : FALSE, creationFlags, nullptr, wdir.empty() ? nullptr : wdir.c_str(), &si, &pi);

            if (!ok)
            {
                throw ProcessError("CreateProcessW failed: " + win::last_error_string());
            }

		m_hProcess = pi.hProcess;
		m_hThread = pi.hThread;
		m_pid = static_cast<std::uint32_t>(pi.dwProcessId);
		m_tid = static_cast<std::uint32_t>(pi.dwThreadId);

		m_launched = true;
		m_running = true;
	}

	std::optional<std::uint32_t> WindowsProcessLauncher::run_debug_loop(IDebugEventSink& sink)
	{
		if (!m_launched)
		{
			throw ProcessError("run_debug_loop called before launch().");
		}

		DEBUG_EVENT de{};
		std::optional<std::uint32_t> exitCode;

		while (!m_requestStop)
		{
			#ifdef GWATCH_PROFILE
			const auto wait_start = std::chrono::high_resolution_clock::now();
			#endif
            if (!WaitForDebugEvent(&de, kWaitMs))
            {
                throw ProcessError("WaitForDebugEvent failed: " + win::last_error_string());
            }
			#ifdef GWATCH_PROFILE
			const auto wait_end = std::chrono::high_resolution_clock::now();
			profiling::add_loop_wait_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(wait_end - wait_start).count());
			#endif

			DebugEvent ev{};
			ev.process_id = de.dwProcessId;
			ev.thread_id = de.dwThreadId;

			auto sinkDecision = ContinueStatus::Default;
			#ifdef GWATCH_PROFILE
			const auto handle_start = std::chrono::high_resolution_clock::now();
			#endif

			switch (de.dwDebugEventCode)
			{
			case CREATE_PROCESS_DEBUG_EVENT:
				{
					ev.type = DebugEventType::_CreateProcess;
					const auto& info = de.u.CreateProcessInfo;
					CreateProcessInfo cp{};
					cp.image_base = reinterpret_cast<std::uint64_t>(info.lpBaseOfImage);
					cp.entry_point = reinterpret_cast<std::uint64_t>(info.lpStartAddress);
					cp.image_path.clear();
					ev.payload = cp;

					if (info.hFile)
						CloseHandle(info.hFile);

					sinkDecision = sink.on_event(ev);
					break;
				}
			case EXIT_PROCESS_DEBUG_EVENT:
				{
					ev.type = DebugEventType::ExitProcess;
					ExitProcessInfo xp{};
					xp.exit_code = de.u.ExitProcess.dwExitCode;
					ev.payload = xp;
					sinkDecision = sink.on_event(ev);

					exitCode = xp.exit_code;
					m_running = false;
					break;
				}
			case CREATE_THREAD_DEBUG_EVENT:
				{
					ev.type = DebugEventType::CreateThread;
					CreateThreadInfo ct{};
					ct.start_address = reinterpret_cast<std::uint64_t>(de.u.CreateThread.lpStartAddress);
					ev.payload = ct;
					sinkDecision = sink.on_event(ev);
					break;
				}
			case EXIT_THREAD_DEBUG_EVENT:
				{
					ev.type = DebugEventType::ExitThread;
					ExitThreadInfo xt{};
					xt.exit_code = de.u.ExitThread.dwExitCode;
					ev.payload = xt;
					sinkDecision = sink.on_event(ev);
					break;
				}
			case EXCEPTION_DEBUG_EVENT:
				{
					ev.type = DebugEventType::Exception;
					const auto& [ExceptionRecord, dwFirstChance] = de.u.Exception;
					ExceptionInfo xi{};
					xi.code = ExceptionRecord.ExceptionCode;
					xi.address = reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress);
					xi.first_chance = (dwFirstChance != 0);
					ev.payload = xi;
					sinkDecision = sink.on_event(ev);
					break;
				}
            case LOAD_DLL_DEBUG_EVENT:
                {
                    ev.type = DebugEventType::LoadDll;
                    ev.payload = LoadDllInfo{};
                    // Close file handle if provided to avoid leaks
                    if (de.u.LoadDll.hFile)
                        CloseHandle(de.u.LoadDll.hFile);
                    sinkDecision = ContinueStatus::Default;
                    break;
                }
            case UNLOAD_DLL_DEBUG_EVENT:
                {
                    ev.type = DebugEventType::UnloadDll;
                    ev.payload = UnloadDllInfo{};
                    sinkDecision = ContinueStatus::Default;
                    break;
                }
            case OUTPUT_DEBUG_STRING_EVENT:
                {
                    ev.type = DebugEventType::_OutputDebugString;
                    ev.payload = OutputDebugStringInfo{};
                    sinkDecision = ContinueStatus::Default;
                    break;
                }
			case RIP_EVENT:
				{
					ev.type = DebugEventType::Rip;
					RipInfo ri{};
					ri.error = de.u.RipInfo.dwError;
					ri.type = de.u.RipInfo.dwType;
					ev.payload = ri;
					sinkDecision = sink.on_event(ev);
					break;
				}
			default:
				{
					// Unknown event: notify sink with a RIP-like container
					ev.type = DebugEventType::Rip;
					RipInfo ri{};
					ri.error = 0;
					ri.type = de.dwDebugEventCode;
					ev.payload = ri;
					sinkDecision = sink.on_event(ev);
					break;
				}
			}

			const DWORD cont = map_continue_code(sinkDecision, ev);
			ContinueDebugEvent(de.dwProcessId, de.dwThreadId, cont);

			#ifdef GWATCH_PROFILE
			const auto handle_end = std::chrono::high_resolution_clock::now();
			profiling::add_loop_handle_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(handle_end - handle_start).count());
			profiling::inc_loop_iteration();
			#endif

			if (de.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
			{
				break;
			}
		}

		return exitCode;
	}

	void WindowsProcessLauncher::stop()
	{
		m_requestStop = true;
	}

	void* WindowsProcessLauncher::native_process_handle() const
	{
		return m_hProcess;
	}

	std::wstring WindowsProcessLauncher::to_wstring(std::string_view s)
	{
		if (s.empty())
			return {};
		const int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
		if (needed <= 0)
			return {};
		std::wstring w(static_cast<std::size_t>(needed), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), needed);
		return w;
	}

	std::string WindowsProcessLauncher::utf8_from_wstring(std::wstring_view ws)
	{
		if (ws.empty())
			return {};

		if (ws.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			throw ProcessError("utf8_from_wstring: input too large for WideCharToMultiByte.");
		}

		const int srcLen = static_cast<int>(ws.size());
		const int need = WideCharToMultiByte(
			CP_UTF8, 0,
			ws.data(), srcLen,
			nullptr, 0,
			nullptr, nullptr
		);
        if (need <= 0)
        {
            throw ProcessError("WideCharToMultiByte(size) failed: " + win::last_error_string());
        }

		std::string out(static_cast<size_t>(need), '\0');
		const int written = WideCharToMultiByte(
			CP_UTF8, 0,
			ws.data(), srcLen,
			out.data(), need,
			nullptr, nullptr
		);
        if (written <= 0)
        {
            throw ProcessError("WideCharToMultiByte(copy) failed: " + win::last_error_string());
        }
		return out;
	}

	std::wstring WindowsProcessLauncher::build_command_line(const LaunchConfig& cfg)
	{
		const std::wstring exeW = to_wstring(cfg.exe_path);
		std::wstring cmd = quote_arg(exeW);
		for (const auto& a : cfg.args)
		{
			cmd.push_back(L' ');
			cmd += quote_arg(to_wstring(a));
		}
		return cmd;
	}

	std::wstring WindowsProcessLauncher::quote_arg(std::wstring_view arg)
	{
		// Quote if empty or contains spaces/tabs or quotes.
		const bool needQuotes = arg.empty()
			|| std::ranges::any_of(arg, [](const wchar_t c) { return c == L' ' || c == L'\t' || c == L'\"'; });

		if (!needQuotes)
			return std::wstring(arg);

		std::wstring out;
		out.reserve(arg.size() + 8);
		out.push_back(L'"');
		std::size_t bsCount = 0;
		for (const wchar_t ch : arg)
		{
			if (ch == L'\\')
			{
				++bsCount;
			}
			else if (ch == L'"')
			{
				// Escape all backslashes, then escape the quote
				out.append(bsCount * 2, L'\\');
				bsCount = 0;
				out.append(L"\\\"");
			}
			else
			{
				out.append(bsCount, L'\\');
				bsCount = 0;
				out.push_back(ch);
			}
		}
		// Escape trailing backslashes
		out.append(bsCount * 2, L'\\');
		out.push_back(L'"');
		return out;
	}

	std::uint32_t WindowsProcessLauncher::map_continue_code(const ContinueStatus sinkDecision, const DebugEvent& ev)
	{
		if (sinkDecision == ContinueStatus::Continue)
			return DBG_CONTINUE;
		if (sinkDecision == ContinueStatus::NotHandled)
			return DBG_EXCEPTION_NOT_HANDLED;

		// Default policy:
		// - Swallow breakpoints & single-step
		// - Everything else: not handled
		if (ev.type == DebugEventType::Exception)
		{
			switch (const auto& ex = std::get<ExceptionInfo>(ev.payload); ex.code)
			{
			case EXCEPTION_BREAKPOINT:
			case EXCEPTION_SINGLE_STEP:
				return DBG_CONTINUE;
			default:
				return DBG_EXCEPTION_NOT_HANDLED;
			}
		}

		return DBG_CONTINUE;
	}
}

#endif
