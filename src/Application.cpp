#include "../include/Application.h"

#include <string>
#include <stdexcept>
#include <variant>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#endif
#ifdef GWATCH_PROFILE
#include <chrono>
#include "../include/Profiling.h"
#endif
#include <iostream>

#include "../include/WinUtil.h"


namespace
{
#ifdef _WIN32
	std::wstring utf16_from_utf8(const std::string& s)
	{
		if (s.empty())
			return {};
		const int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
		if (needed <= 0)
		{
			throw std::runtime_error("MultiByteToWideChar(size) failed: " + gwatch::win::last_error_string());
		}
		std::wstring out(static_cast<std::size_t>(needed), L'\0');
		const int written = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), needed);
		if (written <= 0)
		{
			throw std::runtime_error("MultiByteToWideChar(copy) failed: " + gwatch::win::last_error_string());
		}
		return out;
	}
#endif
}

namespace gwatch
{
	class Application::DebugLoopSink final : public IDebugEventSink
	{
	public:
		explicit DebugLoopSink(Application& app) : m_app(app) {}

		ContinueStatus on_event(const DebugEvent& ev) override
		{
#ifdef _WIN32
			if (!m_app.m_memoryWatcher)
			{
				if (ev.type == DebugEventType::_CreateProcess)
				{
					const auto& cp = std::get<CreateProcessInfo>(ev.payload);
					m_app.resolve_symbol(cp);
					m_app.setup_memory_watcher();
					return m_app.m_memoryWatcher->on_event(ev);
				}
				return ContinueStatus::Default;
			}
			return m_app.m_memoryWatcher->on_event(ev);
#else
			(void)ev;
			return ContinueStatus::Default;
#endif
		}

	private:
		Application& m_app;
	};

	Application::Application(const CliArgs& args) :
		m_args(args),
		m_hProc(nullptr)
	{
	}

	Application::~Application()
	{
		if (m_hProc)
		{
			CloseHandle(m_hProc);
		}
	}

	int Application::execute()
	{
		try
		{
			start_process();
			DebugLoopSink sink(*this);
			const std::optional<std::uint32_t> exitCode = m_processLauncher->run_debug_loop(sink);
			return exitCode.value_or(0);
		}
		catch (const SymbolError& e)
		{
			std::cerr << e.what() << "\n";
			return 1;
		}
		catch (const ProcessError& e)
		{
			std::cerr << e.what() << "\n";
			return 1;
		}
		catch (const MemoryWatchError& e)
		{
			std::cerr << e.what() << "\n";
			return 1;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Unexpected error: " << e.what() << "\n";
			return 1;
		}
	}

	void Application::start_process()
	{
#ifdef _WIN32
		m_processLauncher = std::make_unique<WindowsProcessLauncher>();
#endif
		const LaunchConfig cfg{
			.exe_path = m_args.execPath,
			.args = m_args.targetArgs,
			.new_console = false,
			.suspended = false,
			.debug_children = false,
		};
		#ifdef GWATCH_PROFILE
		const auto launch_start = std::chrono::high_resolution_clock::now();
		#endif
		m_processLauncher->launch(cfg);
		#ifdef GWATCH_PROFILE
		const auto launch_end = std::chrono::high_resolution_clock::now();
		profiling::add_process_launch_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(launch_end - launch_start).count());
		#endif
	}

	void Application::resolve_symbol(const CreateProcessInfo& cpInfo)
	{
		if (!m_processLauncher)
			throw std::runtime_error("You must attach WindowsProcessLauncher before resolving!");
#ifdef GWATCH_PROFILE
		const auto resolve_start = std::chrono::high_resolution_clock::now();
#endif
#ifdef _WIN32
		const auto* w = dynamic_cast<WindowsProcessLauncher*>(m_processLauncher.get());
		if (!w) throw std::runtime_error("WindowsProcessLauncher expected");

		if (m_hProc)
		{
			CloseHandle(m_hProc);
			m_hProc = nullptr;
		}

		const HANDLE opened = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, w->pid());
		if (!opened)
		{
			throw std::runtime_error("OpenProcess failed: " + win::last_error_string());
		}
		m_hProc = opened;

		WindowsSymbolResolver::ModuleLoadHint hint;
		hint.image_base = cpInfo.image_base;
		hint.image_size = 0;
		const std::string imagePath = !cpInfo.image_path.empty() ? cpInfo.image_path : m_args.execPath;
		hint.image_path = utf16_from_utf8(imagePath);

		const std::unique_ptr<ISymbolResolver> resolver =
			std::make_unique<WindowsSymbolResolver>(m_hProc, "", false, &hint);
		try
		{
			m_symbol = resolver->resolve(m_args.symbol);
		}
		catch (const SymbolError& inner)
		{
			std::ostringstream oss;
			oss << "Failed to resolve symbol '" << m_args.symbol << "' in target '" << imagePath << "'.\n"
				<< "Details: " << inner.what() << "\n"
				<< "Hint: verify the global variable name, that symbols/PDB are available, and that it is a 4–8 byte integer.";
			throw SymbolError(oss.str());
		}
#endif
#ifdef GWATCH_PROFILE
		const auto resolve_end = std::chrono::high_resolution_clock::now();
		profiling::add_symbol_resolve_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(resolve_end - resolve_start).count());
#endif
	}

	void Application::setup_memory_watcher()
	{
		if (m_memoryWatcher)
			return;
		if (!m_hProc || !m_symbol.has_value())
		{
			throw std::runtime_error("You must attach the process and resolve the symbol before setting up the watcher!");
		}

#ifdef _WIN32
		#ifdef GWATCH_PROFILE
		const auto setup_start = std::chrono::high_resolution_clock::now();
		#endif
		m_memoryWatcher = std::make_unique<WindowsMemoryWatcher>(m_hProc, *m_symbol);
		#ifdef GWATCH_PROFILE
		const auto setup_end = std::chrono::high_resolution_clock::now();
		profiling::add_setup_watcher_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(setup_end - setup_start).count());
		#endif
#endif
	}
}
