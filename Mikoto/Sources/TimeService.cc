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
#include <EASTL/ratio.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Service.hh>
#include <Core/TimeService.hh>

#include <Logging/Logger.hh>

namespace mikoto::core {

    auto Time::GetUnitString() const -> eastl::string_view {
        return GetUnitString( this->mUnit );
    }

    auto Time::Convert( TimeUnit unit ) const -> double {
        if (unit == mUnit) {
            return mValue;
        }

        return Convert(this->mUnit, unit, mValue);
    }

    auto Time::GetUnitString( TimeUnit unit ) -> eastl::string_view {
        switch (unit) {
            case TimeUnit::eSeconds: return "s";
            case TimeUnit::eMilliseconds: return "ms";
            case TimeUnit::eMicroseconds: return "us";
            case TimeUnit::eNanoseconds: return "ns";
        }

        return "Unknown";
    }

    auto Time::Convert( TimeUnit src, TimeUnit dst, double value ) -> double {
        // Convert source -> seconds
        double seconds{};

        switch ( src ) {
            case TimeUnit::eSeconds:
                seconds = value;
                break;
            case TimeUnit::eMilliseconds:
                seconds = value / 1'000.0;
                break;
            case TimeUnit::eMicroseconds:
                seconds = value / 1'000'000.0;
                break;
            case TimeUnit::eNanoseconds:
                seconds = value / 1'000'000'000.0;
                break;
        }

        // Convert seconds -> destination
        switch ( dst ) {
            case TimeUnit::eSeconds:
                return seconds;
            case TimeUnit::eMilliseconds:
                return seconds * 1'000.0;
            case TimeUnit::eMicroseconds:
                return seconds * 1'000'000.0;
            case TimeUnit::eNanoseconds:
                return seconds * 1'000'000'000.0;
        }

        return seconds; // fallback
    }

    auto TimeServiceCreateInfo::WithDefaultUnit( const TimeUnit unit ) -> TimeServiceCreateInfo & {
        this->mDefaultUnit = unit;
        return *this;
    }

    TimeService::TimeService( const TimeServiceCreateInfo &option )
        : mDefaultUnits{ option.mDefaultUnit } {}

    auto TimeService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing TimeService..." );

        mInitTimePoint = Clock::now();

        mIsInitialized = true;
    }

    auto TimeService::Tick() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const auto now{ Clock::now() };
        mTimeStep = eastl::chrono::duration_cast<Sec>( now - mLastFrameTime ).count();
        mLastFrameTime = now;
    }

    auto TimeService::PushTime( const eastl::string& tag, Time time ) -> void {
        constexpr size_t kMaxSamples{ 120 }; // ~2 seconds at 60fps

        auto& history { mTimeMap[tag] };

        history.push_back(time);

        if (history.size() > kMaxSamples) {
            history.erase(history.begin()); // simple, fine for now
        }
    }

    auto TimeService::GetDefaultUnit() const -> TimeUnit {
        return mDefaultUnits;
    }

    auto TimeService::GetTimeStep( const TimeUnit unit ) const -> double {
        switch (unit) {
            case TimeUnit::eSeconds:
                return mTimeStep;
            case TimeUnit::eMilliseconds:
                return mTimeStep * kMilliPerSeconds;
            case TimeUnit::eMicroseconds:
                return mTimeStep * kMicroPerSeconds;
            case TimeUnit::eNanoseconds:
                return mTimeStep * kNanoPerSeconds;
        }

        return mTimeStep;
    }

    auto TimeService::GetTime( const TimeUnit unit ) const -> double {
        switch (unit) {
            case TimeUnit::eSeconds:
                return eastl::chrono::duration_cast<Sec>( Clock::now() - mInitTimePoint ).count();
            case TimeUnit::eMilliseconds:
                return eastl::chrono::duration_cast<Milli>( Clock::now() - mInitTimePoint ).count();
            case TimeUnit::eMicroseconds:
                return eastl::chrono::duration_cast<Micro>( Clock::now() - mInitTimePoint ).count();
            case TimeUnit::eNanoseconds:
                return eastl::chrono::duration_cast<Nano>( Clock::now() - mInitTimePoint ).count();
        }

        return eastl::chrono::duration_cast<Sec>( Clock::now() - mInitTimePoint ).count();
    }

    auto TimeService::ToString( double time, const TimeUnit unit ) -> eastl::string {
        using namespace eastl::literals::chrono_literals;

        if (unit != TimeUnit::eSeconds) time = TransformToSeconds( time, unit );

        std::chrono::hours hours{ as<ull>( time / kSecPerHour ) };
        std::chrono::minutes minutes{ ( as<ull>( time ) % as<ull>( kSecPerHour ) / kSecPerMin ) };
        std::chrono::seconds seconds{ ( as<ull>( time ) % as<ull>( kSecPerHour ) % kSecPerMin ) };

        return string::ToEA_Stl( fmt::format( "{:%H:%M:%S}", hours + minutes + seconds ) );
    }

    auto TimeService::TransformToSeconds( const double time, const TimeUnit unit ) -> double {
        switch (unit) {
            case TimeUnit::eMilliseconds:
                return time / kMilliPerSeconds;
            case TimeUnit::eMicroseconds:
                return time / kMicroPerSeconds;
            case TimeUnit::eNanoseconds:
                return time / kNanoPerSeconds;
            default:
                return time;
        }
    }

    auto TimeService::Shutdown() -> void {
        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down TimeService..." );
    }
}// namespace Mikoto