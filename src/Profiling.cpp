#include "../include/Profiling.h"

#ifdef GWATCH_PROFILE

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace
{
	struct ProfilingStats
	{
		std::atomic<std::uint64_t> event_count{0};
		std::atomic<std::uint64_t> read_count{0};
		std::atomic<std::uint64_t> log_count{0};
		std::atomic<long long> event_ns{0};
		std::atomic<long long> read_ns{0};
		std::atomic<long long> log_ns{0};
	};

	ProfilingStats& stats()
	{
		static ProfilingStats instance;
		return instance;
	}

	class Reporter
	{
	public:
		Reporter()
		{
			std::atexit(&Reporter::dump);
		}

		static void dump()
		{
			const auto events = stats().event_count.load(std::memory_order_relaxed);
			if (events == 0)
				return;

			const auto reads = stats().read_count.load(std::memory_order_relaxed);
			const auto logs = stats().log_count.load(std::memory_order_relaxed);
			const auto total_event_ns = stats().event_ns.load(std::memory_order_relaxed);
			const auto total_read_ns = stats().read_ns.load(std::memory_order_relaxed);
			const auto total_log_ns = stats().log_ns.load(std::memory_order_relaxed);

			const auto leftover_ns = total_event_ns - total_read_ns - total_log_ns;

			const auto to_ms = [](long long ns) -> double
			{
				return static_cast<double>(ns) / 1'000'000.0;
			};

			const auto safe_avg = [](long long total_ns, std::uint64_t count) -> double
			{
				if (count == 0)
					return 0.0;
				return static_cast<double>(total_ns) / static_cast<double>(count);
			};

			std::cerr << std::fixed << std::setprecision(3);
			std::cerr << "[profiling] events: " << events
				<< " total=" << to_ms(total_event_ns) << " ms"
				<< " avg=" << safe_avg(total_event_ns, events) / 1'000'000.0 << " ms\n";
			std::cerr << "[profiling] read_value calls: " << reads
				<< " total=" << to_ms(total_read_ns) << " ms"
				<< " avg=" << safe_avg(total_read_ns, reads) / 1'000.0 << " us\n";
			std::cerr << "[profiling] logger calls: " << logs
				<< " total=" << to_ms(total_log_ns) << " ms"
				<< " avg=" << safe_avg(total_log_ns, logs) / 1'000.0 << " us\n";
			std::cerr << "[profiling] other handler time total="
				<< to_ms(std::max<long long>(0, leftover_ns)) << " ms\n";
		}
	};

	[[maybe_unused]] const Reporter g_reporter{};
}

namespace gwatch::profiling
{
	namespace
	{
		using clock = std::chrono::high_resolution_clock;
	}

	EventTimer::EventTimer() :
		m_start(clock::now())
	{
	}

	EventTimer::~EventTimer()
	{
		const auto end = clock::now();
		const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
		stats().event_count.fetch_add(1, std::memory_order_relaxed);
		stats().event_ns.fetch_add(ns, std::memory_order_relaxed);
	}

	void add_read_duration(const std::uint64_t nanoseconds)
	{
		stats().read_count.fetch_add(1, std::memory_order_relaxed);
		stats().read_ns.fetch_add(static_cast<long long>(nanoseconds), std::memory_order_relaxed);
	}

	void add_log_duration(const std::uint64_t nanoseconds)
	{
		stats().log_count.fetch_add(1, std::memory_order_relaxed);
		stats().log_ns.fetch_add(static_cast<long long>(nanoseconds), std::memory_order_relaxed);
	}
}

#endif
