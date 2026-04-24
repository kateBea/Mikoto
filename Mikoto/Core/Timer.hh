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

#ifndef MIKOTO_TIMER_HH
#define MIKOTO_TIMER_HH

#include <chrono>

#include <EASTL/chrono.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/TimeService.hh>

namespace mikoto::core {

    class Timer final {
    public:
        explicit Timer(bool printOnExit);
        Timer(eastl::string_view scopeName, eastl::string_view startMessage, bool showStartMessage = false);

        MKT_NODISCARD auto GetCurrentProgress(TimeUnit defaultUnit = TimeUnit::eSeconds) const -> double;
        auto Restart() -> void;

        ~Timer();

    private:
        using Nano = eastl::chrono::duration<double, eastl::ratio<1, 1000000000>>;
        using Micro = eastl::chrono::duration<double, eastl::ratio<1, 1000000>>;
        using Milli = eastl::chrono::duration<double, eastl::ratio<1, 1000>>;
        using Sec = eastl::chrono::duration<double, eastl::ratio<1, 1>>;

        using Clock = eastl::chrono::high_resolution_clock;
        using TimePoint = eastl::chrono::time_point<Clock>;

    private:
        eastl::string m_ScopeName{};
        TimePoint m_TimeSinceStart{};
        bool m_PrintOnExit{ true };
    };
}

#if !defined(NDEBUG)
    #define MKT_PROFILE_SCOPE()  Timer _Timer{ __PRETTY_FUNCTION__, string::Format("[START] Profiling: {}", __PRETTY_FUNCTION__ ) }
    #define MKT_PROFILE_SCOPE_MARKED(MARK)  Timer _Timer{ MARK, string::Format("[START] Profiling: {}. Mark {}", __PRETTY_FUNCTION__, MARK ) }
#else
    #define MKT_PROFILE_SCOPE()
    #define MKT_PROFILE_SCOPE_MARKED()
#endif


#endif // MIKOTO_TIMER_HH
