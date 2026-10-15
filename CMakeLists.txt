cmake_minimum_required(VERSION 3.24)

project(TaskMesh
    VERSION 0.1.0
    DESCRIPTION "High-performance priority job scheduling engine in modern C++20"
    LANGUAGES CXX
)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for tooling (clangd, clang-tidy, IDEs)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option to enable/disable testing (default ON)
option(TASKMESH_BUILD_TESTS "Build TaskMesh test suite" ON)

# Warning flags interface
add_library(taskmesh_warnings INTERFACE)
if(MSVC)
    target_compile_options(taskmesh_warnings INTERFACE
        /W4
        /permissive-
        /EHsc
        /Zc:__cplusplus
    )
else()
    target_compile_options(taskmesh_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# Third-party SQLite3 library
add_library(sqlite3 STATIC
    third_party/sqlite/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC
    third_party/sqlite
)
if(MSVC)
    target_compile_options(sqlite3 PRIVATE /W3 /EHsc)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()

# Core static library
add_library(taskmesh_core STATIC
    src/job.cpp
    src/priority_scheduler.cpp
    src/job_executor.cpp
    src/concurrent_priority_queue.cpp
    src/execution_engine.cpp
    src/job_store.cpp
)

target_include_directories(taskmesh_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(taskmesh_core PUBLIC
    taskmesh_warnings
    sqlite3
)

target_compile_features(taskmesh_core PUBLIC cxx_std_20)

# CLI Application
add_executable(taskmesh_cli
    src/main.cpp
)

target_link_libraries(taskmesh_cli PRIVATE
    taskmesh_core
)

target_compile_features(taskmesh_cli PRIVATE cxx_std_20)

# Test Suite
if(TASKMESH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
