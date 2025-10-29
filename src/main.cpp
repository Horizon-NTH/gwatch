#include "ArgumentsParser.h"

int main(const int argc, const char* argv[])
{
	const gwatch::CliArgs args = gwatch::ArgumentsParser::parse(std::span(argv, argc));
	return 0;
}
