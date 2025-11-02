#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <filesystem>
#include <string>
#include "Application.h"

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
	// Locate the folder where the test binary runs because we copied the debuggee next to it.
	std::filesystem::path CurrentBinDir()
	{
		wchar_t buf[MAX_PATH]{0};
		if (const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH); n == 0 || n >= MAX_PATH)
			return {};
		std::filesystem::path p(buf);
		return p.parent_path();
	}
}

TEST(ApplicationTest, Execute_HappyPath_ReturnsExitCode_And_ProducesLogs)
{
	const auto exe = (CurrentBinDir() / "gwatch_debuggee_app.exe");
	ASSERT_TRUE(std::filesystem::exists(exe)) << "Debuggee not found at: " << exe.string();

	testing::internal::CaptureStdout();

	CliArgs args;
	args.symbol = "g_counter";
	args.execPath = exe.string();
	args.targetArgs.clear();

	Application app(args);
    const int rc = app.execute();

    EXPECT_EQ(rc, 123);
    const std::string out = testing::internal::GetCapturedStdout();

    // The OS may deliver multiple SINGLE_STEP exceptions for reads, causing
    // duplicate "read" lines. Be tolerant by validating the sequence of writes
    // (which is stable) and that we observed an initial read of 0.
    std::istringstream iss(out);
    std::string line;
    std::vector<std::string> writes;
    bool saw_initial_read0 = false;
    while (std::getline(iss, line))
    {
        if (line == "g_counter read 0")
            saw_initial_read0 = true;
        if (line.rfind("g_counter write ", 0) == 0)
            writes.push_back(line);
    }

    EXPECT_TRUE(saw_initial_read0);
    const std::vector<std::string> expected_writes = {
        "g_counter write 0 -> 1",
        "g_counter write 1 -> 2",
        "g_counter write 2 -> 3",
        "g_counter write 3 -> 4",
    };
    EXPECT_EQ(writes, expected_writes);
}

TEST(ApplicationTest, Execute_MissingExecutable_Returns1)
{
	CliArgs args;
	args.symbol = "g_counter";
	args.execPath = R"(C:\definitely\not\there\nope.exe)";
	args.targetArgs.clear();

	Application app(args);
	testing::internal::CaptureStderr();
	const int rc = app.execute();
	testing::internal::GetCapturedStderr();
	EXPECT_EQ(rc, 1);
}

TEST(ApplicationTest, Execute_BadSymbol_Returns1)
{
	const auto exe = (CurrentBinDir() / "gwatch_debuggee_app.exe");
	ASSERT_TRUE(std::filesystem::exists(exe)) << "Debuggee not found at: " << exe.string();

	CliArgs args;
	args.symbol = "ThisSymbolDoesNotExist_12345";
	args.execPath = exe.string();

	Application app(args);
	testing::internal::CaptureStderr();
	const int rc = app.execute();
	testing::internal::GetCapturedStderr();
	EXPECT_EQ(rc, 1);
}

#else

TEST(ApplicationPortable, SkippedOnNonWindows)
{
	GTEST_SKIP() << "Application integration tests are Windows-only.";
}

#endif
