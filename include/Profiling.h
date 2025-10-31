#pragma once

#include <cstdint>

#ifdef GWATCH_PROFILE
#include <chrono>
#endif

namespace gwatch::profiling
{
#ifdef GWATCH_PROFILE
	class EventTimer
	{
	public:
		EventTimer();
		~EventTimer();

		EventTimer(const EventTimer&) = delete;
		EventTimer& operator=(const EventTimer&) = delete;
		EventTimer(EventTimer&&) = delete;
		EventTimer& operator=(EventTimer&&) = delete;

	private:
		std::chrono::high_resolution_clock::time_point m_start;
	};

	void add_read_duration(std::uint64_t nanoseconds);
	void add_log_duration(std::uint64_t nanoseconds);
#else
	class EventTimer
	{
	public:
		EventTimer() = default;
	};

	inline void add_read_duration(std::uint64_t) {}
	inline void add_log_duration(std::uint64_t) {}
#endif
}
