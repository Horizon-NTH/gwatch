#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <limits>

#include "MemoryWatcher.h"
#include "ProcessLauncher.h"
#include "Logger.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

using namespace gwatch;

#ifdef _WIN32

namespace
{
	// Build a DebugEvent representing a SINGLE_STEP exception on the current thread.
	DebugEvent SingleStepEvent()
	{
		DebugEvent ev{};
		ev.type = DebugEventType::Exception;
		ev.process_id = ::GetCurrentProcessId();
		ev.thread_id = ::GetCurrentThreadId();

		ExceptionInfo info{};
		info.code = EXCEPTION_SINGLE_STEP;
		info.address = 0;
		info.first_chance = true;

		ev.payload = info;
		return ev;
	}

	// Build a DebugEvent representing CREATE_PROCESS to prime baseline.
	DebugEvent CreateProcessEvt()
	{
		DebugEvent ev{};
		ev.type = DebugEventType::_CreateProcess;
		ev.process_id = ::GetCurrentProcessId();
		ev.thread_id = ::GetCurrentThreadId();

		CreateProcessInfo cp{};
		cp.image_base = 0;
		cp.entry_point = 0;
		cp.image_path = "";
		ev.payload = cp;
		return ev;
	}

	ResolvedSymbol create_resolve_symbol(const std::uint64_t address, const std::uint64_t size, const std::string& name)
	{
		return ResolvedSymbol{
				.name = name,
				.address = address,
				.size = size,
			};
	}

	// Global storage to watch via ReadProcessMemory (stable address).
	alignas(8) volatile std::uint64_t g_watch64 = 0;
	alignas(4) volatile std::uint32_t g_watch32 = 0;
}

TEST(WindowsMemoryWatcherTest, ClassifiesReadAndWrite64)
{
	std::ostringstream oss;
	Logger logger(oss);

	g_watch64 = 0;
	WindowsMemoryWatcher mw(::GetCurrentProcess(), create_resolve_symbol(reinterpret_cast<std::uint64_t>(&g_watch64), 8, "sym64"), logger);

	// First SINGLE_STEP -> no previous value -> read
	auto ev = SingleStepEvent();
	auto st = mw.on_event(ev);
	EXPECT_EQ(st, ContinueStatus::Default);
	EXPECT_EQ(oss.str(), "sym64 read 0\n");

	// Change value -> write
	g_watch64 = 5;
	st = mw.on_event(SingleStepEvent());
	EXPECT_EQ(st, ContinueStatus::Default);

	// Same value -> read
	st = mw.on_event(SingleStepEvent());
	EXPECT_EQ(st, ContinueStatus::Default);

	const std::string expected =
		"sym64 read 0\n"
		"sym64 write 0 -> 5\n"
		"sym64 read 5\n";
	EXPECT_EQ(oss.str(), expected);
}

TEST(WindowsMemoryWatcherTest, ClassifiesReadAndWrite32)
{
	std::ostringstream oss;
	Logger logger(oss);

	g_watch32 = 10;
	WindowsMemoryWatcher mw(::GetCurrentProcess(), create_resolve_symbol(reinterpret_cast<std::uint64_t>(&g_watch32), 4, "sym32"), logger);

	// First -> read
	auto st = mw.on_event(SingleStepEvent());
	EXPECT_EQ(st, ContinueStatus::Default);

	// Change -> write
	g_watch32 = 11;
	st = mw.on_event(SingleStepEvent());
	EXPECT_EQ(st, ContinueStatus::Default);

	// No change -> read
	st = mw.on_event(SingleStepEvent());
	EXPECT_EQ(st, ContinueStatus::Default);

	const std::string expected =
		"sym32 read 10\n"
		"sym32 write 10 -> 11\n"
		"sym32 read 11\n";
	EXPECT_EQ(oss.str(), expected);
}

TEST(WindowsMemoryWatcherTest, CreateProcessPrimesBaseline_NoInitialReadLine)
{
	std::ostringstream oss;
	const Logger logger(oss);

	g_watch64 = 42;
	WindowsMemoryWatcher mw(::GetCurrentProcess(), create_resolve_symbol(reinterpret_cast<std::uint64_t>(&g_watch64), 8, "sym"), logger);

	// Simulate CREATE_PROCESS: watcher will try to arm thread (ignored on failure)
	// and attempt to read baseline value (42) without logging.
	mw.on_event(CreateProcessEvt());

	// Now change and trigger SINGLE_STEP -> write
	g_watch64 = 43;
	mw.on_event(SingleStepEvent());

	EXPECT_EQ(oss.str(), "sym write 42 -> 43\n");
}

TEST(WindowsMemoryWatcherTest, InvalidAddressReturnsNotHandledAndNoLog)
{
	std::ostringstream oss;
	const Logger logger(oss);

	// Intentionally invalid (null) address: ReadProcessMemory will fail.
	WindowsMemoryWatcher mw(::GetCurrentProcess(), create_resolve_symbol(0ull, 8, "bad"), logger);

	const auto st = mw.on_event(SingleStepEvent());
	EXPECT_EQ(st, ContinueStatus::NotHandled);
	EXPECT_TRUE(oss.str().empty());
}

TEST(WindowsMemoryWatcherTest, RejectsUnsupportedSize)
{
	std::ostringstream oss;
	const Logger logger(oss);

	EXPECT_THROW(
		WindowsMemoryWatcher(::GetCurrentProcess(), create_resolve_symbol(reinterpret_cast<std::uint64_t>(&g_watch32), 2, "badSize"), logger),
		MemoryWatchError
	);
}

#else

TEST(MemoryWatcherPortable, SkippedOnNonWindows)
{
	GTEST_SKIP() << "WindowsMemoryWatcher tests require _WIN32.";
}

#endif
