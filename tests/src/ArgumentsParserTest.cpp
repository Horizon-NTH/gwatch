#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <span>
#include <stdexcept>

#include "ArgumentsParser.h"

using gwatch::ArgumentsParser;
using gwatch::CliArgs;
using gwatch::ParseError;

namespace
{
	struct ArgvBuilder
	{
		std::vector<std::string> storage;
		std::vector<const char*> ptrs;

		ArgvBuilder& add(std::string s)
		{
			storage.emplace_back(std::move(s));
			return *this;
		}

		std::span<const char*> span()
		{
			ptrs.clear();
			ptrs.reserve(storage.size());
			for (const auto& s : storage)
				ptrs.push_back(s.c_str());
			return std::span{ptrs.data(), ptrs.size()};
		}
	};

	void expect_parse_error_contains(const std::span<const char*> sp, const std::string_view needle)
	{
		try
		{
			ArgumentsParser::parse(sp);
			FAIL() << "Expected ParseError";
		}
		catch (const ParseError& e)
		{
			std::string msg = e.what();
			ASSERT_NE(msg.find(std::string(needle)), std::string::npos)
		        << "Error message does not contain expected fragment.\n"
		        << "Message: " << msg << "\nExpected to contain: " << needle;
		}
		catch (...)
		{
			FAIL() << "Expected ParseError, got different exception type";
		}
	}
}

TEST(ArgumentsParser, Parses_LongForms_WithSeparateValues)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var").add("foo").add("--exec").add("/bin/echo");
	const auto sp = ab.span();

	const auto [symbol, execPath, targetArgs, showHelp] = ArgumentsParser::parse(sp);
	EXPECT_FALSE(showHelp);
	EXPECT_EQ(symbol, "foo");
	EXPECT_EQ(execPath, "/bin/echo");
	EXPECT_TRUE(targetArgs.empty());
}

TEST(ArgumentsParser, Parses_LongForms_WithEquals)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var=foo").add("--exec=/usr/bin/true");
	const auto sp = ab.span();

	const auto [symbol, execPath, targetArgs, showHelp] = ArgumentsParser::parse(sp);
	EXPECT_FALSE(showHelp);
	EXPECT_EQ(symbol, "foo");
	EXPECT_EQ(execPath, "/usr/bin/true");
	EXPECT_TRUE(targetArgs.empty());
}

TEST(ArgumentsParser, Parses_ShortAliases)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("-v").add("SYM").add("-e").add("/bin/false");
	const auto sp = ab.span();

	const auto [symbol, execPath, targetArgs, showHelp] = ArgumentsParser::parse(sp);
	EXPECT_FALSE(showHelp);
	EXPECT_EQ(symbol, "SYM");
	EXPECT_EQ(execPath, "/bin/false");
	EXPECT_TRUE(targetArgs.empty());
}

TEST(ArgumentsParser, Collects_TargetArgs_AfterSeparator)
{
	ArgvBuilder ab;
	ab.add("gwatch")
	  .add("--var").add("X")
	  .add("--exec").add("/bin/echo")
	  .add("--")
	  .add("-n").add("hello world").add("42");
	const auto sp = ab.span();

	const CliArgs args = ArgumentsParser::parse(sp);
	EXPECT_FALSE(args.showHelp);
	ASSERT_EQ(args.targetArgs.size(), 3u);
	EXPECT_EQ(args.targetArgs[0], "-n");
	EXPECT_EQ(args.targetArgs[1], "hello world");
	EXPECT_EQ(args.targetArgs[2], "42");
}

TEST(ArgumentsParser, Help_WhenNoArgs_ShowsHelp)
{
	ArgvBuilder ab;
	ab.add("gwatch");
	const auto sp = ab.span();

	const CliArgs args = ArgumentsParser::parse(sp);
	EXPECT_TRUE(args.showHelp);
}

TEST(ArgumentsParser, Help_Flag_SetsShowHelp)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--help").add("--var").add("X").add("--exec").add("/bin/echo");
	const auto sp = ab.span();

	const CliArgs args = ArgumentsParser::parse(sp);
	EXPECT_TRUE(args.showHelp);
}

TEST(ArgumentsParser, Error_UnknownOption)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var").add("X").add("--unknown").add("--exec").add("/bin/echo");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Unknown option");
}

TEST(ArgumentsParser, Error_PositionalBeforeSeparator)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("positional").add("--var").add("X").add("--exec").add("/bin/echo");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Unexpected argument before `--`");
}

TEST(ArgumentsParser, Error_MissingExec)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var").add("X");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Missing required option: --exec");
}

TEST(ArgumentsParser, Error_MissingVar)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--exec").add("/bin/echo");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Missing required option: --var");
}

TEST(ArgumentsParser, Error_MissingValue_ForVar)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var").add("--exec").add("/bin/echo");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Missing value for option: --var");
}

TEST(ArgumentsParser, Error_MissingValue_ForExec)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var").add("X").add("--exec");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Missing value for option: --exec");
}

TEST(ArgumentsParser, Error_DuplicateVar)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var").add("X").add("--var").add("Y").add("--exec").add("/bin/echo");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Option specified more than once");
}

TEST(ArgumentsParser, Error_DuplicateExec)
{
	ArgvBuilder ab;
	ab.add("gwatch")
	  .add("--var").add("X")
	  .add("--exec").add("/bin/echo")
	  .add("--exec").add("/bin/false");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Option specified more than once");
}

TEST(ArgumentsParser, Error_EmptyValue_VarEquals)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var=").add("--exec").add("/bin/echo");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Empty value for --var");
}

TEST(ArgumentsParser, Error_EmptyValue_ExecEquals)
{
	ArgvBuilder ab;
	ab.add("gwatch").add("--var").add("X").add("--exec=");
	const auto sp = ab.span();

	expect_parse_error_contains(sp, "Empty value for --exec");
}
