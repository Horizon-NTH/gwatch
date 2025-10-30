#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

extern "C" __declspec(dllexport) volatile long long g_counter = 0;

int main()
{
	for (int i = 0; i < 64; i++)
	{
		const long long v = g_counter;
		g_counter = v + 1;
	}
	return 123;
}
#else
int main() { return 0; }
#endif
