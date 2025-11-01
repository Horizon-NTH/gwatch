#pragma once
#include <cstdint>
#include <string_view>
#include <cstdio>

namespace gwatch
{
	// Interface for emitting access logs.
	// Format (space-separated, decimal, no leading zeros):
	//   <symbol> read  <value>
	//   <symbol> write <old> -> <new>
	class Logger
	{
	public:
		static void log_read(std::string_view symbol, std::uint64_t value);
		static void log_write(std::string_view symbol, std::uint64_t old_value, std::uint64_t new_value);
	};
}
