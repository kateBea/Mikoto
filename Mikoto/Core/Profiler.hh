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

#ifndef MIKOTO_CORE_PROFILER_HH
#define MIKOTO_CORE_PROFILER_HH

#if defined( MIKOTO_ENABLE_TRACY )
#include <tracy/Tracy.hpp>

#ifndef MIKOTO_TRACY_CALLSTACK_DEPTH
#define MIKOTO_TRACY_CALLSTACK_DEPTH 0
#endif

// Tracy records the function, file and line without walking the native stack.
// Scoped zones close on every return/exception; on-demand mode handles reconnects.
// Keep one function zone per scope. Use a nested scope for a named sub-operation.
// Profile work at its callers so hot lookups and atomic operations do not flood the trace.
#define MKT_BEGIN_PROFILER()                ZoneScopedS( MIKOTO_TRACY_CALLSTACK_DEPTH )
#define MKT_PROFILE_NAMED( name )           ZoneScopedNS( name, MIKOTO_TRACY_CALLSTACK_DEPTH )
#define MKT_PROFILE_ZONE_NAME( data, size ) ZoneName( data, size )
#define MKT_PROFILE_ZONE_TEXT( data, size ) ZoneText( data, size )
#define MKT_PROFILE_FRAME()                 FrameMark
#define MKT_PROFILE_THREAD( name )          tracy::SetThreadName( name )
#else
// Disabled builds do not format strings, allocate timers, or log each call.
// Arguments are not evaluated, even in Debug builds.
#define MKT_BEGIN_PROFILER()
#define MKT_PROFILE_NAMED( name )
#define MKT_PROFILE_ZONE_NAME( data, size )
#define MKT_PROFILE_ZONE_TEXT( data, size )
#define MKT_PROFILE_FRAME()
#define MKT_PROFILE_THREAD( name )
#endif

// Preserve existing instrumentation while exposing the shorter public spelling.
#define MKT_BEGIN_PROFILER_NAMED() MKT_BEGIN_PROFILER()

#endif// MIKOTO_CORE_PROFILER_HH
