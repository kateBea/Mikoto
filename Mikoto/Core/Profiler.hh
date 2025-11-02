//
// Created by kate on 11/1/25.
//

#ifndef MIKOTO_PROFILER_HH
#define MIKOTO_PROFILER_HH

#include <string_view>

#include <tracy/Tracy.hpp>

#include <Logging/Assert.hh>

namespace Mikoto {

#if !defined(NDEBUG)
#define MKT_BEGIN_PROFILER(name)  ZoneScopedNS( name, 30 );
#define MKT_BEGIN_PROFILER_NAMED()  ZoneScopedNS( __PRETTY_FUNCTION__ , 30 );
#define MKT_END_PROFILER()   EndProfilingZone()
#else
#define MKT_BEGIN_PROFILER(name)
#define MKT_END_PROFILER()
#endif



} // Miktoto

#endif //
