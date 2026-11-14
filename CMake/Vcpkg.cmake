# Configure before project(), when the vcpkg toolchain installs the manifest.
set(VCPKG_MANIFEST_MODE ON CACHE BOOL "Use Mikoto's dependency manifest" FORCE)

# The Visual Studio vcpkg distribution nests buildtrees under vcpkg_installed.
# MSVC can exceed MAX_PATH there even with CMake's object-path hashing.
# Keep intermediates short and isolate different build directories/presets.
if(CMAKE_HOST_WIN32)
    string(SHA256 mikoto_vcpkg_build_id "${CMAKE_BINARY_DIR}")
    string(SUBSTRING "${mikoto_vcpkg_build_id}" 0 8 mikoto_vcpkg_build_id)
    set(MIKOTO_VCPKG_BUILD_ROOT
        "${CMAKE_SOURCE_DIR}/vcpkg/${mikoto_vcpkg_build_id}"
        CACHE PATH "Short directory for vcpkg intermediate builds")
    set(mikoto_has_buildtrees_override OFF)
    foreach(mikoto_install_option IN LISTS VCPKG_INSTALL_OPTIONS)
        if(mikoto_install_option MATCHES "^--x-buildtrees-root(=|$)")
            set(mikoto_has_buildtrees_override ON)
        endif()
    endforeach()
    if(NOT mikoto_has_buildtrees_override)
        list(APPEND VCPKG_INSTALL_OPTIONS
            "--x-buildtrees-root=${MIKOTO_VCPKG_BUILD_ROOT}")
    endif()
endif()

list(APPEND VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_LIST_DIR}/Triplets")
list(REMOVE_DUPLICATES VCPKG_OVERLAY_TRIPLETS)

if(NOT VCPKG_TARGET_TRIPLET)
    if(CMAKE_HOST_WIN32)
        set(VCPKG_TARGET_TRIPLET x64-windows-mikoto CACHE STRING "vcpkg target")
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(VCPKG_TARGET_TRIPLET x64-linux-mikoto CACHE STRING "vcpkg target")
    endif()
endif()

if(CMAKE_HOST_WIN32 AND NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows-mikoto")
    message(FATAL_ERROR "Use -DVCPKG_TARGET_TRIPLET=x64-windows-mikoto for the Windows build. Mikoto requires the static CRT and dependency settings defined by this triplet.")
endif()

# Slang is an optional manifest feature so SDK mode does not download/build it.
if(MIKOTO_USE_PRECOMPILED_SLANG)
    list(REMOVE_ITEM VCPKG_MANIFEST_FEATURES source-slang)
else()
    list(APPEND VCPKG_MANIFEST_FEATURES source-slang)
endif()

if(MIKOTO_ENABLE_TRACY_PROFILING)
    list(APPEND VCPKG_MANIFEST_FEATURES profiling)
endif()
list(REMOVE_DUPLICATES VCPKG_MANIFEST_FEATURES)
