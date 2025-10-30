#include "../include/Logger.h"

namespace gwatch
{
	Logger::Logger(std::ostream& os) :
		m_os(&os)
	{
	}

	void Logger::log_read(const std::string_view symbol, const std::uint64_t value) const
	{
		*m_os << symbol << " read " << value << '\n';
	}

	void Logger::log_write(const std::string_view symbol, const std::uint64_t old_value, const std::uint64_t new_value) const
	{
		*m_os << symbol << " write " << old_value << " -> " << new_value << '\n';
	}
}
