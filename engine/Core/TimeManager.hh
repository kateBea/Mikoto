//
// Created by kate on 6/8/23.
//

#ifndef KATE_ENGINE_TIME_STEP_HH
#define KATE_ENGINE_TIME_STEP_HH

#include <ratio>
#include <chrono>
#include <string>

#include <fmt/chrono.h>

#include <Utility/Common.hh>

namespace Mikoto {
    enum class TimeUnit {
        NONE,
        SECONDS,
        MILLISECONDS,
        MICROSECONDS,
        NANOSECONDS,
        COUNT,
    };

    class TimeManager {
    public:
        static auto Init() {
            s_InitTimePoint = Clock_T::now();
        }

        static auto UpdateDeltaTime() -> void {
            TimePoint_T now{ Clock_T::now() };
            s_TimeStep = std::chrono::duration_cast<Sec_T>(now - s_LastFrameTime).count();
            s_LastFrameTime = now;
        }

        static auto GetDeltaTime(TimeUnit unit = TimeUnit::SECONDS) -> double {
            switch (unit) {
                case TimeUnit::SECONDS:         return s_TimeStep;
                case TimeUnit::MILLISECONDS:    return s_TimeStep * SEC_TO_MILLI;
                case TimeUnit::MICROSECONDS:    return s_TimeStep * SEC_TO_MICRO;
                case TimeUnit::NANOSECONDS:     return s_TimeStep * SEC_TO_NANO;

                case TimeUnit::NONE:
                case TimeUnit::COUNT:   [[fallthrough]];
                default:                return -1;
            }
        }

        static auto GetTime(TimeUnit unit = TimeUnit::SECONDS) -> double {
            switch (unit) {
                case TimeUnit::SECONDS:         return std::chrono::duration_cast<Sec_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::MILLISECONDS:    return std::chrono::duration_cast<Milli_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::MICROSECONDS:    return std::chrono::duration_cast<Micro_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::NANOSECONDS:     return std::chrono::duration_cast<Nano_T>(Clock_T::now() - s_InitTimePoint).count();

                case TimeUnit::NONE:
                case TimeUnit::COUNT:   [[fallthrough]];
                default:                return -1;
            }
        }

        static auto ToString(double time) {
            using namespace std::literals::chrono_literals;
            std::chrono::hours hours{ (ULongLong_T)(time / HOURS_TO_SECONDS) };
            std::chrono::minutes minutes{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)(HOURS_TO_SECONDS)) / MINUTES_TO_SECONDS) };
            std::chrono::seconds seconds{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)(HOURS_TO_SECONDS)) % MINUTES_TO_SECONDS) };
            return fmt::format("{:%H:%M:%S}", hours + minutes + seconds);
        }

    public:
        static constexpr UInt32_T HOURS_TO_SECONDS{ 3'600 };
        static constexpr UInt32_T MINUTES_TO_SECONDS{ 60 };

        static constexpr UInt32_T SEC_TO_MILLI{ 1'000 };
        static constexpr UInt32_T SEC_TO_MICRO{ 1'000'000 };
        static constexpr UInt32_T SEC_TO_NANO{ 1'000'000'000 };
    private:
        [[maybe_unused]] static auto TransformToSeconds(double time, TimeUnit unit) -> double {
            switch (unit) {
                case TimeUnit::MILLISECONDS:    return time / SEC_TO_MILLI;
                case TimeUnit::MICROSECONDS:    return time / SEC_TO_MICRO;
                case TimeUnit::NANOSECONDS:     return time / SEC_TO_NANO;
                default:                        return time;
            }
        }

        using Nano_T    = std::chrono::duration<double, std::ratio<1, 1'000'000'000>>;
        using Micro_T   = std::chrono::duration<double, std::ratio<1, 1'000'000>>;
        using Milli_T   = std::chrono::duration<double, std::ratio<1, 1'000>>;
        using Sec_T     = std::chrono::duration<double, std::ratio<1, 1>>;

        using Clock_T       = std::chrono::high_resolution_clock;
        using TimePoint_T   =  std::chrono::time_point<Clock_T>;

        inline static double s_TimeStep{};              // Time in seconds
        inline static TimePoint_T s_LastFrameTime{};
        inline static TimePoint_T s_InitTimePoint{};
    };
}


#endif//KATE_ENGINE_TIME_STEP_HH
