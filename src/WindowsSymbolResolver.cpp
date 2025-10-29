#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#include <cstddef>
#include <sstream>
#include <vector>

#pragma comment(lib, "dbghelp.lib")

#include "SymbolResolver.h"

namespace gwatch
{
	namespace
	{
		// Recommended DbgHelp options:
		constexpr DWORD kSymOpts =
			SYMOPT_UNDNAME				// undecorate names when possible
			| SYMOPT_DEFERRED_LOADS     // defer symbol loading for performance
			| SYMOPT_LOAD_LINES;		// useful while debugging (minimal overhead for resolution)

		std::string to_hex(const uint64_t v)
		{
			std::ostringstream oss;
			oss << "0x" << std::hex << std::uppercase << v;
			return oss.str();
		}
	}

	WindowsSymbolResolver::WindowsSymbolResolver(void* hProcess, const std::string& searchPath, const bool invadeProcess) :
		m_hProcess(hProcess)
	{
		if (!m_hProcess)
		{
			throw SymbolError("WindowsSymbolResolver: hProcess invalid (null).");
		}

		SymSetOptions(kSymOpts);

		if (const char* path = searchPath.empty() ? nullptr : searchPath.c_str(); !SymInitialize(m_hProcess, path, invadeProcess ? TRUE : FALSE))
		{
			throw SymbolError(std::string("SymInitialize failed: ") + lastErrorAsString());
		}
		m_symInitialized = true;
	}

	WindowsSymbolResolver::~WindowsSymbolResolver()
	{
		if (m_symInitialized)
		{
			SymCleanup(m_hProcess);
		}
	}

	ResolvedSymbol WindowsSymbolResolver::resolve(const std::string_view symbol)
	{
		// Allocate a buffer large enough for SYMBOL_INFO with a long name.
		constexpr DWORD MaxNameLen = 1024;
		constexpr size_t bufSize = sizeof(SYMBOL_INFO) + MaxNameLen * sizeof(char);

		std::vector<std::byte> buf(bufSize);
		auto* info = reinterpret_cast<SYMBOL_INFO*>(buf.data());
		std::memset(info, 0, sizeof(SYMBOL_INFO));
		info->SizeOfStruct = sizeof(SYMBOL_INFO);
		info->MaxNameLen = MaxNameLen;

		if (!SymFromName(m_hProcess, std::string(symbol).c_str(), info))
		{
			throw SymbolError("SymFromName(\"" + std::string(symbol) + "\") failed: " + lastErrorAsString());
		}

		// Query the size from type information (more reliable for globals than SYMBOL_INFO::Size).
		ULONG64 length = 0;
		if (!SymGetTypeInfo(m_hProcess, info->ModBase, info->TypeIndex, TI_GET_LENGTH, &length))
		{
			throw SymbolError("SymGetTypeInfo(TI_GET_LENGTH) failed: " + lastErrorAsString());
		}

		ResolvedSymbol out
			{
				.name = info->Name,
				.module = to_hex(info->ModBase),
				.address = info->Address,
				.size = length
			};

		if (out.size < 4 || out.size > 8)
		{
			std::ostringstream oss;
			oss << "The symbol \"" << out.name << "\" has a size of " << out.size
				<< " bytes (outside the range [4..8]).";
			throw SymbolError(oss.str());
		}

		return out;
	}

	std::string WindowsSymbolResolver::lastErrorAsString()
	{
		const DWORD err = GetLastError();
		if (err == 0)
			return "OK";

		LPSTR msgBuf = nullptr;
		const DWORD size = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);

		std::string message = (size && msgBuf) ? std::string(msgBuf, msgBuf + size) : "code=" + std::to_string(err);
		if (msgBuf)
			LocalFree(msgBuf);
		return message;
	}
}

#endif
