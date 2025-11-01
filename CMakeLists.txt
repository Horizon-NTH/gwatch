cmake_minimum_required(VERSION 3.14)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(gwatch VERSION 1.0)

option(ENABLE_TESTS "Enable testing" OFF)
option(GWATCH_PROFILE "Enable internal gwatch profiling instrumentation" OFF)

set(HEADER_FILES
	include/ArgumentsParser.h
	include/SymbolResolver.h
	include/ProcessLauncher.h
	include/MemoryWatcher.h
	include/Logger.h
	include/Application.h
	include/Profiling.h
)

set(SOURCE_FILES
	src/ArgumentsParser.cpp
	src/WindowsSymbolResolver.cpp
	src/WindowsProcessLauncher.cpp
	src/WindowsMemoryWatcher.cpp
	src/Logger.cpp
	src/Application.cpp
	src/Profiling.cpp
)

set(PROJECT_LIB ${PROJECT_NAME}_lib)

add_library(${PROJECT_LIB} STATIC ${HEADER_FILES} ${SOURCE_FILES})

target_include_directories(${PROJECT_LIB} PUBLIC include)
target_compile_features(${PROJECT_LIB} PUBLIC cxx_std_20)

if (WIN32)
    target_link_libraries(${PROJECT_LIB} PUBLIC Dbghelp)
endif ()

add_executable(${PROJECT_NAME} src/main.cpp)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_link_libraries(${PROJECT_NAME} ${PROJECT_LIB})
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if (GWATCH_PROFILE)
	target_compile_definitions(${PROJECT_LIB} PRIVATE GWATCH_PROFILE)
	target_compile_definitions(${PROJECT_NAME} PRIVATE GWATCH_PROFILE)
endif ()

if (ENABLE_TESTS)
	add_subdirectory(tests)
endif ()
