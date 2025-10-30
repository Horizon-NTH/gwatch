#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <Psapi.h>
#include <cstddef>
#include <sstream>
#include <vector>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

#include <iostream>

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

	std::string format_win_error(DWORD err)
	{
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

	WindowsSymbolResolver::WindowsSymbolResolver(void* hProcess, const std::string& searchPath, const bool invadeProcess, const ModuleLoadHint* hint) :
		m_hProcess(hProcess)
	{
		if (!m_hProcess)
		{
			throw SymbolError("WindowsSymbolResolver: hProcess invalid (null).");
		}

		SymSetOptions(kSymOpts);

		if (const char* path = searchPath.empty() ? nullptr : searchPath.c_str(); !SymInitialize(m_hProcess, path, invadeProcess ? TRUE : FALSE))
		{
			throw SymbolError(std::string("SymInitialize failed: ") + format_win_error(GetLastError()));
		}
		m_symInitialized = true;
		if (!invadeProcess)
		{
			if (hint && hint->image_base != 0)
			{
				const wchar_t* modulePath = hint->image_path.empty() ? nullptr : hint->image_path.c_str();
				if (!SymLoadModuleExW(m_hProcess, nullptr, modulePath, nullptr,
					hint->image_base,
					static_cast<DWORD>(hint->image_size), nullptr, 0))
				{
					const DWORD err = GetLastError();
					SymCleanup(m_hProcess);
					m_symInitialized = false;
					throw SymbolError(std::string("SymLoadModuleExW failed: ") + format_win_error(err));
				}
			}
			else
			{
				DWORD bytesNeeded = 0;
				const auto hProc = m_hProcess;
				if (!EnumProcessModulesEx(hProc, nullptr, 0, &bytesNeeded, LIST_MODULES_ALL) || bytesNeeded < sizeof(HMODULE))
				{
					const DWORD err = GetLastError();
					SymCleanup(m_hProcess);
					m_symInitialized = false;
					throw SymbolError(std::string("EnumProcessModulesEx(size) failed: ") + format_win_error(err));
				}

				const std::size_t count = bytesNeeded / sizeof(HMODULE);
				std::vector<HMODULE> modules(count);
				if (!EnumProcessModulesEx(hProc, modules.data(), bytesNeeded, &bytesNeeded, LIST_MODULES_ALL) || bytesNeeded < sizeof(HMODULE))
				{
					const DWORD err = GetLastError();
					SymCleanup(m_hProcess);
					m_symInitialized = false;
					throw SymbolError(std::string("EnumProcessModulesEx(list) failed: ") + format_win_error(err));
				}

				if (!modules.empty())
				{
					const HMODULE hModule = modules.front();
					wchar_t path[MAX_PATH]{};
					if (GetModuleFileNameExW(hProc, hModule, path, MAX_PATH) == 0)
					{
						const DWORD err = GetLastError();
						SymCleanup(m_hProcess);
						m_symInitialized = false;
						throw SymbolError(std::string("GetModuleFileNameExW failed: ") + format_win_error(err));
					}

					MODULEINFO mi{};
					if (!GetModuleInformation(hProc, hModule, &mi, sizeof(mi)))
					{
						const DWORD err = GetLastError();
						SymCleanup(m_hProcess);
						m_symInitialized = false;
						throw SymbolError(std::string("GetModuleInformation failed: ") + format_win_error(err));
					}

					if (!SymLoadModuleExW(m_hProcess, nullptr, path, nullptr,
						reinterpret_cast<DWORD64>(mi.lpBaseOfDll),
						mi.SizeOfImage, nullptr, 0))
					{
						const DWORD err = GetLastError();
						SymCleanup(m_hProcess);
						m_symInitialized = false;
						throw SymbolError(std::string("SymLoadModuleExW failed: ") + format_win_error(err));
					}
				}
				else
				{
					SymCleanup(m_hProcess);
					m_symInitialized = false;
					throw SymbolError("EnumProcessModulesEx did not return any modules.");
				}
			}
		}
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
			throw SymbolError("SymFromName(\"" + std::string(symbol) + "\") failed: " + last_error_as_string());
		}

		// Query the size from type information (more reliable for globals than SYMBOL_INFO::Size).
		ULONG64 length = 0;
		if (!SymGetTypeInfo(m_hProcess, info->ModBase, info->TypeIndex, TI_GET_LENGTH, &length))
		{
			throw SymbolError("SymGetTypeInfo(TI_GET_LENGTH) failed: " + last_error_as_string());
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

	std::string WindowsSymbolResolver::last_error_as_string()
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
