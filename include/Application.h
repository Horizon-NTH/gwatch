#pragma once
#include <memory>

#include "ArgumentsParser.h"
#include "MemoryWatcher.h"
#include "ProcessLauncher.h"
#include "SymbolResolver.h"

namespace gwatch
{
	class Application
	{
	public:
		explicit Application(const CliArgs& args);
		Application(const Application&) = delete;
		Application(Application&&) = delete;

		~Application();

		int execute();

	private:
		CliArgs m_args;
		std::unique_ptr<IProcessLauncher> m_processLauncher;
		std::unique_ptr<IMemoryWatcher> m_memoryWatcher;
		std::unique_ptr<ISymbolResolver> m_resolver;
		std::optional<ResolvedSymbol> m_symbol;
		void* m_hProc;

		void start_process();
		void resolve_symbol();
		void setup_memory_watcher();
	};
}
