cmake_minimum_required(VERSION 3.22)

project(MikotoTests LANGUAGES CXX)

# ======================================
# Language requirements
# ======================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ================================
# Fetch GoogleTest
# ================================
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG main
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ================================
# Gather ALL .cc files as tests
# ================================
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cc
)

add_executable(MikotoTests ${TEST_SOURCES})

target_link_libraries(MikotoTests PRIVATE
        Mikoto
        GTest::gtest
        GTest::gtest_main
)

target_include_directories(${PROJECT_NAME} PUBLIC .)
target_include_directories(${PROJECT_NAME} PUBLIC
        ${googletest_SOURCE_DIR}
)

# ================================
# Register tests with CTest
# ================================
include(CTest)
enable_testing()
add_test(NAME MikotoTests COMMAND MikotoTests)
