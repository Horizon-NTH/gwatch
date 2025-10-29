#include <gtest/gtest.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>
#include <string>
#include <filesystem>

#include "SymbolResolver.h"

extern "C"
{
// Global test variables with stable C linkage (no C++ name mangling).
// Marked volatile and referenced in tests to avoid being optimized out.

// 8-byte integer (should pass)
volatile long long GWatchTest_Global64 = 42;

// 4-byte integer (should pass)
volatile std::int32_t GWatchTest_Global32 = -7;

// Too small (1 byte) → the resolver should reject (size check)
volatile char GWatchTest_Small = 1;

// Too big (16 bytes) → the resolver should reject (size check)
struct GWatchTest_Big16
{
	std::uint64_t a;
	std::uint64_t b;
};

volatile GWatchTest_Big16 GWatchTest_Big = {1u, 2u};
}

// C++ global
namespace GWatchCppNS
{
	volatile long long CppGlobal = 77;
}

namespace
{
	// Touch the globals so the linker keeps them even with higher optimizations.
	void TouchTestGlobals()
	{
		GWatchTest_Global32 += 1;
		GWatchTest_Global64 += GWatchTest_Global32;
		GWatchTest_Small += 1;
		const auto sink = GWatchTest_Big.a + GWatchTest_Big.b;
		(void)sink;
	}

	std::string CurrentModuleNameNoExt()
	{
		char path[MAX_PATH]{};
		const DWORD n = ::GetModuleFileNameA(nullptr, path, MAX_PATH);
		if (n == 0 || n >= MAX_PATH)
			return {};
		const std::filesystem::path p(path);
		return p.stem().string();
	}
}

class WindowsSymbolResolverTest : public ::testing::Test
{
protected:
	std::unique_ptr<gwatch::ISymbolResolver> resolver;

	void SetUp() override
	{
		TouchTestGlobals();
		HANDLE self = ::GetCurrentProcess();
		resolver = std::make_unique<gwatch::WindowsSymbolResolver>(self, "", true);
	}
};

TEST(SymbolResolverCtor, NullHandleThrows)
{
	EXPECT_THROW({
		gwatch::WindowsSymbolResolver bad(nullptr);
		}, gwatch::SymbolError);
}

TEST_F(WindowsSymbolResolverTest, Resolve_Int64_Global)
{
	const auto [name, module, address, size] = resolver->resolve("GWatchTest_Global64");
	EXPECT_EQ(name, "GWatchTest_Global64") << "Name should match the undecorated C symbol.";
	EXPECT_EQ(size, 8u) << "Expect 8 bytes for long long on Windows.";
	EXPECT_NE(address, 0u) << "Virtual address must be non-zero.";
	ASSERT_FALSE(module.empty());
	EXPECT_EQ(module.rfind("0x", 0), 0u) << "Module base is expected as hex string prefixed with 0x.";
}

TEST_F(WindowsSymbolResolverTest, Resolve_Int32_Global)
{
	const auto s = resolver->resolve("GWatchTest_Global32");
	EXPECT_EQ(s.name, "GWatchTest_Global32");
	EXPECT_EQ(s.size, 4u) << "Expect 4 bytes for int32_t.";
	EXPECT_NE(s.address, 0u);
}

TEST_F(WindowsSymbolResolverTest, Resolve_Qualified_ModuleName)
{
	const std::string mod = CurrentModuleNameNoExt();
	ASSERT_FALSE(mod.empty()) << "Cannot determine current module name.";
	const std::string qualified = mod + "!GWatchTest_Global32";
	const auto s = resolver->resolve(qualified);
	EXPECT_EQ(s.name, "GWatchTest_Global32");
	EXPECT_EQ(s.size, 4u);
}

TEST_F(WindowsSymbolResolverTest, Resolve_NonExisting_Symbol_Throws)
{
	EXPECT_THROW({
		(void)resolver->resolve("DefinitelyNotExistingSymbol_12345");
		}, gwatch::SymbolError);
}

TEST_F(WindowsSymbolResolverTest, Rejects_Small_Size)
{
	EXPECT_THROW({
		(void)resolver->resolve("GWatchTest_Small");
		}, gwatch::SymbolError);
}

TEST_F(WindowsSymbolResolverTest, Rejects_Big_Size)
{
	EXPECT_THROW({
		(void)resolver->resolve("GWatchTest_Big");
		}, gwatch::SymbolError);
}

TEST_F(WindowsSymbolResolverTest, Resolve_Cpp_UndecoratedName)
{
	const auto s = resolver->resolve("GWatchCppNS::CppGlobal");
	EXPECT_EQ(s.size, 8u);
	EXPECT_NE(s.address, 0u);
}

#else

TEST(SymbolResolverPortable, SkippedOnNonWindows)
{
	GTEST_SKIP() << "WindowsSymbolResolver tests require _WIN32.";
}

#endif
