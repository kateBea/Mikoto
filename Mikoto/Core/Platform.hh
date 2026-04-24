//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_PLATFORM_H
#define MIKOTO_PLATFORM_H

// =======================
//  Operating System
// =======================
#if defined(_WIN32) || defined(_WIN64)
    #define MIKOTO_PLATFORM_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define MIKOTO_PLATFORM_MACOS 1
    #elif TARGET_OS_IPHONE || TARGET_OS_IPHONE_SIMULATOR
        #define MIKOTO_PLATFORM_IOS 1
    #else
        #error "Unsupported Apple platform"
    #endif
#elif defined(__ANDROID__)
    #define MIKOTO_PLATFORM_ANDROID 1
#elif defined(__linux__)
    #define MIKOTO_PLATFORM_LINUX 1
#else
    #error "Unknown or unsupported platform"
#endif

// =======================
//  Architecture
// =======================
#if defined(_M_X64) || defined(__x86_64__)
    #define MIKOTO_ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__)
    #define MIKOTO_ARCH_X86 1
#elif defined(_M_ARM64) || defined(__aarch64__)
    #define MIKOTO_ARCH_ARM64 1
#elif defined(_M_ARM) || defined(__arm__)
    #define MIKOTO_ARCH_ARM 1
#else
    #error "Unsupported CPU architecture"
#endif

// =======================
//  Compiler
// =======================
#if defined(_MSC_VER)
    #define MIKOTO_COMPILER_MSVC 1
#elif defined(__clang__)
    #define MIKOTO_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define MIKOTO_COMPILER_GCC 1
#else
    #error "Unsupported compiler"
#endif

// =======================
//  Build Configuration
// =======================
#if !defined(NDEBUG)
    #define MIKOTO_DEBUG 1
#else
    #define MIKOTO_RELEASE 1
#endif

// =======================
//  Export / Import API
// =======================
// Need to define MIKOTO_BUILD in build system (e.g. CMake) when building Mikoto itself.
// Clients that use Mikoto should NOT define it.

#if defined(MIKOTO_PLATFORM_WINDOWS)
    #if defined(MIKOTO_BUILD)
        #define MIKOTO_API __declspec(dllexport)
    #else
        #define MIKOTO_API __declspec(dllimport)
    #endif
#else
    #if defined(MIKOTO_BUILD)
        #define MIKOTO_API __attribute__((visibility("default")))
    #else
        #define MIKOTO_API
    #endif
#endif

// =======================
//  Endianness
// =======================
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define MIKOTO_BIG_ENDIAN 1
#else
    #define MIKOTO_LITTLE_ENDIAN 1
#endif

#endif//MIKOTO_PLATFORM_H
