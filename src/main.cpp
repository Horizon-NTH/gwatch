#include "Application.h"
#include "ArgumentsParser.h"

int main(const int argc, const char* argv[])
{
#ifdef _WIN32
	const gwatch::CliArgs args = gwatch::ArgumentsParser::parse(std::span(argv, argc));
	gwatch::Application app(args);
	return app.execute();
#else
	std::cerr << "This build currently supports Windows only.\n";
	return 1;
#endif
}
