#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>

namespace gwatch::win
{
	inline std::string last_error_string()
	{
		const DWORD err = GetLastError();
		if (err == 0)
			return "OK";
		LPSTR buf = nullptr;
		const DWORD n = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&buf), 0, nullptr);
		std::string s = n && buf ? std::string(buf, buf + n) : "code=" + std::to_string(err);
		if (buf)
			LocalFree(buf);
		return s;
	}
}
#endif

