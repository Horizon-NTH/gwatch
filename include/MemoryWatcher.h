#pragma once
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <stdexcept>

#include "ProcessLauncher.h"
#include "SymbolResolver.h"

namespace gwatch
{
	class MemoryWatchError final : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	class IMemoryWatcher : public IDebugEventSink
	{
	public:
		IMemoryWatcher() = default;
		~IMemoryWatcher() override = default;
	};

#ifdef _WIN32

	// Windows implementation using per-thread hardware data breakpoints (DR0).
	// On each access (read or write), a SINGLE_STEP exception is delivered.
	// We read the current value in the target and compare with the previous;
	// if changed => write "<old> -> <new>", otherwise => read "<val>".
	class WindowsMemoryWatcher final : public IMemoryWatcher
	{
	public:
		WindowsMemoryWatcher(void* hProcess, const ResolvedSymbol& resolvedSymbol, bool enableHardwareBreakpoints = true);

		~WindowsMemoryWatcher() override = default;

		ContinueStatus on_event(const DebugEvent& ev) override;

	private:
		void* m_hProcess{};
		ResolvedSymbol m_resolvedSymbol{};
		bool m_enableHardwareBreakpoints{true};

		std::optional<std::uint64_t> m_lastValue{};
		std::unordered_set<std::uint32_t> m_armedThreads;

		static std::uint64_t mask_for_size(std::uint32_t size);
		static std::uint64_t len_encoding_for_size(std::uint32_t size);

		void install_on_thread(std::uint32_t tid);
		std::uint64_t read_value() const;
		ContinueStatus handle_single_step(std::uint32_t tid);
	};

#endif
}
