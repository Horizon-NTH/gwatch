#include "../include/Logger.h"
#ifdef GWATCH_PROFILE
#include "../include/Profiling.h"
#include <chrono>
#endif

namespace gwatch
{
	Logger::Logger(std::ostream& os) :
		m_os(&os)
	{
	}

	void Logger::log_read(const std::string_view symbol, const std::uint64_t value) const
	{
#ifdef GWATCH_PROFILE
		const auto start = std::chrono::high_resolution_clock::now();
#endif
		*m_os << symbol << " read " << value << '\n';
#ifdef GWATCH_PROFILE
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
		profiling::add_log_duration(static_cast<std::uint64_t>(elapsed));
#endif
	}

	void Logger::log_write(const std::string_view symbol, const std::uint64_t old_value, const std::uint64_t new_value) const
	{
#ifdef GWATCH_PROFILE
		const auto start = std::chrono::high_resolution_clock::now();
#endif
		*m_os << symbol << " write " << old_value << " -> " << new_value << '\n';
#ifdef GWATCH_PROFILE
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
		profiling::add_log_duration(static_cast<std::uint64_t>(elapsed));
#endif
	}
}
