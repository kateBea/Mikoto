cmake_minimum_required(VERSION 3.22)

cmake_policy(SET CMP0091 NEW)

option(MIKOTO_BUILD_TESTS "Build Mikoto unit tests" ON)

# Disable IPO globally
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

# Force disable for all targets (even if subprojects enable it)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set_property(GLOBAL PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)

project(Mikoto
        VERSION 1.0
        LANGUAGES CXX
)

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
if(WIN32)
    # Static MSVC runtime
    message("Target platform Windows")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

        # Propagate CRT type to all sub-targets
        set_property(GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()

    add_compile_options(/MP)

    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)

elseif(LINUX)
    message("Target platform Linux")

    # Easiest for now to run valgrind tests
    #add_compile_options(-fsanitize=address -g3 -D_DEBUG -Wall -Wextra -Wpedantic )
    #add_link_options(-fsanitize=address)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g3 -D_GLIBCXX_DEBUG -Wall -Wextra -Wpedantic)
    else()
        add_compile_options(-O3 -Wall -Wextra -Wpedantic)
    endif()
endif()

# ===========================
# Add engine and applications
# ===========================
#add_subdirectory(Mikoto-Sandbox)
add_subdirectory(Mikoto-Apps)
#add_subdirectory(Mikoto-Editor)

# Important to only enable the target you want to build.
# For the time being this has to be done manually here

# Mikoto added last so it builds with target
# specified configurations.
add_subdirectory(Mikoto)

if (MIKOTO_BUILD_TESTS)
    enable_testing()         # <-- recommended but not specific to GoogleTest
    add_subdirectory(Mikoto-Tests)
endif()
