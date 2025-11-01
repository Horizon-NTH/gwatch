#include "Application.h"
#include "ArgumentsParser.h"
#include <iostream>

int main(const int argc, const char* argv[])
{
#ifdef _WIN32
	try
	{
		const gwatch::CliArgs args = gwatch::ArgumentsParser::parse(std::span(argv, argc));
		if (args.showHelp)
		{
			gwatch::ArgumentsParser::print_usage(std::cout, argc > 0 ? argv[0] : "gwatch");
			return 0;
		}
		gwatch::Application app(args);
		return app.execute();
	}
	catch (const gwatch::ParseError& e)
	{
		std::cerr << "Error: " << e.what() << "\n\n";
		gwatch::ArgumentsParser::print_usage(std::cerr, argc > 0 ? argv[0] : "gwatch");
		return 2;
	}
#else
	std::cerr << "This build currently supports Windows only.\n";
	return 1;
#endif
}
