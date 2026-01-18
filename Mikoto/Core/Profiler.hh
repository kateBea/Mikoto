//
// Created by kate on 11/1/25.
//

#ifndef MIKOTO_PROFILER_HH
#define MIKOTO_PROFILER_HH

#include <string_view>

#include <tracy/Tracy.hpp>

#include <Logging/Assert.hh>

namespace Mikoto {

// This should use Tracy ZoneMacros, temporarily disabled as it leaks memory if
// the profiler is not running
#if !defined(NDEBUG)
#define MKT_BEGIN_PROFILER_NAMED() /*ZoneScopedNS( __PRETTY_FUNCTION__ , 30 );*/
#else
#define MKT_BEGIN_PROFILER_NAMED()
#endif



} // Miktoto

#endif //
