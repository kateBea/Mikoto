/**
 * Timer.hh
 * Created by kate on 6/15/23.
 * */

#ifndef MIKOTO_TIMER_HH
#define MIKOTO_TIMER_HH

// C++ Standard Library
#include <chrono>
#include <string_view>

// Project Headers
#include "Common/Common.hh"
#include "Core/TimeManager.hh"

namespace Mikoto {
    class Timer {
    public:
        explicit Timer(std::string_view id = "Unknown scope", TimeUnit defaultUnit = TimeUnit::MICROSECONDS);

        MKT_NODISCARD auto GetCurrentProgress(TimeUnit unit = TimeUnit::MICROSECONDS) -> double;
        auto Restart() -> void;

        ~Timer();

    private:
        MKT_NODISCARD static auto GetUnitStr(TimeUnit unit) -> std::string_view;

    private:
        using Nano_T = std::chrono::duration<double, std::ratio<1, 1000000000>>;
        using Micro_T = std::chrono::duration<double, std::ratio<1, 1000000>>;
        using Milli_T = std::chrono::duration<double, std::ratio<1, 1000>>;
        using Sec_T = std::chrono::duration<double, std::ratio<1, 1>>;

        using Clock_T = std::chrono::high_resolution_clock;
        using TimePoint_T = std::chrono::time_point<Clock_T>;

        std::string m_Identifier{};
        TimePoint_T m_TimeSinceStart{};
        TimeUnit m_DefaultUnit{};
    };
}

#if !defined(NDEBUG) || defined(_DEBUG)
    #define MKT_PROFILE_SCOPE()  Timer _Timer{ ConcatStr(__LINE__, " @line ", __PRETTY_FUNCTION__) }
#endif


#endif // MIKOTO_TIMER_HH
