cmake_minimum_required(VERSION 3.22)

project(Mikoto
        VERSION 1.0
        LANGUAGES CXX
)

option(MIKOTO_BUILD_TESTS "Build Mikoto unit tests" ON)

# ===========================
# C++ language requirements
# ===========================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CPP_RTTI_ENABLED ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# ===========================
# Platform-specific settings
# ===========================
if(MSVC)
    message("Target platform Windows")

    # Compile multithreaded
    add_compile_options(/MP)

elseif(LINUX)
    message("Target platform Linux")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g3 -D_GLIBCXX_DEBUG -Wall)
    else()
        add_compile_options(-O3 -Wall)
    endif()
endif()

# ===========================
# Add engine and applications
# ===========================
#add_subdirectory(Mikoto-Sandbox)
#add_subdirectory(Mikoto-Apps)
add_subdirectory(Mikoto-Editor)

# Mikoto added last so it builds with target
# specified configurations.
add_subdirectory(Mikoto)

if (MIKOTO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Mikoto-Tests)
endif()
