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

#ifndef MIKOTO_PROFILER_HH
#define MIKOTO_PROFILER_HH

#include <tracy/Tracy.hpp>
#include <Logging/Assert.hh>

namespace mikoto::core {

    // This should use Tracy ZoneMacros, must be used with tracy profiler on
    // as it leaks memory if the profiler is not running

#if defined( MIKOTO_ENABLE_TRACY )
#define MKT_BEGIN_PROFILER_NAMED() ZoneScopedNS( __PRETTY_FUNCTION__, 30 );
#else
#define MKT_BEGIN_PROFILER_NAMED()
#endif

}// namespace mikoto::core

#endif//MIKOTO_PROFILER_HH
