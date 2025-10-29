#include "ArgumentsParser.h"

#include <sstream>

namespace gwatch
{
	CliArgs ArgumentsParser::parse(const std::span<const char*>& args)
	{
		CliArgs out;

		size_t n = args.size();
		if (n <= 1)
		{
			out.showHelp = true;
			return out;
		}

		bool seenVar = false;
		bool seenExec = false;

		int i = 1;
		while (i < n)
		{
			std::string tok = args[i];

			if (tok == "--")
			{
				for (int j = i + 1; j < n; j++)
				{
					out.targetArgs.emplace_back(args[j]);
				}
				break;
			}

			if (tok == "-h" || tok == "--help")
			{
				out.showHelp = true;
				return out;
			}

			if (tok.starts_with("--var="))
			{
				ensure_not_duplicate(seenVar, "--var");
				out.symbol = tok.substr(6);
				if (out.symbol.empty())
				{
					throw ParseError("Empty value for --var");
				}
				seenVar = true;
				i++;
				continue;
			}
			if (tok == "--var" || tok == "-v")
			{
				ensure_not_duplicate(seenVar, "--var");
				std::string val = next_value(args, i, "--var");
				if (val.empty())
				{
					throw ParseError("Empty value for --var");
				}
				out.symbol = std::move(val);
				seenVar = true;
				i += 2;
				continue;
			}

			if (tok.starts_with("--exec="))
			{
				ensure_not_duplicate(seenExec, "--exec");
				out.execPath = tok.substr(7);
				if (out.execPath.empty())
				{
					throw ParseError("Empty value for --exec");
				}
				seenExec = true;
				i++;
				continue;
			}
			if (tok == "--exec" || tok == "-e")
			{
				ensure_not_duplicate(seenExec, "--exec");
				std::string val = next_value(args, i, "--exec");
				if (val.empty())
				{
					throw ParseError("Empty value for --exec");
				}
				out.execPath = std::move(val);
				seenExec = true;
				i += 2;
				continue;
			}

			if (!tok.empty() && tok[0] == '-')
			{
				std::ostringstream oss;
				oss << "Unknown option: " << tok;
				throw ParseError(oss.str());
			}

			std::ostringstream oss;
			oss << "Unexpected argument before `--`: " << tok
				<< "\nHint: place target arguments after `--`.";
			throw ParseError(oss.str());
		}

		if (!seenVar || out.symbol.empty())
		{
			throw ParseError("Missing required option: --var <symbol>");
		}
		if (!seenExec || out.execPath.empty())
		{
			throw ParseError("Missing required option: --exec <path>");
		}

		return out;
	}

	void ArgumentsParser::print_usage(std::ostream& os, const std::string_view programName)
	{
		os <<
			"Usage:\n"
			"  " << programName << " --var <symbol> --exec <path> [-- arg1 ... argN]\n\n"
			"Options:\n"
			"  -v, --var <symbol>     Global variable name to watch (required)\n"
			"  -e, --exec <path>      Path to the executable to run (required)\n"
			"      --                 Separator, everything after is passed to the target\n"
			"  -h, --help             Show this help and exit\n\n"
			"Notes:\n"
			"  - Also accepts --var=NAME and --exec=PATH forms.\n"
			"  - Target program arguments must appear after `--`.\n";
	}

	void ArgumentsParser::ensure_not_duplicate(const bool seen, const std::string_view opt)
	{
		if (seen)
		{
			std::ostringstream oss;
			oss << "Option specified more than once: " << opt;
			throw ParseError(oss.str());
		}
	}

	std::string ArgumentsParser::next_value(const std::span<const char*>& args, const int idx, const std::string_view optName)
	{
		if (idx + 1 >= args.size())
		{
			std::ostringstream oss;
			oss << "Missing value for option: " << optName;
			throw ParseError(oss.str());
		}
		std::string v = args[idx + 1];
		if (!v.empty() && v[0] == '-')
		{
			std::ostringstream oss;
			oss << "Missing value for option: " << optName
				<< " (got another option '" << v << "' instead)";
			throw ParseError(oss.str());
		}
		return v;
	}
}
