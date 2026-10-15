include(FetchContent)

# Pin reproducible GoogleTest release v1.15.2
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)

# Ensure GoogleTest uses the same MSVC runtime CRT settings as the parent project
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(taskmesh_tests
    job_tests.cpp
    priority_scheduler_tests.cpp
    job_executor_tests.cpp
    concurrent_queue_tests.cpp
    execution_engine_tests.cpp
    job_store_tests.cpp
)

target_link_libraries(taskmesh_tests PRIVATE
    taskmesh_core
    GTest::gtest_main
)

target_compile_features(taskmesh_tests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(taskmesh_tests)
