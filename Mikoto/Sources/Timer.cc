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

#include <chrono>

#include <EASTL/chrono.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Timer.hh>
#include <Core/String.hh>
#include <Core/TimeService.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

namespace mikoto::core {

    MKT_NODISCARD static constexpr auto GetUnitStr( const TimeUnit defaultUnit = TimeUnit::eSeconds) -> std::string_view {
        switch (defaultUnit) {
            case TimeUnit::eSeconds:         return "s";
            case TimeUnit::eMilliseconds:    return "ms";
            case TimeUnit::eMicroseconds:    return "µs";
            case TimeUnit::eNanoseconds:     return "ns";
        }

        return "s";
    }

    Timer::Timer(bool printOnExit)
        : mTimeSinceStart{ Clock::now() }, mPrintOnExit{ printOnExit }
    {}

    Timer::Timer(eastl::string_view scopeName, eastl::string_view startMessage, bool showStartMessage)
        :   mTimeSinceStart{ Clock::now() }, mScopeName{ scopeName }, mPrintOnExit{ showStartMessage }
    {
        if (showStartMessage) {
            MKT_CORE_LOGGER_DEBUG("{}", startMessage);
        }
    }

    auto Timer::GetCurrentProgress( const TimeUnit defaultUnit ) const -> double {
        switch (defaultUnit) {
            case TimeUnit::eSeconds:         return eastl::chrono::duration_cast<Sec>(Clock::now() - mTimeSinceStart).count();
            case TimeUnit::eMilliseconds:    return eastl::chrono::duration_cast<Milli>(Clock::now() - mTimeSinceStart).count();
            case TimeUnit::eMicroseconds:    return eastl::chrono::duration_cast<Micro>(Clock::now() - mTimeSinceStart).count();
            case TimeUnit::eNanoseconds:     return eastl::chrono::duration_cast<Nano>(Clock::now() - mTimeSinceStart).count();
        }

        return eastl::chrono::duration_cast<Sec>(Clock::now() - mTimeSinceStart).count();;
    }

    auto Timer::Restart() -> void {
        mTimeSinceStart = Clock::now();
    }

    Timer::~Timer() {
        if (mPrintOnExit) {
            const auto units{ TimeUnit::eSeconds };
            MKT_CORE_LOGGER_DEBUG("[END] Profiling Scope {}. Elapsed {:.10f} {}", mScopeName, GetCurrentProgress(units), GetUnitStr(units));
        }

        // FIXME:
        // if (TimeService::GetPtr()) {
        //     TimeService::Get()->PushTime( mScopeName, Time {
        //         .mValue = GetCurrentProgress(TimeUnit::eSeconds),
        //         .mUnit = TimeUnit::eSeconds,
        //     } );
        // }
    }
}
