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

namespace Mikoto {
    class Timer final {
    public:
        explicit Timer(std::string_view startMessage);

        MKT_NODISCARD auto GetCurrentProgress(TimeUnit defaultUnit = TimeUnit::SECONDS) const -> double;
        auto Restart() -> void;

        ~Timer();

    private:
        MKT_NODISCARD static auto GetUnitStr(TimeUnit defaultUnit = TimeUnit::SECONDS) -> std::string_view;

    private:
        using Nano_T = std::chrono::duration<double, std::ratio<1, 1000000000>>;
        using Micro_T = std::chrono::duration<double, std::ratio<1, 1000000>>;
        using Milli_T = std::chrono::duration<double, std::ratio<1, 1000>>;
        using Sec_T = std::chrono::duration<double, std::ratio<1, 1>>;

        using Clock_T = std::chrono::high_resolution_clock;
        using TimePoint_T = std::chrono::time_point<Clock_T>;

        TimePoint_T m_TimeSinceStart{};
    };
}

#if !defined(NDEBUG) || defined(_DEBUG)
    #define MKT_PROFILE_SCOPE()  Timer _Timer{ StringUtils::ConcatStr(__PRETTY_FUNCTION__, "@line: [", __LINE__, "] Start profiling." ) }
#else
    #define MKT_PROFILE_SCOPE()
#endif


#endif // MIKOTO_TIMER_HH
