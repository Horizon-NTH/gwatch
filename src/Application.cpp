#include "../include/Application.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace gwatch
{
	Application::Application(const CliArgs& args) :
		m_args(args)
	{
	}

	Application::~Application()
	{
		if (m_hProc)
		{
			::CloseHandle(m_hProc);
		}
	}

	int Application::execute()
	{
		try
		{
			start_process();
			resolve_symbol();
			setup_memory_watcher();
			const std::optional<std::uint32_t> exitCode = m_processLauncher->run_debug_loop(*m_memoryWatcher);
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
		m_processLauncher->launch(cfg);
	}

	void Application::resolve_symbol()
	{
		if (!m_processLauncher)
			throw std::runtime_error("You must attach WindowsProcessLauncher before resolving!");
#ifdef _WIN32
		const auto* w = dynamic_cast<WindowsProcessLauncher*>(m_processLauncher.get());
		if (!w) throw std::runtime_error("WindowsProcessLauncher expected");

		const auto raw = w->native_process_handle();

		HANDLE dup{};
		if (!::DuplicateHandle(::GetCurrentProcess(), raw, ::GetCurrentProcess(),
		                       &dup, 0, FALSE, DUPLICATE_SAME_ACCESS))
		{
			throw std::runtime_error("DuplicateHandle failed");
		}
		m_hProc = dup;

		const std::unique_ptr<ISymbolResolver> resolver =
			std::make_unique<WindowsSymbolResolver>(m_hProc, "", false);
		m_symbol = resolver->resolve(m_args.symbol);
#endif
	}

	void Application::setup_memory_watcher()
	{
		if (!m_hProc || !m_symbol.has_value())
		{
			throw std::runtime_error("You must attach the process and resolve the symbol before setting up the watcher!");
		}
#ifdef _WIN32
		m_memoryWatcher = std::make_unique<WindowsMemoryWatcher>(m_hProc, *m_symbol, Logger(std::cout));
#endif
	}
}
