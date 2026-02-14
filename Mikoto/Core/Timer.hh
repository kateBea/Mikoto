/**
 * Timer.hh
 * Created by kate on 6/15/23.
 * */

#ifndef MIKOTO_TIMER_HH
#define MIKOTO_TIMER_HH

// C++ Standard Library
#include <chrono>

// Project Headers
#include <Common/Common.hh>
#include <Common/String.hh>
#include <Core/TimeService.hh>

namespace Mikoto {
    class Timer final {
    public:
        explicit Timer(bool printOnExit);
        Timer(std::string_view scopeName, std::string_view startMessage, bool showStartMessage = false);

        MKT_NODISCARD auto GetCurrentProgress(TimeUnit defaultUnit = TimeUnit::SECONDS) const -> double;
        auto Restart() -> void;

        ~Timer();

    private:
        using Nano = std::chrono::duration<double, std::ratio<1, 1000000000>>;
        using Micro = std::chrono::duration<double, std::ratio<1, 1000000>>;
        using Milli = std::chrono::duration<double, std::ratio<1, 1000>>;
        using Sec = std::chrono::duration<double, std::ratio<1, 1>>;

        using Clock = std::chrono::high_resolution_clock;
        using TimePoint = std::chrono::time_point<Clock>;

        TimePoint m_TimeSinceStart{};

        std::string m_ScopeName{};

        bool m_PrintOnExit{ true };
    };
}

#if !defined(NDEBUG)
    #define MKT_PROFILE_SCOPE()  Timer _Timer{ __PRETTY_FUNCTION__, StringUtil::Format("[START] Profiling: {}", __PRETTY_FUNCTION__ ) }
    #define MKT_PROFILE_SCOPE_MARKED(MARK)  Timer _Timer{ MARK, StringUtil::Format("[START] Profiling: {}. Mark {}", __PRETTY_FUNCTION__, MARK ) }
#else
    #define MKT_PROFILE_SCOPE()
    #define MKT_PROFILE_SCOPE_MARKED()
#endif


#endif // MIKOTO_TIMER_HH
