/**
 * Timer.cc
 * Created by kate on 6/15/23.
 * */

// C++ Standard Library
#include <chrono>

// Project Headers
#include <Common/Common.hh>
#include <Core/TimeManager.hh>
#include <Profiling/Timer.hh>

namespace Mikoto {

    Timer::Timer(std::string_view id, TimeUnit unit)
        :   m_Identifier{ id }, m_TimeSinceStart{ Clock_T::now() }, m_DefaultUnit{ unit }
    {}

    auto Timer::GetCurrentProgress(TimeUnit unit) -> double {
        switch (unit) {
            case TimeUnit::SECONDS:         return std::chrono::duration_cast<Sec_T>(Clock_T::now() - m_TimeSinceStart).count();
            case TimeUnit::MILLISECONDS:    return std::chrono::duration_cast<Milli_T>(Clock_T::now() - m_TimeSinceStart).count();
            case TimeUnit::MICROSECONDS:    return std::chrono::duration_cast<Micro_T>(Clock_T::now() - m_TimeSinceStart).count();
            case TimeUnit::NANOSECONDS:     return std::chrono::duration_cast<Nano_T>(Clock_T::now() - m_TimeSinceStart).count();
        }
    }

    auto Timer::Restart() -> void {
        m_TimeSinceStart = Clock_T::now();
    }

    Timer::~Timer() {
        MKT_COLOR_PRINT_FORMATTED(MKT_FMT_COLOR_SNOW, "{} took {} {}\n", m_Identifier, GetCurrentProgress(m_DefaultUnit), GetUnitStr(m_DefaultUnit));
    }
    auto Timer::GetUnitStr(TimeUnit unit) -> std::string_view {
        switch (unit) {
            case TimeUnit::SECONDS:         return "s";
            case TimeUnit::MILLISECONDS:    return "ms";
            case TimeUnit::MICROSECONDS:    return "µs";
            case TimeUnit::NANOSECONDS:     return "ns";
        }
    }
}
