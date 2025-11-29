//
// Created by zanet on 3/25/2025.
//

#include <Common/Service.hh>
#include <Core/Profiler.hh>
#include <Core/TimeService.hh>
#include <Logging/Logger.hh>
#include <tracy/Tracy.hpp>

namespace Mikoto {

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

    auto TimeService::UpdateTimeStep() -> void {
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