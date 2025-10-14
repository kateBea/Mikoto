/**
 * Timer.cc
 * Created by kate on 6/15/23.
 * */

// C++ Standard Library
#include <chrono>

// Project Headers
#include <Common/Common.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Core/TimeService.hh>
#include <Library/String/String.hh>
#include <Core/Timer.hh>

namespace Mikoto {

    MKT_NODISCARD static constexpr auto GetUnitStr( const TimeUnit defaultUnit = TimeUnit::SECONDS) -> std::string_view {
        switch (defaultUnit) {
            case TimeUnit::SECONDS:         return "s";
            case TimeUnit::MILLISECONDS:    return "ms";
            case TimeUnit::MICROSECONDS:    return "µs";
            case TimeUnit::NANOSECONDS:     return "ns";
        }

        return "s";
    }

    Timer::Timer(const std::string_view startMessage)
        :   m_TimeSinceStart{ Clock::now() }
    {
        MKT_CORE_LOGGER_DEBUG("{}", startMessage);
    }

    auto Timer::GetCurrentProgress( const TimeUnit defaultUnit ) const -> double {
        switch (defaultUnit) {
            case TimeUnit::SECONDS:         return std::chrono::duration_cast<Sec>(Clock::now() - m_TimeSinceStart).count();
            case TimeUnit::MILLISECONDS:    return std::chrono::duration_cast<Milli>(Clock::now() - m_TimeSinceStart).count();
            case TimeUnit::MICROSECONDS:    return std::chrono::duration_cast<Micro>(Clock::now() - m_TimeSinceStart).count();
            case TimeUnit::NANOSECONDS:     return std::chrono::duration_cast<Nano>(Clock::now() - m_TimeSinceStart).count();
        }

        return std::chrono::duration_cast<Sec>(Clock::now() - m_TimeSinceStart).count();;
    }

    auto Timer::Restart() -> void {
        m_TimeSinceStart = Clock::now();
    }

    Timer::~Timer() {
        const auto units{ TimeUnit::MILLISECONDS };
        MKT_CORE_LOGGER_DEBUG("{} End Timer. Elapsed {} {}",
            __PRETTY_FUNCTION__, GetCurrentProgress(units), GetUnitStr(units));
    }
}
