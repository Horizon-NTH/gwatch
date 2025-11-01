#include <gtest/gtest.h>
#include <limits>
#include <string>

#include "Logger.h"

using gwatch::Logger;

TEST(LoggerTest, LogRead_ExactFormat)
{
	testing::internal::CaptureStdout();
	Logger::log_read("myVar", 42);
	const std::string out = testing::internal::GetCapturedStdout();
	EXPECT_EQ(out, std::string("myVar read 42\n"));
}

TEST(LoggerTest, LogWrite_ExactFormat)
{
	testing::internal::CaptureStdout();
	Logger::log_write("counter", 50, 100);
	const std::string out = testing::internal::GetCapturedStdout();
	EXPECT_EQ(out, std::string("counter write 50 -> 100\n"));
}

TEST(LoggerTest, NoLeadingZeros)
{
	testing::internal::CaptureStdout();
	Logger::log_read("x", 0);
	Logger::log_read("x", 7);
	Logger::log_write("x", 7, 13);
	const std::string out = testing::internal::GetCapturedStdout();
	const std::string expected =
		"x read 0\n"
		"x read 7\n"
		"x write 7 -> 13\n";
	EXPECT_EQ(out, expected);
}

TEST(LoggerTest, SupportsUint64Max)
{
	testing::internal::CaptureStdout();
	constexpr auto maxv = std::numeric_limits<std::uint64_t>::max();
	Logger::log_read("big", maxv);
	const std::string out = testing::internal::GetCapturedStdout();

	std::ostringstream check;
	check << "big read " << std::dec << maxv << "\n";
	EXPECT_EQ(out, check.str());
}

TEST(LoggerTest, MultipleCallsAppendWithNewlines)
{
	testing::internal::CaptureStdout();
	Logger::log_read("a", 1);
	Logger::log_write("a", 1, 2);
	Logger::log_read("a", 2);
	const std::string out = testing::internal::GetCapturedStdout();
	const std::string expected =
		"a read 1\n"
		"a write 1 -> 2\n"
		"a read 2\n";
	EXPECT_EQ(out, expected);
}
