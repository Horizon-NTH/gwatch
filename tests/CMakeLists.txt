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
	src/WindowsProcessLauncherTest.cpp
	src/LoggerTest.cpp
	src/WindowsMemoryWatcherTest.cpp
	src/ApplicationTest.cpp
)

add_executable(runTests ${TEST_SOURCES})
target_compile_features(runTests PUBLIC cxx_std_20)
target_link_libraries(runTests GTest::gtest_main ${PROJECT_LIB})

set_target_properties(runTests PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin
)

file(GLOB DEBUGEE_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/debugee/*.cpp"
)

set(DEBUGEE_TARGETS "")
foreach (src IN LISTS DEBUGEE_SOURCES)
	get_filename_component(name_we "${src}" NAME_WE)
	set(tgt "gwatch_debuggee_${name_we}")
	add_executable(${tgt} "${src}")
	list(APPEND DEBUGEE_TARGETS ${tgt})

	set_target_properties(${tgt} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin"
	)

	if (MSVC)
		target_compile_options(${tgt} PRIVATE /Zi)
		target_link_options(${tgt} PRIVATE /DEBUG)
	else ()
		target_compile_options(${tgt} PRIVATE -g)
	endif ()

	add_dependencies(runTests ${tgt})
endforeach ()

foreach (tgt IN LISTS DEBUGEE_TARGETS)
	add_custom_command(TARGET runTests POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:${tgt}>
		$<TARGET_FILE_DIR:runTests>
	)
endforeach ()

include(GoogleTest)
gtest_discover_tests(runTests
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin
)