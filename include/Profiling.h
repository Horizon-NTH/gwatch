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

	// Program phases and loop timings
	void add_process_launch_duration(std::uint64_t nanoseconds);
	void add_symbol_resolve_duration(std::uint64_t nanoseconds);
	void add_setup_watcher_duration(std::uint64_t nanoseconds);
	void add_loop_wait_duration(std::uint64_t nanoseconds);
	void add_loop_handle_duration(std::uint64_t nanoseconds);
	void inc_loop_iteration();
#else
	class EventTimer
	{
	public:
		EventTimer() = default;
	};

	inline void add_read_duration(std::uint64_t) {}
	inline void add_log_duration(std::uint64_t) {}
    inline void add_process_launch_duration(std::uint64_t) {}
    inline void add_symbol_resolve_duration(std::uint64_t) {}
    inline void add_setup_watcher_duration(std::uint64_t) {}
    inline void add_loop_wait_duration(std::uint64_t) {}
    inline void add_loop_handle_duration(std::uint64_t) {}
    inline void inc_loop_iteration() {}
#endif
}
