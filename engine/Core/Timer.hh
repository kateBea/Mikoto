//
// Created by kate on 6/15/23.
//

#ifndef KATE_ENGINE_TIMER_HH
#define KATE_ENGINE_TIMER_HH

#include <chrono>
#include <string_view>

#include <Core/TimeManager.hh>

#include <Utility/Common.hh>

namespace Mikoto {
    class Timer {
    public:
        explicit Timer(std::string_view id = "Unknown scope", TimeUnit defaultUnit = TimeUnit::MICROSECONDS);
        ~Timer();

        auto GetCurrentProgress(TimeUnit unit = TimeUnit::MICROSECONDS) -> double;

        auto Restart() -> void;

    private:
        static auto GetUnitStr(TimeUnit unit) -> std::string_view;
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

// Call first at the beginning of the scope
#if defined(NDEBUG) || defined(_DEBUG)
    #define KT_PROFILE_SCOPE()  Timer _Timer{ ConcatStr(__LINE__, " @line ", __PRETTY_FUNCTION__) }
#endif


#endif//KATE_ENGINE_TIMER_HH
