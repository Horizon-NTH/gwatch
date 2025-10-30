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

// RAII to redirect std::cout to a buffer (to capture MemoryWatcher/Logger output).
struct CoutRedirect
{
	std::streambuf* old = nullptr;
	std::ostringstream oss;
	CoutRedirect() { old = std::cout.rdbuf(oss.rdbuf()); }
	~CoutRedirect() { std::cout.rdbuf(old); }
};

TEST(ApplicationTest, Execute_HappyPath_ReturnsExitCode_And_ProducesLogs)
{
	const auto exe = (CurrentBinDir() / "gwatch_debuggee_app.exe");
	ASSERT_TRUE(std::filesystem::exists(exe)) << "Debuggee not found at: " << exe.string();

	CoutRedirect cap;

	CliArgs args;
	args.symbol = "g_counter";
	args.execPath = exe.string();
	args.targetArgs.clear();

	Application app(args);
	const int rc = app.execute();

	EXPECT_EQ(rc, 123);
	const std::string out = cap.oss.str();
	EXPECT_NE(out.find("g_counter"), std::string::npos) << "No logs for g_counter were captured.";
	EXPECT_NE(out.find(" read "), std::string::npos);
	EXPECT_NE(out.find(" write "), std::string::npos);
}

TEST(ApplicationTest, Execute_MissingExecutable_Returns1)
{
	CliArgs args;
	args.symbol = "g_counter";
	args.execPath = R"(C:\definitely\not\there\nope.exe)";
	args.targetArgs.clear();

	Application app(args);
	const int rc = app.execute();
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
	const int rc = app.execute();
	EXPECT_EQ(rc, 1);
}

#else

TEST(ApplicationPortable, SkippedOnNonWindows)
{
	GTEST_SKIP() << "Application integration tests are Windows-only.";
}

#endif
