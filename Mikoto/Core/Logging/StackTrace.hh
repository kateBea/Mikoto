//
// Created by zanet on 2/1/2025.
//

#ifndef STACKTRACE_HH
#define STACKTRACE_HH

#include <cpptrace/cpptrace.hpp>


#if !defined(NDEBUG) || defined(_DEBUG)
    #define MKT_STACK_TRACE() cpptrace::generate_trace().print()
#else
    #define MKT_STACK_TRACE()
#endif

namespace Mikoto {


}// namespace Mikoto

#endif//STACKTRACE_HH
