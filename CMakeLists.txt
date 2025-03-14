CMAKE_MINIMUM_REQUIRED(VERSION 3.18)

PROJECT(Mikoto)

# CXX Lang Requirements
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

IF(WIN32)
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


ADD_SUBDIRECTORY(Mikoto)
ADD_SUBDIRECTORY(Mikoto-Editor)
ADD_SUBDIRECTORY(Mikoto-Sandbox)