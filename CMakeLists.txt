cmake_minimum_required(VERSION 3.22)

set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

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
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

# ===========================
# Platform-specific settings
# ===========================
if(WIN32)
    # Static MSVC runtime
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()

    #add_compile_options(/W4 /fsanitize=address)
    #add_link_options(/fsanitize=address)

    set(PYTHON_EXECUTABLE "python")
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)

else()
    set(PYTHON_EXECUTABLE "python3")
    add_compile_options(-fsanitize=address -D_DEBUG -Wall -Wextra -Wpedantic)
    add_link_options(-fsanitize=address)
endif()

# ===========================
# Add engine and editor
# ===========================
add_subdirectory(Mikoto)        # Core engine
add_subdirectory(Mikoto-Editor) # Editor
add_subdirectory(Mikoto-Sandbox) # Sandbox
