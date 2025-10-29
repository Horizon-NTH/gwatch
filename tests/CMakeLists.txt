enable_testing()

include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(TEST_SOURCES
	src/ArgumentsParserTest.cpp
	src/WindowsSymbolResolverTest.cpp
)

add_executable(runTests ${TEST_SOURCES})

target_compile_features(runTests PUBLIC cxx_std_20)
target_link_libraries(runTests GTest::gtest_main ${PROJECT_LIB})

include(GoogleTest)
gtest_discover_tests(runTests)