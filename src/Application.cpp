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
		m_hProc = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, m_processLauncher->pid());
		if (!m_hProc)
		{
			throw std::runtime_error("Can't open process!");
		}
		m_resolver = std::make_unique<WindowsSymbolResolver>(m_hProc, "", true);
		m_symbol = m_resolver->resolve(m_args.symbol);
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
