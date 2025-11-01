#include <gtest/gtest.h>
#include <sstream>
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
	const std::string expected =
		"g_counter read 0\n"
		"g_counter write 0 -> 1\n"
		"g_counter read 1\n"
		"g_counter write 1 -> 2\n"
		"g_counter read 2\n"
		"g_counter write 2 -> 3\n"
		"g_counter read 3\n"
		"g_counter write 3 -> 4\n";
	EXPECT_EQ(out, expected);
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
