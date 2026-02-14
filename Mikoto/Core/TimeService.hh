//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_TIME_MANAGER_HH
#define MIKOTO_TIME_MANAGER_HH

#include <ratio>
#include <chrono>
#include <string>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class TimeUnit {
        SECONDS,
        MILLISECONDS,
        MICROSECONDS,
        NANOSECONDS,
    };

    // Represents a time
    // in the specified units
    struct Time {
        double Value{};
        TimeUnit Unit{ TimeUnit::SECONDS };

        MKT_NODISCARD auto GetUnitString() const -> std::string_view;
        MKT_NODISCARD auto Convert( TimeUnit unit = TimeUnit::SECONDS) const -> double;

        MKT_NODISCARD static auto GetUnitString(TimeUnit unit) -> std::string_view;

    private:
        MKT_NODISCARD static auto Convert( TimeUnit src, TimeUnit dst, double value ) -> double;
    };

    struct TimeServiceCreateInfo {
        TimeUnit DefaultUnit{ TimeUnit::SECONDS };
        auto WithDefaultUnit(TimeUnit unit) -> TimeServiceCreateInfo&;
    };

    class TimeService final : public IService, public Singleton<TimeService> {
    public:
        explicit TimeService(const TimeServiceCreateInfo& option);

        ~TimeService() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto Tick() -> void;

        MKT_NODISCARD auto GetDefaultUnit() const -> TimeUnit;
        MKT_NODISCARD auto GetTimeStep( TimeUnit unit = TimeUnit::SECONDS) const -> double;
        MKT_NODISCARD auto GetTime( TimeUnit unit = TimeUnit::SECONDS ) const -> double;
        MKT_NODISCARD static auto ToString(double time, TimeUnit unit = TimeUnit::SECONDS) -> std::string;
        MKT_NODISCARD static auto TransformToSeconds( double time, TimeUnit unit ) -> double;

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