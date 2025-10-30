#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <iomanip>

#include "MemoryWatcher.h"
#include "ProcessLauncher.h"

namespace gwatch
{
	IMemoryWatcher::IMemoryWatcher(Logger logger) :
		m_logger(logger)
	{
	}

	WindowsMemoryWatcher::WindowsMemoryWatcher(void* hProcess, const ResolvedSymbol& resolvedSymbol, Logger logger, bool enableHardwareBreakpoints) :
		IMemoryWatcher(std::move(logger)),
		m_hProcess(hProcess),
		m_resolvedSymbol(resolvedSymbol),
		m_enableHardwareBreakpoints(enableHardwareBreakpoints)
	{
		if (!m_hProcess)
		{
			throw MemoryWatchError("WindowsMemoryWatcher: null process handle.");
		}
		if (!(resolvedSymbol.size == 4 || resolvedSymbol.size == 8))
		{
			throw MemoryWatchError("WindowsMemoryWatcher: size must be 4 or 8 bytes.");
		}
	}

	ContinueStatus WindowsMemoryWatcher::on_event(const DebugEvent& ev)
	{
		using T = DebugEventType;
		switch (ev.type)
		{
			case T::_CreateProcess:
				try { install_on_thread(ev.thread_id); }
				catch (...) {}
				try { m_lastValue = read_value(); }
				catch (...) { m_lastValue.reset(); }
				return ContinueStatus::Default;

			case T::CreateThread:
				try { install_on_thread(ev.thread_id); }
				catch (...) {}
				return ContinueStatus::Default;

			case T::ExitThread:
				m_armedThreads.erase(ev.thread_id);
				return ContinueStatus::Default;

			case T::Exception:
				{
					if (const auto& ex = std::get<ExceptionInfo>(ev.payload); ex.code == EXCEPTION_SINGLE_STEP)
					{
						return handle_single_step(ev.thread_id);
					}
					return ContinueStatus::Default;
				}

			case T::ExitProcess:
				return ContinueStatus::Default;

			default:
				return ContinueStatus::Default;
		}
	}

	std::string WindowsMemoryWatcher::last_error_string()
	{
		const DWORD err = ::GetLastError();
		if (err == 0)
			return "OK";
		LPSTR buf = nullptr;
		const DWORD n = ::FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&buf), 0, nullptr);
		std::string s = (n && buf) ? std::string(buf, buf + n) : "code=" + std::to_string(err);
		if (buf)
			::LocalFree(buf);
		return s;
	}

	std::uint64_t WindowsMemoryWatcher::mask_for_size(const std::uint32_t size)
	{
		switch (size)
		{
			case 4:
				return 0xFFFFFFFFull;
			case 8:
				return 0xFFFFFFFFFFFFFFFFull;
			default:
				throw MemoryWatchError("mask_for_size: unsupported size (expected 4 or 8).");
		}
	}

	std::uint64_t WindowsMemoryWatcher::len_encoding_for_size(const std::uint32_t size)
	{
		// DR7 LEN encoding (x86/x64):
		switch (size)
		{
			case 4:
				return 0b11;
			case 8:
				return 0b10;
			default:
				throw MemoryWatchError("len_encoding_for_size: unsupported size (expected 4 or 8).");
		}
	}

	void WindowsMemoryWatcher::install_on_thread(const std::uint32_t tid)
	{
		if (!m_enableHardwareBreakpoints)
		{
			m_armedThreads.insert(tid);
			return;
		}

		if (m_armedThreads.contains(tid))
			return;

		const HANDLE hThread = ::OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME,
			FALSE, tid);
		if (!hThread)
		{
			throw MemoryWatchError("OpenThread failed for TID=" + std::to_string(tid) + ": " + last_error_string());
		}

		CONTEXT ctx{};
		ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

		if (!::GetThreadContext(hThread, &ctx))
		{
			::CloseHandle(hThread);
			throw MemoryWatchError("GetThreadContext failed for TID=" + std::to_string(tid) + ": " + last_error_string());
		}

#ifdef _WIN64
		ctx.Dr0 = m_resolvedSymbol.address;
#else
		ctx.Dr0 = static_cast<DWORD>(m_resolvedSymbol.address);
#endif

		// Build DR7 config for slot 0:
		// - L0 (bit 0) = 1 (local enable)
		// - RW0 (bits 16..17) = 11b (read/write)
		// - LEN0 (bits 18..19) = per size
		constexpr std::uint64_t rw = 0b11ull;
		const std::uint64_t len = len_encoding_for_size(static_cast<std::uint32_t>(m_resolvedSymbol.size));

#ifdef _WIN64
		DWORD64 dr7 = ctx.Dr7;
		// Clear previous slot-0 config: L0, RW0, LEN0
		dr7 &= ~(1ull << 0);
		dr7 &= ~(0b11ull << 16);
		dr7 &= ~(0b11ull << 18);
		// Set new config
		dr7 |= (1ull << 0);
		dr7 |= (rw << 16);
		dr7 |= (len << 18);
		ctx.Dr7 = dr7;

		// Clear DR6 to avoid stale status bits.
		ctx.Dr6 = 0;
#else
		DWORD dr7 = ctx.Dr7;
		dr7 &= ~(1u << 0);
		dr7 &= ~(0b11u << 16);
		dr7 &= ~(0b11u << 18);
		dr7 |= (1u << 0);
		dr7 |= (static_cast<DWORD>(rw) << 16);
		dr7 |= (static_cast<DWORD>(len) << 18);
		ctx.Dr7 = dr7;
		ctx.Dr6 = 0;
#endif

		if (!::SetThreadContext(hThread, &ctx))
		{
			::CloseHandle(hThread);
			throw MemoryWatchError("SetThreadContext failed for TID=" + std::to_string(tid) + ": " + last_error_string());
		}

		::CloseHandle(hThread);
		m_armedThreads.insert(tid);
	}

	std::uint64_t WindowsMemoryWatcher::read_value() const
	{
		std::uint64_t val = 0;
		SIZE_T read = 0;
		if (!::ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(m_resolvedSymbol.address), &val, m_resolvedSymbol.size, &read) || read != m_resolvedSymbol.size)
		{
			throw MemoryWatchError("ReadProcessMemory failed: " + last_error_string());
		}
		// Interpret as little-endian unsigned integer masked to 'size' bytes.
		return val & mask_for_size(static_cast<std::uint32_t>(m_resolvedSymbol.size));
	}

	ContinueStatus WindowsMemoryWatcher::handle_single_step(const std::uint32_t tid)
	{
		std::uint64_t current = 0;
		try
		{
			current = read_value();
		}
		catch (...)
		{
			return ContinueStatus::NotHandled;
		}

		if (!m_lastValue.has_value())
		{
			m_logger.log_read(m_resolvedSymbol.name, current);
			m_lastValue = current;
			return ContinueStatus::Default;
		}

		if (current != *m_lastValue)
		{
			m_logger.log_write(m_resolvedSymbol.name, *m_lastValue, current);
			*m_lastValue = current;
		}
		else
		{
			m_logger.log_read(m_resolvedSymbol.name, current);
		}

		// Ensure DR0 remains armed for this thread. Normally DR state persists, but some debuggers refresh.
		if (!m_armedThreads.contains(tid))
		{
			try { install_on_thread(tid); }
			catch (...) {}
		}

		return ContinueStatus::Default;
	}
}

#endif
