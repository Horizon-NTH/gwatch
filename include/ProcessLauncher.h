#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <optional>
#include <stdexcept>

namespace gwatch
{
	class ProcessError final : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	struct LaunchConfig
	{
		std::string exe_path;                 // absolute or relative path to the executable
		std::vector<std::string> args;        // argv[1..]
		std::optional<std::string> workdir;   // working directory (null -> inherit)
		bool inherit_handles = false;         // CreateProcess bInheritHandles
		bool new_console = false;             // Create a new console for the debuggee
		bool suspended = false;               // Start suspended (debugger can patch before resume)
		bool debug_children = false;          // DEBUG_PROCESS vs DEBUG_ONLY_THIS_PROCESS
	};

	// Cross-platform debug event types
	enum class DebugEventType
	{
		CreateProcess,
		ExitProcess,
		CreateThread,
		ExitThread,
		Exception,
		LoadDll,
		UnloadDll,
		OutputDebugString,
		Rip
	};

	struct ExceptionInfo
	{
		std::uint32_t code = 0;       // OS-specific exception code
		std::uint64_t address = 0;    // faulting address / EIP-RIP for breakpoint/singlestep
		bool first_chance = false;    // true = first chance, false = second chance
	};

	struct CreateProcessInfo
	{
		std::uint64_t image_base = 0;   // base address of the image (module)
		std::uint64_t entry_point = 0;  // entry-point address
		std::string image_path;         // best-effort resolved path (may be empty)
	};

	struct ExitProcessInfo
	{
		std::uint32_t exit_code = 0;
	};

	struct CreateThreadInfo
	{
		std::uint64_t start_address = 0;
	};

	struct ExitThreadInfo
	{
		std::uint32_t exit_code = 0;
	};

	struct LoadDllInfo
	{
		std::uint64_t base = 0;      // base address of the loaded module
		std::string path;            // best-effort resolved path (may be empty)
	};

	struct UnloadDllInfo
	{
		std::uint64_t base = 0;
	};

	struct OutputDebugStringInfo
	{
		std::string message;
	};

	struct RipInfo
	{
		std::uint32_t error = 0;
		std::uint32_t type = 0;
	};

	// Generic event container passed to the client sink.
	struct DebugEvent
	{
		DebugEventType type{};
		std::uint32_t process_id = 0;
		std::uint32_t thread_id = 0;

		std::variant<
			CreateProcessInfo,
			ExitProcessInfo,
			CreateThreadInfo,
			ExitThreadInfo,
			ExceptionInfo,
			LoadDllInfo,
			UnloadDllInfo,
			OutputDebugStringInfo,
			RipInfo
		> payload;
	};

	// What the client asks the loop to do after an event.
	// - Default: let the launcher decide sensible defaults (e.g., swallow breakpoints).
	// - Continue: force DBG_CONTINUE (Windows).
	// - NotHandled: force DBG_EXCEPTION_NOT_HANDLED (Windows).
	enum class ContinueStatus { Default, Continue, NotHandled };

	// Event sink implemented by the watcher to observe and steer the debug loop.
	class IDebugEventSink
	{
	public:
		virtual ~IDebugEventSink() = default;

		// Called for every debug event. Return how the loop should continue.
		// The default return (ContinueStatus::Default) lets the launcher map to OS defaults.
		virtual ContinueStatus on_event(const DebugEvent& ev) = 0;
	};

	class IProcessLauncher
	{
	public:
		virtual ~IProcessLauncher() = default;

		virtual void launch(const LaunchConfig& cfg) = 0;
		virtual std::optional<std::uint32_t> run_debug_loop(IDebugEventSink& sink) = 0;
		virtual void stop() = 0;

		virtual std::uint32_t pid() const = 0;
		virtual bool running() const = 0;
	};

#ifdef _WIN32

	// Windows implementation
	class WindowsProcessLauncher final : public IProcessLauncher
	{
	public:
		WindowsProcessLauncher();
		~WindowsProcessLauncher() override;

		WindowsProcessLauncher(const WindowsProcessLauncher&) = delete;
		WindowsProcessLauncher& operator=(const WindowsProcessLauncher&) = delete;
		WindowsProcessLauncher(WindowsProcessLauncher&&) = delete;
		WindowsProcessLauncher& operator=(WindowsProcessLauncher&&) = delete;

		void launch(const LaunchConfig& cfg) override;
		std::optional<std::uint32_t> run_debug_loop(IDebugEventSink& sink) override;
		void stop() override;

		std::uint32_t pid() const override { return m_pid; }
		bool running() const override { return m_running; }

	private:
		HANDLE m_hProcess = nullptr;
		HANDLE m_hThread = nullptr;
		std::uint32_t m_pid = 0;
		std::uint32_t m_tid = 0;

		bool m_launched = false;
		bool m_running = false;
		bool m_requestStop = false;

		static std::wstring to_wstring(std::string_view s);
		static std::string utf8_from_wstring(std::wstring_view ws);
		static std::wstring build_command_line(const LaunchConfig& cfg);
		static std::wstring quote_arg(std::wstring_view arg);
		static std::string last_error_string();

		static std::string resolve_module_path(HANDLE hFile, HANDLE hProcess, void* remoteImageName, std::uint16_t isUnicode, std::size_t maxBytes = 32768);

		static std::string read_remote_string(HANDLE hProcess, const void* remote, bool isUnicode, std::size_t maxBytes);

		static std::uint32_t map_continue_code(ContinueStatus sinkDecision, const DebugEvent& ev);
	};

#endif
}
