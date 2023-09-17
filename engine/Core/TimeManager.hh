/**
 * TimeManager.hh
 * Created by kate on 6/8/23.
 * */

#ifndef MIKOTO_TIME_MANAGER_HH
#define MIKOTO_TIME_MANAGER_HH

// C++ Standard Library
#include <ratio>
#include <chrono>
#include <string>

// Third-Party Libraries
#include <fmt/chrono.h>

// Project Headers
#include <Utility/Types.hh>
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
        /**
         * Initializes the time manager
         * */
        static auto Init() {
            s_InitTimePoint = Clock_T::now();
        }

        /**
         * Updates the time step. Must call at the beginning of each frame
         * */
        static auto UpdateTimeStep() -> void {
            TimePoint_T now{ Clock_T::now() };
            s_TimeStep = std::chrono::duration_cast<Sec_T>(now - s_LastFrameTime).count();
            s_LastFrameTime = now;
        }

        /**
         * Returns the time step
         * @param unit time step units
         * @returns time step
         * */
        static auto GetTimeStep(TimeUnit unit = TimeUnit::SECONDS) -> double {
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

        /**
         * Returns the time (by default in seconds) since the first call to Init()
         * @param unit time unit
         * @returns amount of time passed since first call to Init()
         * */
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

        /**
         * Returns a formatted string representing the time passed in
         * @param time time to be represented in the format HH:MM:SS
         * @param unit time unit
         * @returns string representing the time
         * */
        static auto ToString(double time, TimeUnit unit = TimeUnit::SECONDS) -> std::string {
            using namespace std::literals::chrono_literals;

            if (unit != TimeUnit::SECONDS)
                time = TransformToSeconds(time, unit);

            std::chrono::hours hours{ (ULongLong_T)(time / HOURS_TO_SECONDS) };
            std::chrono::minutes minutes{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)(HOURS_TO_SECONDS)) / MINUTES_TO_SECONDS) };
            std::chrono::seconds seconds{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)(HOURS_TO_SECONDS)) % MINUTES_TO_SECONDS) };

            return fmt::format("{:%H:%M:%S}", hours + minutes + seconds);
        }

    public:
        /**
         * Returns the time converted to seconds
         * @param time time to be converted
         * @param unit units of the time provided
         * @returns time converted to seconds
         * */
        MKT_UNUSED_FUNC static auto TransformToSeconds(double time, TimeUnit unit) -> double {
            switch (unit) {
                case TimeUnit::MILLISECONDS:    return time / SEC_TO_MILLI;
                case TimeUnit::MICROSECONDS:    return time / SEC_TO_MICRO;
                case TimeUnit::NANOSECONDS:     return time / SEC_TO_NANO;
                default:                        return time;
            }
        }

        static constexpr UInt32_T HOURS_TO_SECONDS{ 3'600 };        // Seconds per hour
        static constexpr UInt32_T MINUTES_TO_SECONDS{ 60 };         // Minutes per second

        static constexpr UInt32_T SEC_TO_MILLI{ 1'000 };            // Milliseconds per second
        static constexpr UInt32_T SEC_TO_MICRO{ 1'000'000 };        // Microseconds per second
        static constexpr UInt32_T SEC_TO_NANO{ 1'000'000'000 };     // Nanoseconds per second

        using Nano_T = std::chrono::duration<double, std::ratio<1, 1'000'000'000>>;
        using Micro_T = std::chrono::duration<double, std::ratio<1, 1'000'000>>;
        using Milli_T = std::chrono::duration<double, std::ratio<1, 1'000>>;
        using Sec_T = std::chrono::duration<double, std::ratio<1, 1>>;
        using Clock_T = std::chrono::high_resolution_clock;
        using TimePoint_T = std::chrono::time_point<Clock_T>;

    private:
        inline static double s_TimeStep{}; // Time in seconds
        inline static TimePoint_T s_LastFrameTime{};
        inline static TimePoint_T s_InitTimePoint{};
    };
}

#endif // MIKOTO_TIME_MANAGER_HH