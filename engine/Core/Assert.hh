/**
 * Assert.hh
 * Created by kate on 5/25/23.
 * */

#ifndef KATE_ENGINE_ASSERT_HH
#define KATE_ENGINE_ASSERT_HH

// C++ Standard Library
#include <cstdlib>

// Project Headers
#include <Utility/Common.hh>

#if defined(_WIN32) || defined(_WIN64)
    #define __PRETTY_FUNCTION__  __FUNCTION__
#endif

#if defined(NDEBUG) || defined(_DEBUG)
    #define KT_ENABLE_ASSERTIONS
#else
    #undef KT_ENABLE_ASSERTIONS
#endif

#if defined(KT_ENABLE_ASSERTIONS)

/**
 * Print __MESSAGE and abort program execution if __EXPR evaluates to false
 */
#define KT_ASSERT(__EXPR, __MESSAGE)                                      \
    do {                                                                   \
        if (!(__EXPR)) {                                                   \
            KT_COLOR_PRINT_FORMATTED(KT_FMT_COLOR_RED, "MESSAGE: {}\n"   \
                                                      "FUNCTION: {}\n"     \
                                                      "SRC: {}\n"          \
                                                      "LINE: {}\n",        \
                                     __MESSAGE, __PRETTY_FUNCTION__,       \
                                     __FILE__, __LINE__);                  \
            std::abort();                                                  \
        }                                                                  \
    } while (false)

#define KT_ASSERT_EXPR(__EXPR)                                                     \
    do {                                                                   \
        if (!(__EXPR)) {                                                   \
            KT_COLOR_PRINT_FORMATTED(KT_FMT_COLOR_RED, "Condition: {} failed\n"   \
                                                      "FUNCTION: {}\n"     \
                                                      "SRC: {}\n"          \
                                                      "LINE: {}\n",        \
                                     #__EXPR, __PRETTY_FUNCTION__,       \
                                     __FILE__, __LINE__);                  \
            std::abort();                                                  \
        }                                                                  \
    } while (false)
#else
    #define KT_ASSERT(__EXPR, __MESSAGE)
    #define KT_ASSERT_EXPR(__EXPR)
#endif

// TODO: add static assert


#endif //KATE_ENGINE_ASSERT_HH
