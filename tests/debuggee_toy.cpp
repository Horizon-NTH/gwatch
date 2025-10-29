#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// A tiny debuggee that emits OutputDebugString messages and exits with a fixed code.
int main()
{
	::OutputDebugStringA("DBG:hello_ascii");
	::OutputDebugStringW(L"DBG:hello_utf16_é");
	return 123;
}
#else
int main() { return 0; }
#endif
