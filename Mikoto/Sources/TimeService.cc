//
// Created by zanet on 3/25/2025.
//

#include <Common/Service.hh>
#include <Timing/TimeService.hh>

namespace Mikoto {

    auto TimeServiceCreateInfo::WithDefaultUnit( const TimeUnit unit ) -> TimeServiceCreateInfo & {
        this->DefaultUnit = unit;

        return *this;
    }

    TimeService::TimeService( const TimeServiceCreateInfo& option )
        : m_DefaultUnits{ option.DefaultUnit } {
    }

    auto TimeService::Init() -> void {
        m_InitTimePoint = Clock_T::now();

        m_IsInitialized = true;
    }

    auto TimeService::Update() -> void {
        const auto now{ Clock_T::now() };
        m_TimeStep = std::chrono::duration_cast<Sec_T>(now - m_LastFrameTime).count();
        m_LastFrameTime = now;
    }

    auto TimeService::GetDefaultUnit() const -> TimeUnit { return m_DefaultUnits; }

    auto TimeService::GetTimeStep( const TimeUnit unit ) const -> double {
        switch (unit) {
            case TimeUnit::SECONDS:         return m_TimeStep;
            case TimeUnit::MILLISECONDS:    return m_TimeStep * MILLISECONDS_PER_SECOND;
            case TimeUnit::MICROSECONDS:    return m_TimeStep * MICROSECONDS_PER_SECOND;
            case TimeUnit::NANOSECONDS:     return m_TimeStep * NANOSECONDS_PER_SECOND;
        }

        return m_TimeStep;
    }

    auto TimeService::GetTime( const TimeUnit unit ) const -> double {
        switch (unit) {
            case TimeUnit::SECONDS:         return std::chrono::duration_cast<Sec_T>(Clock_T::now() - m_InitTimePoint).count();
            case TimeUnit::MILLISECONDS:    return std::chrono::duration_cast<Milli_T>(Clock_T::now() - m_InitTimePoint).count();
            case TimeUnit::MICROSECONDS:    return std::chrono::duration_cast<Micro_T>(Clock_T::now() - m_InitTimePoint).count();
            case TimeUnit::NANOSECONDS:     return std::chrono::duration_cast<Nano_T>(Clock_T::now() - m_InitTimePoint).count();
        }

        return std::chrono::duration_cast<Sec_T>(Clock_T::now() - m_InitTimePoint).count();
    }

    auto TimeService::ToString( double time, const TimeUnit unit ) -> std::string {
        using namespace std::literals::chrono_literals;

        if (unit != TimeUnit::SECONDS)
            time = TransformToSeconds(time, unit);

        std::chrono::hours hours{ static_cast<ULongLong_T>(time / SECONDS_PER_HOUR) };
        std::chrono::minutes minutes{ (static_cast<ULongLong_T>(time) % static_cast<ULongLong_T>(SECONDS_PER_HOUR) / SECONDS_PER_MINUTE) };
        std::chrono::seconds seconds{ (static_cast<ULongLong_T>(time) % static_cast<ULongLong_T>(SECONDS_PER_HOUR) % SECONDS_PER_MINUTE) };

        return fmt::format("{:%H:%M:%S}", hours + minutes + seconds);
        return "";
    }

    auto TimeService::TransformToSeconds( const double time, const TimeUnit unit ) -> double {
        switch ( unit ) {
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

        if (m_IsInitialized) {

        }
    }

}// namespace Mikoto