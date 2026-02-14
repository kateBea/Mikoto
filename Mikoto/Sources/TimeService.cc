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


#include <Common/Service.hh>
#include <Core/Profiler.hh>
#include <Core/TimeService.hh>
#include <Logging/Logger.hh>

namespace Mikoto {

    auto Time::GetUnitString() const -> std::string_view {
        return GetUnitString( this->Unit );
    }

    auto Time::Convert( TimeUnit unit ) const -> double {
        if (unit == Unit) {
            return Value;
        }

        return Convert(this->Unit, unit, Value);
    }

    auto Time::GetUnitString( TimeUnit unit ) -> std::string_view {
        switch (unit) {
            case TimeUnit::SECONDS: return "s";
            case TimeUnit::MILLISECONDS: return "ms";
            case TimeUnit::MICROSECONDS: return "us";
            case TimeUnit::NANOSECONDS: return "ns";
        }

        return "Unknown";
    }

    auto Time::Convert( TimeUnit src, TimeUnit dst, double value ) -> double {
        // Convert source -> seconds
        double seconds{};

        switch ( src ) {
            case TimeUnit::SECONDS:
                seconds = value;
                break;
            case TimeUnit::MILLISECONDS:
                seconds = value / 1'000.0;
                break;
            case TimeUnit::MICROSECONDS:
                seconds = value / 1'000'000.0;
                break;
            case TimeUnit::NANOSECONDS:
                seconds = value / 1'000'000'000.0;
                break;
        }

        // Convert seconds -> destination
        switch ( dst ) {
            case TimeUnit::SECONDS:
                return seconds;
            case TimeUnit::MILLISECONDS:
                return seconds * 1'000.0;
            case TimeUnit::MICROSECONDS:
                return seconds * 1'000'000.0;
            case TimeUnit::NANOSECONDS:
                return seconds * 1'000'000'000.0;
        }

        return seconds; // fallback
    }

    auto TimeServiceCreateInfo::WithDefaultUnit( const TimeUnit unit ) -> TimeServiceCreateInfo & {
        this->DefaultUnit = unit;

        return *this;
    }

    TimeService::TimeService( const TimeServiceCreateInfo &option )
        : m_DefaultUnits{ option.DefaultUnit } {}

    auto TimeService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing TimeService..." );

        m_InitTimePoint = Clock::now();

        m_IsInitialized = true;
    }

    auto TimeService::Tick() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        const auto now{ Clock::now() };
        m_TimeStep = std::chrono::duration_cast<Sec>( now - m_LastFrameTime ).count();
        m_LastFrameTime = now;
    }

    auto TimeService::GetDefaultUnit() const -> TimeUnit { return m_DefaultUnits; }

    auto TimeService::GetTimeStep( const TimeUnit unit ) const -> double {
        switch (unit) {
            case TimeUnit::SECONDS:
                return m_TimeStep;
            case TimeUnit::MILLISECONDS:
                return m_TimeStep * MILLISECONDS_PER_SECOND;
            case TimeUnit::MICROSECONDS:
                return m_TimeStep * MICROSECONDS_PER_SECOND;
            case TimeUnit::NANOSECONDS:
                return m_TimeStep * NANOSECONDS_PER_SECOND;
        }

        return m_TimeStep;
    }

    auto TimeService::GetTime( const TimeUnit unit ) const -> double {
        switch (unit) {
            case TimeUnit::SECONDS:
                return std::chrono::duration_cast<Sec>( Clock::now() - m_InitTimePoint ).count();
            case TimeUnit::MILLISECONDS:
                return std::chrono::duration_cast<Milli>( Clock::now() - m_InitTimePoint ).count();
            case TimeUnit::MICROSECONDS:
                return std::chrono::duration_cast<Micro>( Clock::now() - m_InitTimePoint ).count();
            case TimeUnit::NANOSECONDS:
                return std::chrono::duration_cast<Nano>( Clock::now() - m_InitTimePoint ).count();
        }

        return std::chrono::duration_cast<Sec>( Clock::now() - m_InitTimePoint ).count();
    }

    auto TimeService::ToString( double time, const TimeUnit unit ) -> std::string {
        using namespace std::literals::chrono_literals;

        if (unit != TimeUnit::SECONDS) time = TransformToSeconds( time, unit );

        std::chrono::hours hours{ static_cast<ULongLong>( time / SECONDS_PER_HOUR ) };
        std::chrono::minutes minutes{ ( static_cast<ULongLong>( time ) % static_cast<ULongLong>( SECONDS_PER_HOUR ) / SECONDS_PER_MINUTE ) };
        std::chrono::seconds seconds{ ( static_cast<ULongLong>( time ) % static_cast<ULongLong>( SECONDS_PER_HOUR ) % SECONDS_PER_MINUTE ) };

        return fmt::format( "{:%H:%M:%S}", hours + minutes + seconds );
    }

    auto TimeService::TransformToSeconds( const double time, const TimeUnit unit ) -> double {
        switch (unit) {
            case TimeUnit::MILLISECONDS:
                return time / MILLISECONDS_PER_SECOND;
            case TimeUnit::MICROSECONDS:
                return time / MICROSECONDS_PER_SECOND;
            case TimeUnit::NANOSECONDS:
                return time / NANOSECONDS_PER_SECOND;
            default:
                return time;
        }
    }

    auto TimeService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down TimeService..." );
    }

}// namespace Mikoto