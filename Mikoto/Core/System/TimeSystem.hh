/**
 * TimeSystem.hh
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
#include <STL/Utility/Types.hh>
#include <Models/Enums.hh>
#include <Common/Common.hh>

namespace Mikoto {

    /**
     * @brief A utility class to manage time-related operations.
     * */
    class TimeManager {
    public:
        /**
         * @brief Initializes the time manager.
         * This function sets an initial time point.
         * */
        static auto Init() -> void {
            s_InitTimePoint = Clock_T::now();
        }


        /**
         * @brief Updates the time step. Call once per loop.
         * */
        static auto UpdateTimeStep() -> void {
            const auto now{ Clock_T::now() };
            s_TimeStep = std::chrono::duration_cast<Sec_T>(now - s_LastFrameTime).count();
            s_LastFrameTime = now;
        }


        /**
         * @brief Returns the time step. Allows conversion
         * of the time step value to different time units.
         * @param unit The time step unit of the returned value, default unit is seconds.
         * @returns Time step value in the specified time unit.
         * */
        static auto GetTimeStep(TimeUnit unit = TimeUnit::SECONDS) -> double {
            switch (unit) {
                case TimeUnit::SECONDS:         return s_TimeStep;
                case TimeUnit::MILLISECONDS:    return s_TimeStep * MILLISECONDS_PER_SECOND;
                case TimeUnit::MICROSECONDS:    return s_TimeStep * MICROSECONDS_PER_SECOND;
                case TimeUnit::NANOSECONDS:     return s_TimeStep * NANOSECONDS_PER_SECOND;
            }

            return s_TimeStep;
        }


        /**
         * @brief Returns the time since the first call to Init().
         * @param unit The time unit of the returned value, default unit is seconds.
         * @returns The amount of time passed since the first call to Init().
         * */
        static auto GetTime(TimeUnit unit = TimeUnit::SECONDS) -> double {
            switch (unit) {
                case TimeUnit::SECONDS:         return std::chrono::duration_cast<Sec_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::MILLISECONDS:    return std::chrono::duration_cast<Milli_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::MICROSECONDS:    return std::chrono::duration_cast<Micro_T>(Clock_T::now() - s_InitTimePoint).count();
                case TimeUnit::NANOSECONDS:     return std::chrono::duration_cast<Nano_T>(Clock_T::now() - s_InitTimePoint).count();
            }

            return std::chrono::duration_cast<Sec_T>(Clock_T::now() - s_InitTimePoint).count();
        }


        /**
         * @brief Returns a formatted string representing the time.
         * @param time The time to be represented.
         * @param unit The time unit. Specifies the unit for the first argument.
         * @returns A string representing the time in HH:MM:SS format.
         * */
        static auto ToString(double time, TimeUnit unit = TimeUnit::SECONDS) -> std::string {
            using namespace std::literals::chrono_literals;

            if (unit != TimeUnit::SECONDS)
                time = TransformToSeconds(time, unit);

            std::chrono::hours hours{ (ULongLong_T)(time / SECONDS_PER_HOUR ) };
            std::chrono::minutes minutes{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)( SECONDS_PER_HOUR )) / SECONDS_PER_MINUTE ) };
            std::chrono::seconds seconds{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)( SECONDS_PER_HOUR )) % SECONDS_PER_MINUTE ) };

            return fmt::format("{:%H:%M:%S}", hours + minutes + seconds);
        }


        /**
         * @brief Converts time to seconds.
         * @param time The time to be converted.
         * @param unit The units of the time provided.
         * @returns The time converted to seconds.
         * */
        MKT_UNUSED_FUNC static auto TransformToSeconds(double time, TimeUnit unit) -> double {
            switch (unit) {
                case TimeUnit::MILLISECONDS:    return time / MILLISECONDS_PER_SECOND;
                case TimeUnit::MICROSECONDS:    return time / MICROSECONDS_PER_SECOND;
                case TimeUnit::NANOSECONDS:     return time / NANOSECONDS_PER_SECOND;
                default:                        return time;
            }
        }

        // Conversion constants
        static constexpr UInt32_T SECONDS_PER_HOUR{ 3'600 };        /**< Seconds per hour. */
        static constexpr UInt32_T SECONDS_PER_MINUTE{ 60 };         /**< Minutes per second. */

        static constexpr UInt32_T MILLISECONDS_PER_SECOND{ 1'000 };            /**< Milliseconds per second. */
        static constexpr UInt32_T MICROSECONDS_PER_SECOND{ 1'000'000 };        /**< Microseconds per second. */
        static constexpr UInt32_T NANOSECONDS_PER_SECOND{ 1'000'000'000 };     /**< Nanoseconds per second. */

        // Conversion types
        using Nano_T      = std::chrono::duration<double, std::ratio<1, 1'000'000'000>>;   /**< Type for nanoseconds. */
        using Micro_T     = std::chrono::duration<double, std::ratio<1, 1'000'000>>;       /**< Type for microseconds. */
        using Milli_T     = std::chrono::duration<double, std::ratio<1, 1'000>>;           /**< Type for milliseconds. */
        using Sec_T       = std::chrono::duration<double, std::ratio<1, 1>>;               /**< Type for seconds. */

        // Time point types
        using Clock_T     = std::chrono::high_resolution_clock;                            /**< Type for clock. */
        using TimePoint_T = std::chrono::time_point<Clock_T>;                              /**< Type for a point in time. */

    private:
        inline static double s_TimeStep{};               /**< Time in seconds. */
        inline static TimePoint_T s_LastFrameTime{};     /**< Last frame's time point. */
        inline static TimePoint_T s_InitTimePoint{};     /**< Initial time point. */
    };
}

#endif // MIKOTO_TIME_MANAGER_HH