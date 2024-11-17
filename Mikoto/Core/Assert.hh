/**
 * Assert.hh
 * Created by kate on 5/25/23.
 * */

#ifndef MIKOTO_ASSERT_HH
#define MIKOTO_ASSERT_HH

// C++ Standard Library
#include <cstdlib>

// Third-Party Libraries
#include "fmt/format.h"

// Project Headers
#include "Common/Common.hh"

#if defined(_WIN32) || defined(_WIN64)
    #define __PRETTY_FUNCTION__  __FUNCTION__
#endif

#if !defined(NDEBUG) || defined(_DEBUG)
    #define MKT_ENABLE_ASSERTIONS
#else
    #undef MKT_ENABLE_ASSERTIONS
#endif

#if defined(MKT_ENABLE_ASSERTIONS)

    /**
     * Print __MESSAGE and abort program execution if __EXPR evaluates to false
     * */
    #define MKT_ASSERT(__EXPR, __MESSAGE)                                 \
    do {                                                                  \
        if (!(__EXPR)) {                                                  \
            MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_RED, "MESSAGE: {}\n"  \
                                                         "FUNCTION: {}\n" \
                                                         "SRC: {}\n"      \
                                                         "LINE: {}\n",    \
                                      __MESSAGE, __PRETTY_FUNCTION__,     \
                                      __FILE__, __LINE__);                \
            std::abort();                                                 \
        }                                                                 \
    } while (false)

    /**
     * Print __EXPR and abort program execution if __EXPR evaluates to false
     * */
    #define MKT_ASSERT_EXPR(__EXPR)                                                 \
    do {                                                                            \
        if (!(__EXPR)) {                                                            \
            MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_RED, "Condition: {} failed\n"   \
                                                      "FUNCTION: {}\n"              \
                                                      "SRC: {}\n"                   \
                                                      "LINE: {}\n",                 \
                                     #__EXPR, __PRETTY_FUNCTION__,                  \
                                     __FILE__, __LINE__);                           \
            std::abort();                                                           \
        }                                                                           \
    } while (false)
#else
    #define MKT_ASSERT(__EXPR, __MESSAGE)
    #define MKT_ASSERT_EXPR(__EXPR)
#endif

#endif // MIKOTO_ASSERT_HH
