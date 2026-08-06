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

#include <EASTL/chrono.h>
#include <EASTL/ratio.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

namespace mikoto::core {

    enum class TimeUnit {
        eSeconds,
        eMilliseconds,
        eMicroseconds,
        eNanoseconds,
    };

    struct Time {
        double mValue{};
        TimeUnit mUnit{ TimeUnit::eSeconds };

        MKT_NODISCARD auto GetUnitString() const -> eastl::string_view;
        MKT_NODISCARD auto Convert( TimeUnit unit = TimeUnit::eSeconds) const -> double;

        MKT_NODISCARD static auto GetUnitString(TimeUnit unit) -> eastl::string_view;


    private:
        MKT_NODISCARD static auto Convert( TimeUnit src, TimeUnit dst, double value ) -> double;
    };

    struct TimeServiceCreateInfo {
        TimeUnit mDefaultUnit{ TimeUnit::eSeconds };

        auto WithDefaultUnit(TimeUnit unit) -> TimeServiceCreateInfo&;
    };

    class TimeService final : public IService, public Singleton<TimeService> {
    public:
        explicit TimeService(const TimeServiceCreateInfo& option);

        ~TimeService() override = default;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Tick() -> void;

        // TODO: Add a system that uses MKT_BEGIN_PROFILER_NAMED() and stores the time every function took
        // can use that to do some nice visualizations on ImGui
        auto PushTime( const eastl::string& tag, Time time ) -> void;

        MKT_NODISCARD auto GetProfilingTimes() const -> const auto& { return mTimeMap; }

        MKT_NODISCARD auto GetDefaultUnit() const -> TimeUnit;
        MKT_NODISCARD auto GetTimeStep( TimeUnit unit = TimeUnit::eSeconds) const -> double;
        MKT_NODISCARD auto GetTime( TimeUnit unit = TimeUnit::eSeconds ) const -> double;
        MKT_NODISCARD static auto ToString(double time, TimeUnit unit = TimeUnit::eSeconds) -> eastl::string;
        MKT_NODISCARD static auto TransformToSeconds( double time, TimeUnit unit ) -> double;

        // Conversion constants
        static constexpr u32 kSecPerHour{ 3'600 };        /**< Seconds per hour. */
        static constexpr u32 kSecPerMin{ 60 };         /**< Minutes per second. */

        static constexpr u32 kMilliPerSeconds{ 1'000 };            /**< Milliseconds per second. */
        static constexpr u32 kMicroPerSeconds{ 1'000'000 };        /**< Microseconds per second. */
        static constexpr u32 kNanoPerSeconds{ 1'000'000'000 };     /**< Nanoseconds per second. */

        // Conversion types
        using Nano      = eastl::chrono::duration<double, eastl::ratio<1, 1'000'000'000>>;   /**< Type for nanoseconds. */
        using Micro     = eastl::chrono::duration<double, eastl::ratio<1, 1'000'000>>;       /**< Type for microseconds. */
        using Milli     = eastl::chrono::duration<double, eastl::ratio<1, 1'000>>;           /**< Type for milliseconds. */
        using Sec       = eastl::chrono::duration<double /* eastl::ratio<1, 1> */>;               /**< Type for seconds. */

        // Time point types
        using Clock     = eastl::chrono::high_resolution_clock;                            /**< Type for clock. */
        using TimePoint = eastl::chrono::time_point<Clock>;                              /**< Type for a point in time. */

    private:
        double mTimeStep{};
        TimePoint mLastFrameTime{};
        TimePoint mInitTimePoint{};

        TimeUnit mDefaultUnits{ TimeUnit::eSeconds };

        std::mutex mTimeMutex{};
        ankerl::unordered_dense::map<eastl::string, eastl::vector<Time>> mTimeMap;
    };
}

#endif // MIKOTO_TIME_MANAGER_HH