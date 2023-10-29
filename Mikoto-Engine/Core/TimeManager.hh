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
#include <Common/Types.hh>
#include <Common/Common.hh>

namespace Mikoto {
    /**
     * @brief Enum class representing different time units.
     * */
    enum class TimeUnit {
        NONE,            /**< No specific time unit. */
        SECONDS,         /**< Time unit in seconds. */
        MILLISECONDS,    /**< Time unit in milliseconds. */
        MICROSECONDS,    /**< Time unit in microseconds. */
        NANOSECONDS,     /**< Time unit in nanoseconds. */
        COUNT,           /**< Placeholder for the number of time units. */
    };

    /**
     * @brief A utility class managing time-related operations.
     * */
    class TimeManager {
    public:
        /**
         * @brief Initializes the time manager.
         * This function sets the initial time point used for calculating time differences.
         * */
        static auto Init() {
            s_InitTimePoint = Clock_T::now();
        }

        /**
         * @brief Updates the time step.
         * This function should be called at the beginning of each frame to update the time step value.
         * */
        static auto UpdateTimeStep() -> void {
            TimePoint_T now{ Clock_T::now() };
            s_TimeStep = std::chrono::duration_cast<Sec_T>(now - s_LastFrameTime).count();
            s_LastFrameTime = now;
        }

        /**
         * @brief Returns the time step.
         * @param unit The time step units.
         * @returns Time step value in the specified time unit.
         * This function allows conversion of the time step value to different time units.
         * */
        static auto GetTimeStep(TimeUnit unit = TimeUnit::SECONDS) -> double {
            switch (unit) {
                case TimeUnit::SECONDS:         return s_TimeStep;
                case TimeUnit::MILLISECONDS:    return s_TimeStep * secToMilli;
                case TimeUnit::MICROSECONDS:    return s_TimeStep * secToMicro;
                case TimeUnit::NANOSECONDS:     return s_TimeStep * secToNano;
                case TimeUnit::NONE:
                case TimeUnit::COUNT:
                default:                        return -1;
            }
        }

        /**
         * @brief Returns the time since the first call to Init().
         * This function calculates the time difference from the initial time point set by Init().
         * @param unit The time unit.
         * @returns The amount of time passed since the first call to Init().
         * */
        static auto GetTime(TimeUnit unit = TimeUnit::SECONDS) -> double {
            switch (unit) {
                case TimeUnit::SECONDS:         return std::chrono::duration_cast<Sec_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::MILLISECONDS:    return std::chrono::duration_cast<Milli_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::MICROSECONDS:    return std::chrono::duration_cast<Micro_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::NANOSECONDS:     return std::chrono::duration_cast<Nano_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::NONE:
                case TimeUnit::COUNT:
                default:                        return -1;
            }
        }

        /**
         * @brief Returns a formatted string representing the time.
         * @param time The time to be represented.
         * @param unit The time unit.
         * @returns A string representing the time in HH:MM:SS format.
         * This function formats the provided time into HH:MM:SS format for display.
         * */
        static auto ToString(double time, TimeUnit unit = TimeUnit::SECONDS) -> std::string {
            using namespace std::literals::chrono_literals;

            if (unit != TimeUnit::SECONDS)
                time = TransformToSeconds(time, unit);

            std::chrono::hours hours{ (ULongLong_T)(time / hoursToSeconds) };
            std::chrono::minutes minutes{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)(hoursToSeconds)) / minutesToSeconds) };
            std::chrono::seconds seconds{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)(hoursToSeconds)) % minutesToSeconds) };

            return fmt::format("{:%H:%M:%S}", hours + minutes + seconds);
        }

    public:
        /**
         * @brief Converts time to seconds.
         * @param time The time to be converted.
         * @param unit The units of the time provided.
         * @returns The time converted to seconds.
         * This function is used to convert time values from various units to seconds for consistency.
         * */
        MKT_UNUSED_FUNC static auto TransformToSeconds(double time, TimeUnit unit) -> double {
            switch (unit) {
                case TimeUnit::MILLISECONDS:    return time / secToMilli;
                case TimeUnit::MICROSECONDS:    return time / secToMicro;
                case TimeUnit::NANOSECONDS:     return time / secToNano;
                default:                        return time;
            }
        }

        static constexpr UInt32_T hoursToSeconds{ 3'600 };        /**< Seconds per hour. */
        static constexpr UInt32_T minutesToSeconds{ 60 };         /**< Minutes per second. */
        static constexpr UInt32_T secToMilli{ 1'000 };            /**< Milliseconds per second. */
        static constexpr UInt32_T secToMicro{ 1'000'000 };        /**< Microseconds per second. */
        static constexpr UInt32_T secToNano{ 1'000'000'000 };     /**< Nanoseconds per second. */

        using Nano_T      = std::chrono::duration<double, std::ratio<1, 1'000'000'000>>;   /**< Type for nanoseconds. */
        using Micro_T     = std::chrono::duration<double, std::ratio<1, 1'000'000>>;       /**< Type for microseconds. */
        using Milli_T     = std::chrono::duration<double, std::ratio<1, 1'000>>;           /**< Type for milliseconds. */
        using Sec_T       = std::chrono::duration<double, std::ratio<1, 1>>;               /**< Type for seconds. */
        using Clock_T     = std::chrono::high_resolution_clock;                            /**< Type for clock. */
        using TimePoint_T = std::chrono::time_point<Clock_T>;                              /**< Type for a point in time. */

    private:
        inline static double s_TimeStep{};               /**< Time in seconds. */
        inline static TimePoint_T s_LastFrameTime{};     /**< Last frame's time point. */
        inline static TimePoint_T s_InitTimePoint{};     /**< Initial time point. */
    };
}

#endif // MIKOTO_TIME_MANAGER_HH