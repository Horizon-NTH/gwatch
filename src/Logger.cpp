#include "../include/Logger.h"
#ifdef GWATCH_PROFILE
#include "../include/Profiling.h"
#include <chrono>
#endif
#include <cinttypes>
#include <cstdio>
#include <charconv>

namespace gwatch
{
	void Logger::log_read(const std::string_view symbol, const std::uint64_t value)
	{
#ifdef GWATCH_PROFILE
		const auto start = std::chrono::high_resolution_clock::now();
#endif
		std::printf("%.*s read %" PRIu64 "\n", static_cast<int>(symbol.size()), symbol.data(), static_cast<uint64_t>(value));
#ifdef GWATCH_PROFILE
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
		profiling::add_log_duration(static_cast<std::uint64_t>(elapsed));
#endif
	}

	void Logger::log_write(const std::string_view symbol, const std::uint64_t old_value, const std::uint64_t new_value)
	{
#ifdef GWATCH_PROFILE
		const auto start = std::chrono::high_resolution_clock::now();
#endif
		std::printf("%.*s write %" PRIu64 " -> %" PRIu64 "\n", static_cast<int>(symbol.size()), symbol.data(), static_cast<uint64_t>(old_value), static_cast<uint64_t>(new_value));
#ifdef GWATCH_PROFILE
		const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();
		profiling::add_log_duration(static_cast<std::uint64_t>(elapsed));
#endif
	}
}
