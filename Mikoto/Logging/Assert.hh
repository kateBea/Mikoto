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

#ifndef MIKOTO_ASSERT_HH
#define MIKOTO_ASSERT_HH

// C++ Standard Library
#include <cstdlib>

// Third-Party Libraries
#include <fmt/format.h>
#include <cpptrace/cpptrace.hpp>


// Project Headers
#include <Library/String/String.hh>

#if defined( WIN32 ) || defined( WIN64 )
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#if !defined( NDEBUG )
#define MKT_ENABLE_ASSERTIONS
#else
#undef MKT_ENABLE_ASSERTIONS
#endif

#if defined( MKT_ENABLE_ASSERTIONS )

/**
     * Print __MESSAGE and abort program execution if __EXPR evaluates to false
     * */
#define MKT_ASSERT( __EXPR, __MESSAGE )                                \
    do {                                                               \
        if ( !( __EXPR ) ) {                                           \
            cpptrace::generate_trace().print();                        \
            MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_RED,              \
                                       "MESSAGE: {}\n"                 \
                                       "FUNCTION: {}\n"                \
                                       "SRC: {}\n"                     \
                                       "LINE: {}\n",                   \
                                       __MESSAGE, __PRETTY_FUNCTION__, \
                                       __FILE__, __LINE__ );           \
            cpptrace::generate_trace().print();                        \
            std::abort();                                              \
        }                                                              \
    } while ( false )

/**
     * Print __EXPR and abort program execution if __EXPR evaluates to false
     * */
#define MKT_ASSERT_EXPR( __EXPR )                                    \
    do {                                                             \
        if ( !( __EXPR ) ) {                                         \
            MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_RED,            \
                                       "Condition: {} failed\n"      \
                                       "FUNCTION: {}\n"              \
                                       "SRC: {}\n"                   \
                                       "LINE: {}\n",                 \
                                       #__EXPR, __PRETTY_FUNCTION__, \
                                       __FILE__, __LINE__ );         \
            cpptrace::generate_trace().print();                      \
            std::abort();                                            \
        }                                                            \
    } while ( false )

#define MKT_STATIC_ASSERT( __EXPR, __MESSAGE ) static_assert( __EXPR, __MESSAGE )

#else
#define MKT_ASSERT( __EXPR, __MESSAGE )
#define MKT_ASSERT_EXPR( __EXPR )
#define MKT_STATIC_ASSERT( __EXPR, __MESSAGE )
#endif

#endif// MIKOTO_ASSERT_HH
