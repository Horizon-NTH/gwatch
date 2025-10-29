enable_testing()

add_executable(gwatch_debuggee_toy debuggee_toy.cpp)
# Put the debuggee next to the test binary for easy discovery
set_target_properties(gwatch_debuggee_toy PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin
)
if (MSVC)
	target_compile_options(gwatch_debuggee_toy PRIVATE /Zi)
	target_link_options(gwatch_debuggee_toy PRIVATE /DEBUG)
endif ()

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
	src/ProcessLauncherTest.cpp
)

add_executable(runTests ${TEST_SOURCES})

target_compile_features(runTests PUBLIC cxx_std_20)
target_link_libraries(runTests GTest::gtest_main ${PROJECT_LIB})
set_target_properties(runTests PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin
)

add_custom_command(TARGET runTests POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:gwatch_debuggee_toy>
	$<TARGET_FILE_DIR:runTests>
)

include(GoogleTest)
gtest_discover_tests(runTests
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin
)