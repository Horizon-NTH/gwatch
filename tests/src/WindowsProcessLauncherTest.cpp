#include <gtest/gtest.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

#include "ProcessLauncher.h"

namespace
{
	std::filesystem::path CurrentModuleDir()
	{
		wchar_t buf[MAX_PATH]{0};
		if (const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH); n == 0 || n >= MAX_PATH)
			return {};
		std::filesystem::path p(buf);
		return p.parent_path();
	}
}

struct RecordingSink final : gwatch::IDebugEventSink
{
    bool saw_create_process = false;
    std::optional<std::uint32_t> exit_code;

    gwatch::ContinueStatus on_event(const gwatch::DebugEvent& ev) override
    {
        using T = gwatch::DebugEventType;
        switch (ev.type)
        {
            case T::_CreateProcess:
                saw_create_process = true;
                break;
            case T::ExitProcess:
            {
                const auto& [code] = std::get<gwatch::ExitProcessInfo>(ev.payload);
                exit_code = code;
                break;
            }
            default:
                break;
        }
        return gwatch::ContinueStatus::Default;
    }
};

TEST(WindowsProcessLauncherTest, LaunchesAndReceivesEvents)
{
	using namespace gwatch;

	const auto exe = (CurrentModuleDir() / "gwatch_debuggee_toy.exe");
	ASSERT_TRUE(std::filesystem::exists(exe)) << "Debuggee not found at: " << exe.string();

	WindowsProcessLauncher launcher;

	LaunchConfig cfg;
	cfg.exe_path = exe.string();
	cfg.args = {};
	cfg.new_console = false;
	cfg.debug_children = false;
	cfg.suspended = false;

	ASSERT_NO_THROW(launcher.launch(cfg));

	RecordingSink sink;
	const auto result = launcher.run_debug_loop(sink);

	ASSERT_TRUE(result.has_value()) << "Exit code should be available.";
	EXPECT_EQ(result.value(), 123u) << "Debuggee must return 123.";
    EXPECT_TRUE(sink.saw_create_process) << "CREATE_PROCESS event should have been observed.";
}

TEST(WindowsProcessLauncherTest, LaunchFailsForMissingExe)
{
	using namespace gwatch;

	WindowsProcessLauncher launcher;

	LaunchConfig cfg;
	cfg.exe_path = R"(C:\definitely\not\there\nope_debuggee.exe)";
	cfg.args = {};

	EXPECT_THROW(launcher.launch(cfg), ProcessError);
}

#else

TEST(ProcessLauncherPortable, SkippedOnNonWindows)
{
	GTEST_SKIP() << "Windows-only test suite.";
}

#endif
