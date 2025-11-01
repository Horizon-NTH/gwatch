long long g_counter = 0;

int main()
{
	for (int i = 0; i < 4; ++i)
	{
		const long long v = g_counter;
		g_counter = v + 1;
	}
	return 123;
}
