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
#include <iomanip>
#include <algorithm>
#include <cstdint>

#include "ProcessLauncher.h"

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
			::CloseHandle(m_hThread);
			m_hThread = nullptr;
		}
		if (m_hProcess)
		{
			::CloseHandle(m_hProcess);
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
		const BOOL ok = ::CreateProcessW(nullptr, cmd.data(), nullptr,
			nullptr, cfg.inherit_handles ? TRUE : FALSE, creationFlags, nullptr, wdir.empty() ? nullptr : wdir.c_str(), &si, &pi);

		if (!ok)
		{
			throw ProcessError("CreateProcessW failed: " + last_error_string());
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
			if (!::WaitForDebugEvent(&de, kWaitMs))
			{
				throw ProcessError("WaitForDebugEvent failed: " + last_error_string());
			}

			DebugEvent ev{};
			ev.process_id = de.dwProcessId;
			ev.thread_id = de.dwThreadId;

			auto sinkDecision = ContinueStatus::Default;

			switch (de.dwDebugEventCode)
			{
				case CREATE_PROCESS_DEBUG_EVENT:
					{
						ev.type = DebugEventType::CreateProcess;
						const auto& info = de.u.CreateProcessInfo;
						CreateProcessInfo cp{};
						cp.image_base = reinterpret_cast<std::uint64_t>(info.lpBaseOfImage);
						cp.entry_point = reinterpret_cast<std::uint64_t>(info.lpStartAddress);
						cp.image_path = resolve_module_path(info.hFile, m_hProcess, info.lpImageName, info.fUnicode);
						ev.payload = cp;

						if (info.hFile)
							::CloseHandle(info.hFile);

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
						const auto& ld = de.u.LoadDll;
						LoadDllInfo li{};
						li.base = reinterpret_cast<std::uint64_t>(ld.lpBaseOfDll);
						li.path = resolve_module_path(ld.hFile, m_hProcess, ld.lpImageName, ld.fUnicode);
						ev.payload = li;

						if (ld.hFile)
							::CloseHandle(ld.hFile);
						sinkDecision = sink.on_event(ev);
						break;
					}
				case UNLOAD_DLL_DEBUG_EVENT:
					{
						ev.type = DebugEventType::UnloadDll;
						UnloadDllInfo ui{};
						ui.base = reinterpret_cast<std::uint64_t>(de.u.UnloadDll.lpBaseOfDll);
						ev.payload = ui;
						sinkDecision = sink.on_event(ev);
						break;
					}
				case OUTPUT_DEBUG_STRING_EVENT:
					{
						ev.type = DebugEventType::OutputDebugString;
						const auto& [lpDebugStringData, fUnicode, nDebugStringLength] = de.u.DebugString;
						OutputDebugStringInfo oi{};
						oi.message = read_remote_string(m_hProcess, lpDebugStringData, fUnicode != 0, nDebugStringLength);
						ev.payload = oi;
						sinkDecision = sink.on_event(ev);
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
			::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, cont);

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

	std::wstring WindowsProcessLauncher::to_wstring(std::string_view s)
	{
		if (s.empty())
			return {};
		const int needed = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
		if (needed <= 0)
			return {};
		std::wstring w(static_cast<std::size_t>(needed), L'\0');
		::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), needed);
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
		const int need = ::WideCharToMultiByte(
			CP_UTF8, 0,
			ws.data(), srcLen,
			nullptr, 0,
			nullptr, nullptr
		);
		if (need <= 0)
		{
			throw ProcessError("WideCharToMultiByte(size) failed: " + last_error_string());
		}

		std::string out(static_cast<size_t>(need), '\0');
		const int written = ::WideCharToMultiByte(
			CP_UTF8, 0,
			ws.data(), srcLen,
			out.data(), need,
			nullptr, nullptr
		);
		if (written <= 0)
		{
			throw ProcessError("WideCharToMultiByte(copy) failed: " + last_error_string());
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

	std::string WindowsProcessLauncher::last_error_string()
	{
		const DWORD err = ::GetLastError();
		if (err == 0)
			return "OK";
		LPSTR buf = nullptr;
		const DWORD n = ::FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&buf), 0, nullptr);
		std::string s = (n && buf) ? std::string(buf, buf + n) : "code=" + std::to_string(err);
		if (buf)
			::LocalFree(buf);
		return s;
	}

	std::string WindowsProcessLauncher::resolve_module_path(const HANDLE hFile, const HANDLE hProcess, void* remoteImageName, const std::uint16_t isUnicode, const std::size_t maxBytes)
	{
		// If we have a file handle, we try GetFinalPathNameByHandle.
		if (hFile)
		{
			std::wstring ws(32768, L'\0');
			const DWORD n = ::GetFinalPathNameByHandleW(hFile, ws.data(), static_cast<DWORD>(ws.size()),
				FILE_NAME_NORMALIZED);
			if (n > 0 && n < ws.size())
			{
				ws.resize(n);
				// Strip the leading "\\?\" if present.
				if (constexpr std::wstring_view prefix = L"\\\\?\\"; ws.rfind(prefix, 0) == 0)
				{
					ws.erase(0, prefix.size());
				}
				return utf8_from_wstring(ws);
			}
		}

		// Fallback: if the debug record exposes an image name pointer in the target, we read it.
		if (remoteImageName)
		{
			return read_remote_string(hProcess, remoteImageName, isUnicode != 0, maxBytes);
		}

		return {};
	}

	std::string WindowsProcessLauncher::read_remote_string(const HANDLE hProcess, const void* remote, const bool isUnicode, const std::size_t maxBytes)
	{
		if (!remote)
			return {};
		std::vector<std::byte> buf(maxBytes + (isUnicode ? 2 : 1));
		SIZE_T read = 0;
		if (!::ReadProcessMemory(hProcess, remote, buf.data(), maxBytes, &read) || read == 0)
		{
			return {};
		}
		if (isUnicode)
		{
			const auto w = reinterpret_cast<const wchar_t*>(buf.data());
			std::size_t wlen = read / sizeof(wchar_t);
			// Ensure null-termination
			return utf8_from_wstring(std::wstring(w, std::find(w, w + wlen, L'\0')));
		}
		const auto s = reinterpret_cast<const char*>(buf.data());
		const std::size_t n = std::find(s, s + read, '\0') - s;
		return std::string(s, s + n);
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
