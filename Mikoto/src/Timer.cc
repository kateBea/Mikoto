/**
 * Timer.cc
 * Created by kate on 6/15/23.
 * */

// C++ Standard Library
#include <chrono>

// Project Headers
#include <Common/Common.hh>
#include <Models/Enums.hh>
#include <Common/Constants.hh>
#include <Core/Logging/Assert.hh>
#include <Core/Logging/Logger.hh>
#include <Profiling/Timer.hh>
#include <Library/String/String.hh>

namespace Mikoto {

    Timer::Timer(const std::string_view startMessage)
        :   m_TimeSinceStart{ Clock_T::now() }
    {
        MKT_CORE_LOGGER_DEBUG("{}", startMessage);
    }

    auto Timer::GetCurrentProgress( const TimeUnit defaultUnit ) const -> double {
        switch (defaultUnit) {
            case TimeUnit::SECONDS:         return std::chrono::duration_cast<Sec_T>(Clock_T::now() - m_TimeSinceStart).count();
            case TimeUnit::MILLISECONDS:    return std::chrono::duration_cast<Milli_T>(Clock_T::now() - m_TimeSinceStart).count();
            case TimeUnit::MICROSECONDS:    return std::chrono::duration_cast<Micro_T>(Clock_T::now() - m_TimeSinceStart).count();
            case TimeUnit::NANOSECONDS:     return std::chrono::duration_cast<Nano_T>(Clock_T::now() - m_TimeSinceStart).count();
        }

        return -1.0;
    }

    auto Timer::Restart() -> void {
        m_TimeSinceStart = Clock_T::now();
    }

    Timer::~Timer() {
        MKT_CORE_LOGGER_DEBUG("{} End Profiling. Elapsed {} {} ms(s)\n", __PRETTY_FUNCTION__, GetCurrentProgress(), GetUnitStr(TimeUnit::MILLISECONDS));
    }

    auto Timer::GetUnitStr( const TimeUnit defaultUnit ) -> std::string_view {
        switch (defaultUnit) {
            case TimeUnit::SECONDS:         return "s";
            case TimeUnit::MILLISECONDS:    return "ms";
            case TimeUnit::MICROSECONDS:    return "µs";
            case TimeUnit::NANOSECONDS:     return "ns";
        }

        return "";
    }
}
