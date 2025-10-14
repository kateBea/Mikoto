/**
 * TimeService.hh
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
#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    /**
     * @brief Time units.
     * */
    enum class TimeUnit {
        SECONDS,
        MILLISECONDS,
        MICROSECONDS,
        NANOSECONDS,
    };

    struct TimeServiceCreateInfo {
        TimeUnit DefaultUnit{ TimeUnit::SECONDS };

        auto WithDefaultUnit(TimeUnit unit) -> TimeServiceCreateInfo&;
    };
    
    /**
     * @brief A utility class to manage time-related operations.
     * */
    class TimeService final : public IService, public Singleton<TimeService> {
    public:
        explicit TimeService(const TimeServiceCreateInfo& option);

        ~TimeService() override = default;

        /**
         * @brief Initializes the time manager.
         * This function sets an initial time point.
         * */
        auto Init() -> void override;

        /**
         * @brief Updates the time step. Call once per loop.
         * */
        auto Update() -> void;

        MKT_NODISCARD auto GetDefaultUnit() const -> TimeUnit;

        /**
         * @brief Returns the time step. Allows conversion
         * of the time step value to different time units.
         * @param unit The time step unit of the returned value, default unit is seconds.
         * @returns Time step value in the specified time unit.
         * */
        MKT_NODISCARD auto GetTimeStep( TimeUnit unit = TimeUnit::SECONDS) const -> double;

        /**
         * @brief Returns the time since the first call to Init().
         * @param unit The time unit of the returned value, default unit is seconds.
         * @returns The amount of time has passed since the first call to Init().
         * */
        MKT_NODISCARD auto GetTime( TimeUnit unit = TimeUnit::SECONDS ) const -> double;

        /**
         * @brief Returns a formatted string representing the time.
         * @param time The time to be represented.
         * @param unit The time unit. Specifies the unit for the first argument.
         * @returns A string representing the time in HH:MM:SS format.
         * */
        MKT_NODISCARD static auto ToString(double time, TimeUnit unit = TimeUnit::SECONDS) -> std::string;

        /**
         * @brief Converts time to seconds.
         * @param time The time to be converted.
         * @param unit The units of the time provided.
         * @returns The time converted to seconds.
         * */
        MKT_UNUSED_FUNC static auto TransformToSeconds( double time, TimeUnit unit ) -> double;

        auto Shutdown() -> void override;

        // Conversion constants
        static constexpr UInt32 SECONDS_PER_HOUR{ 3'600 };        /**< Seconds per hour. */
        static constexpr UInt32 SECONDS_PER_MINUTE{ 60 };         /**< Minutes per second. */

        static constexpr UInt32 MILLISECONDS_PER_SECOND{ 1'000 };            /**< Milliseconds per second. */
        static constexpr UInt32 MICROSECONDS_PER_SECOND{ 1'000'000 };        /**< Microseconds per second. */
        static constexpr UInt32 NANOSECONDS_PER_SECOND{ 1'000'000'000 };     /**< Nanoseconds per second. */

        // Conversion types
        using Nano      = std::chrono::duration<double, std::ratio<1, 1'000'000'000>>;   /**< Type for nanoseconds. */
        using Micro     = std::chrono::duration<double, std::ratio<1, 1'000'000>>;       /**< Type for microseconds. */
        using Milli     = std::chrono::duration<double, std::ratio<1, 1'000>>;           /**< Type for milliseconds. */
        using Sec       = std::chrono::duration<double /* std::ratio<1, 1> */>;               /**< Type for seconds. */

        // Time point types
        using Clock     = std::chrono::high_resolution_clock;                            /**< Type for clock. */
        using TimePoint = std::chrono::time_point<Clock>;                              /**< Type for a point in time. */

    private:
        double m_TimeStep{};
        TimePoint m_LastFrameTime{};
        TimePoint m_InitTimePoint{};

        TimeUnit m_DefaultUnits{ TimeUnit::SECONDS };
    };
}

#endif // MIKOTO_TIME_MANAGER_HH