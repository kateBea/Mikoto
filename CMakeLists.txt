CMAKE_MINIMUM_REQUIRED(VERSION 3.22)

PROJECT(Mikoto)

# CXX Lang Requirements
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

IF(WIN32)
    # Ensure that CMake uses static MSVC runtime library
    # JoltUses this so we just enforce evrything else to use tghe same
    # https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        SET(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        SET(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()

    # Set the python interpreter path
    SET(PYTHON_EXECUTABLE "python")

    # Compile commands for Windows
    ADD_COMPILE_DEFINITIONS(/W4)
    SET(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)

ELSE()
    # Set the python interpreter path
    SET(PYTHON_EXECUTABLE "python3")

    # Compile commands for Linux
    ADD_COMPILE_OPTIONS(-fsanitize=address -D_DEBUG -Wall -Wextra -Wpedantic)

    # Sanitize
    ADD_LINK_OPTIONS(-fsanitize=address)
ENDIF()

if((NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.bootstrap.json"))
    MESSAGE(DEBUG "Clonning repositories into Third-Party")

    execute_process(
            COMMAND ${PYTHON_EXECUTABLE} "bootstrap.py"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE bootstrap_result
    )
    if (NOT (${bootstrap_result} EQUAL 0))
        message(FATAL_ERROR "Cannot run bootstrap.py")
    endif()
endif()

SET(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
SET(CPP_RTTI_ENABLED ON)

ADD_SUBDIRECTORY(Mikoto)
ADD_SUBDIRECTORY(Mikoto-Editor)
ADD_SUBDIRECTORY(Mikoto-Sandbox)