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
#include <fmt/format.h>

// Project Headers
#include <Common/Common.hh>
#include <Core/Engine.hh>
#include <Library/Utility/Types.hh>
#include <Models/Enums.hh>

namespace Mikoto {

    /**
     * @brief A utility class to manage time-related operations.
     * */
    class TimeSystem final : public IEngineSystem {
    public:
        explicit TimeSystem() = default;
        explicit TimeSystem(const EngineConfig& option) {

        }



        ~TimeSystem() override = default;

        /**
         * @brief Initializes the time manager.
         * This function sets an initial time point.
         * */
        auto Init() -> void override {
            m_InitTimePoint = Clock_T::now();
        }

        /**
         * @brief Updates the time step. Call once per loop.
         * */
        auto Update() -> void override {
            const auto now{ Clock_T::now() };
            m_TimeStep = std::chrono::duration_cast<Sec_T>(now - m_LastFrameTime).count();
            m_LastFrameTime = now;
        }

        /**
         * @brief Returns the time step. Allows conversion
         * of the time step value to different time units.
         * @param unit The time step unit of the returned value, default unit is seconds.
         * @returns Time step value in the specified time unit.
         * */
        MKT_NODISCARD auto GetTimeStep( const TimeUnit unit = TimeUnit::SECONDS) const -> double {
            switch (unit) {
                case TimeUnit::SECONDS:         return m_TimeStep;
                case TimeUnit::MILLISECONDS:    return m_TimeStep * MILLISECONDS_PER_SECOND;
                case TimeUnit::MICROSECONDS:    return m_TimeStep * MICROSECONDS_PER_SECOND;
                case TimeUnit::NANOSECONDS:     return m_TimeStep * NANOSECONDS_PER_SECOND;
            }

            return m_TimeStep;
        }

        /**
         * @brief Returns the time since the first call to Init().
         * @param unit The time unit of the returned value, default unit is seconds.
         * @returns The amount of time has passed since the first call to Init().
         * */
        MKT_NODISCARD auto GetTime( const TimeUnit unit = TimeUnit::SECONDS ) const -> double {
            switch (unit) {
                case TimeUnit::SECONDS:         return std::chrono::duration_cast<Sec_T>(Clock_T::now() - m_InitTimePoint).count();
                case TimeUnit::MILLISECONDS:    return std::chrono::duration_cast<Milli_T>(Clock_T::now() - m_InitTimePoint).count();
                case TimeUnit::MICROSECONDS:    return std::chrono::duration_cast<Micro_T>(Clock_T::now() - m_InitTimePoint).count();
                case TimeUnit::NANOSECONDS:     return std::chrono::duration_cast<Nano_T>(Clock_T::now() - m_InitTimePoint).count();
            }

            return std::chrono::duration_cast<Sec_T>(Clock_T::now() - m_InitTimePoint).count();
        }

        /**
         * @brief Returns a formatted string representing the time.
         * @param time The time to be represented.
         * @param unit The time unit. Specifies the unit for the first argument.
         * @returns A string representing the time in HH:MM:SS format.
         * */
        MKT_NODISCARD static auto ToString(double time, const TimeUnit unit = TimeUnit::SECONDS) -> std::string {
            using namespace std::literals::chrono_literals;

            if (unit != TimeUnit::SECONDS)
                time = TransformToSeconds(time, unit);

            std::chrono::hours hours{ (ULongLong_T)(time / SECONDS_PER_HOUR ) };
            std::chrono::minutes minutes{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)( SECONDS_PER_HOUR )) / SECONDS_PER_MINUTE ) };
            std::chrono::seconds seconds{ (ULongLong_T)(((ULongLong_T)(time) % (ULongLong_T)( SECONDS_PER_HOUR )) % SECONDS_PER_MINUTE ) };

            //return fmt::format("{:%H:%M:%S}", hours + minutes + seconds);
            return "";
        }

        /**
         * @brief Converts time to seconds.
         * @param time The time to be converted.
         * @param unit The units of the time provided.
         * @returns The time converted to seconds.
         * */
        MKT_UNUSED_FUNC static auto TransformToSeconds( const double time, const TimeUnit unit ) -> double {
            switch ( unit ) {
                case TimeUnit::MILLISECONDS:
                    return time / MILLISECONDS_PER_SECOND;
                case TimeUnit::MICROSECONDS:
                    return time / MICROSECONDS_PER_SECOND;
                case TimeUnit::NANOSECONDS:
                    return time / NANOSECONDS_PER_SECOND;
                default:
                    return time;
            }
        }

        auto Shutdown() -> void override {

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
        using Sec_T       = std::chrono::duration<double /* std::ratio<1, 1> */>;               /**< Type for seconds. */

        // Time point types
        using Clock_T     = std::chrono::high_resolution_clock;                            /**< Type for clock. */
        using TimePoint_T = std::chrono::time_point<Clock_T>;                              /**< Type for a point in time. */

    private:
        double m_TimeStep{};
        TimePoint_T m_LastFrameTime{};
        TimePoint_T m_InitTimePoint{};
    };
}

#endif // MIKOTO_TIME_MANAGER_HH