#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include <cstdint>

std::int64_t g_counter = 0;

namespace
{
	constexpr std::int32_t kIterations = 20000;
}

int main()
{
	for (std::int32_t i = 0; i < kIterations; ++i)
	{
		const auto current = g_counter;
		g_counter = current + 1;
	}
	return 0;
}

#else
int main() { return 0; }
#endif
