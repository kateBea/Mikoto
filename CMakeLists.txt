CMAKE_MINIMUM_REQUIRED(VERSION 3.18)

PROJECT(Mikoto)

# CXX Lang Requirements
SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

IF(WIN32)
    # Compile commands for Windows
    ADD_COMPILE_DEFINITIONS(/W4)
    SET(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)

ELSE()
    # Compile commands for Linux
    ADD_COMPILE_OPTIONS(-fsanitize=address -D_DEBUG -Wall -Wextra -Wpedantic)

    # Sanitize
    ADD_LINK_OPTIONS(-fsanitize=address)
ENDIF()


ADD_SUBDIRECTORY(Mikoto)
ADD_SUBDIRECTORY(Mikoto-Editor)
ADD_SUBDIRECTORY(Mikoto-Sandbox)