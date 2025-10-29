#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <span>

namespace gwatch
{
	struct CliArgs
	{
		std::string symbol;                  // --var
		std::string execPath;                // --exec
		std::vector<std::string> targetArgs; // args after separtor --
		bool showHelp = false;               // -h / --help
	};

	class ParseError final : public std::runtime_error
	{
	public:
		explicit ParseError(const std::string& msg) :
			std::runtime_error(msg) {}
	};

	class ArgumentsParser
	{
	public:
		static CliArgs parse(const std::span<const char*>& args);

		static void print_usage(std::ostream& os, std::string_view programName);

	private:
		static void ensure_not_duplicate(bool seen, std::string_view opt);
		static std::string next_value(const std::span<const char*>& args, int idx, std::string_view optName);
	};
}
