#include <gtest/gtest.h>
#include <sstream>
#include <limits>
#include <string>

#include "Logger.h"

using gwatch::Logger;

TEST(LoggerTests, LogRead_ExactFormat)
{
	std::ostringstream oss;
	const Logger logger(oss);

	logger.log_read("myVar", 42);

	EXPECT_EQ(oss.str(), std::string("myVar read 42\n"));
}

TEST(LoggerTests, LogWrite_ExactFormat)
{
	std::ostringstream oss;
	const Logger logger(oss);

	logger.log_write("counter", 50, 100);

	EXPECT_EQ(oss.str(), std::string("counter write 50 -> 100\n"));
}

TEST(LoggerTests, NoLeadingZeros)
{
	std::ostringstream oss;
	const Logger logger(oss);

	logger.log_read("x", 0);
	logger.log_read("x", 7);
	logger.log_write("x", 7, 13);

	const std::string out = oss.str();
	const std::string expected =
		"x read 0\n"
		"x read 7\n"
		"x write 7 -> 13\n";

	EXPECT_EQ(out, expected);
}

TEST(LoggerTests, SupportsUint64Max)
{
	std::ostringstream oss;
	const Logger logger(oss);

	constexpr auto maxv = std::numeric_limits<std::uint64_t>::max();
	logger.log_read("big", maxv);

	std::ostringstream check;
	check << "big read " << std::dec << maxv << "\n";

	EXPECT_EQ(oss.str(), check.str());
}

TEST(LoggerTests, MultipleCallsAppendWithNewlines)
{
	std::ostringstream oss;
	const Logger logger(oss);

	logger.log_read("a", 1);
	logger.log_write("a", 1, 2);
	logger.log_read("a", 2);

	const std::string expected =
		"a read 1\n"
		"a write 1 -> 2\n"
		"a read 2\n";

	EXPECT_EQ(oss.str(), expected);
}
