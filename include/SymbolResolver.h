#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace gwatch
{
	struct ResolvedSymbol
	{
		std::string name;      // resolved name
		std::string module;    // module base as hex string
		std::uint64_t address; // virtual address in the target process
		std::uint64_t size;    // size in bytes
	};

	class SymbolError final : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	class ISymbolResolver
	{
	public:
		virtual ~ISymbolResolver() = default;

		virtual ResolvedSymbol resolve(std::string_view symbol) = 0;
	};

#ifdef _WIN32

	class WindowsSymbolResolver final : public ISymbolResolver
	{
	public:
		explicit WindowsSymbolResolver(void* hProcess, const std::string& searchPath = {}, bool invadeProcess = true);

		~WindowsSymbolResolver() override;

		WindowsSymbolResolver(const WindowsSymbolResolver&) = delete;
		WindowsSymbolResolver& operator=(const WindowsSymbolResolver&) = delete;
		WindowsSymbolResolver(WindowsSymbolResolver&&) = delete;
		WindowsSymbolResolver& operator=(WindowsSymbolResolver&&) = delete;

		ResolvedSymbol resolve(std::string_view symbol) override;

	private:
		void* m_hProcess{};
		bool m_symInitialized{false};

		static std::string last_error_as_string();
	};

#endif
}
