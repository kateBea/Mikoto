/**
 * Common.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_COMMON_HH
#define MIKOTO_COMMON_HH

// C++ Standard Libraries
#include <filesystem>
#include <fstream>
#include <string>
#include <array>

#include <fmt/format.h>

// Project Headers

#define MKT_NODISCARD [[nodiscard]]
#define MKT_UNUSED_FUNC [[maybe_unused]]
#define MKT_UNUSED_VAR [[maybe_unused]]

// Set bit specified by the argument
#define BIT_SET(N)              (1 << N)

// Stringify
#define MKT_STRINGIFY(x) #x

#define MKT_IS_NULL(PTR) PTR == nullptr
#define MKT_IS_NOT_NULL(PTR) PTR != nullptr

// Engine version
#define MKT_ENGINE_VERSION_MAJOR 1
#define MKT_ENGINE_VERSION_MINOR 0
#define MKT_ENGINE_VERSION_PATCH 0

#define MKT_THROW_RUNTIME_ERROR(MESSAGE) \
    throw std::runtime_error(fmt::format("Message: {}\n@File: {}\n@Line: {}", MESSAGE, __FILE__, __LINE__))

/**
 * Disable copy constructor and operator, move constructor
 * and operator for CLASS_NAME
 * */
#define DISABLE_COPY_AND_MOVE_FOR(CLASS_NAME)       \
    CLASS_NAME(const CLASS_NAME&)       = delete;   \
    auto operator=(const CLASS_NAME&)   = delete;   \
    CLASS_NAME(CLASS_NAME&&)            = delete;   \
    auto operator=(CLASS_NAME&&)        = delete

/**
 * Disable COPY constructor and operator for CLASS_NAME
 * */
#define DISABLE_COPY_FOR(CLASS_NAME)                 \
    CLASS_NAME(const CLASS_NAME&)       = delete;   \
    auto operator=(const CLASS_NAME&)   = delete

#endif // MIKOTO_COMMON_HH
