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
	bool saw_any_load_dll = false;
	bool saw_output_debug_a = false;
	bool saw_output_debug_w = false;
	std::vector<std::string> debug_msgs;
	std::optional<std::uint32_t> exit_code;

	gwatch::ContinueStatus on_event(const gwatch::DebugEvent& ev) override
	{
		using T = gwatch::DebugEventType;
		switch (ev.type)
		{
			case T::_CreateProcess:
				{
					saw_create_process = true;
					break;
				}
			case T::LoadDll:
				{
					// We expect at least one DLL to load.
					if (const auto& [base, path] = std::get<gwatch::LoadDllInfo>(ev.payload); !path.empty() || base != 0)
					{
						saw_any_load_dll = true;
					}
					break;
				}
			case T::_OutputDebugString:
				{
					const auto& [message] = std::get<gwatch::OutputDebugStringInfo>(ev.payload);
					debug_msgs.push_back(message);
					if (message.find("DBG:hello_ascii") != std::string::npos)
					{
						saw_output_debug_a = true;
					}
					if (message.find("DBG:hello_utf16") != std::string::npos)
					{
						saw_output_debug_w = true;
					}
					break;
				}
			case T::Exception:
				{
					// Default policy already continues breakpoints/single-step.
					// We keep Default to not interfere unless necessary.
					break;
				}
			case T::ExitProcess:
				{
					const auto& [exit_code] = std::get<gwatch::ExitProcessInfo>(ev.payload);
					this->exit_code = exit_code;
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
	EXPECT_TRUE(sink.saw_any_load_dll) << "At least one LOAD_DLL event expected.";
	EXPECT_TRUE(sink.saw_output_debug_a) << "ASCII OutputDebugString should be captured.";
	EXPECT_TRUE(sink.saw_output_debug_w) << "Unicode OutputDebugString should be captured.";

	EXPECT_GE(sink.debug_msgs.size(), 2u);
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
