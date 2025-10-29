#pragma once
#include <cstdint>
#include <string_view>
#include <ostream>
#include <memory>

namespace gwatch
{
	// Interface for emitting access logs.
	// Format (space-separated, decimal, no leading zeros):
	//   <symbol> read  <value>
	//   <symbol> write <old> -> <new>
	class Logger
	{
	public:
		explicit Logger(std::ostream& os);

		void log_read(std::string_view symbol, std::uint64_t value) const;

		void log_write(std::string_view symbol, std::uint64_t old_value, std::uint64_t new_value) const;

	private:
		std::ostream* m_os;
	};
}
