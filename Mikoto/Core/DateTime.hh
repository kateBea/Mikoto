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

#ifndef MIKOTO_CORE_DATETIME_HH
#define MIKOTO_CORE_DATETIME_HH

#include <chrono>
#include <compare>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::core {

    enum class DateTimeKind { eUnspecified, eUtc, eLocal };
    enum class DayOfWeek { eSunday, eMonday, eTuesday, eWednesday, eThursday, eFriday, eSaturday };

    /**
     * @brief Immutable Gregorian date/time, following System.DateTime's value semantics.
     * @details Ticks count 100 ns intervals since 0001-01-01 00:00:00. The supported
     * range ends at 9999-12-31 23:59:59.9999999. Kind describes interpretation;
     * comparison and subtraction compare wall-clock ticks and ignore Kind, as in .NET.
     * Formatting/parsing use the invariant English culture; see DateTime.md for
     * supported patterns and timezone behavior. Duration replaces .NET TimeSpan.
     * @see https://learn.microsoft.com/en-us/dotnet/api/system.datetime
     * @see https://github.com/dotnet/runtime/blob/main/src/libraries/System.Private.CoreLib/src/System/DateTime.cs
     */
    class DateTime {
    public:
        using Duration = std::chrono::duration<i64, std::ratio<1, 10000000>>;
        using SystemTime = std::chrono::sys_time<Duration>;

        static constexpr i64 kTicksPerMicrosecond{ 10 };
        static constexpr i64 kTicksPerMillisecond{ 10000 };
        static constexpr i64 kTicksPerSecond{ 10000000 };
        static constexpr i64 kTicksPerMinute{ 60 * kTicksPerSecond };
        static constexpr i64 kTicksPerHour{ 60 * kTicksPerMinute };
        static constexpr i64 kTicksPerDay{ 24 * kTicksPerHour };
        static constexpr i64 kMaxTicks{ 3155378975999999999 };
        static constexpr i64 kUnixEpochTicks{ 621355968000000000 };

        /** @brief Constructs MinValue with Unspecified kind. */
        DateTime() = default;
        /** @brief Constructs a value from ticks; invalid ticks/kind throw std::out_of_range/std::invalid_argument. */
        explicit DateTime( i64 ticks, DateTimeKind kind = DateTimeKind::eUnspecified );
        /** @brief Constructs midnight. Year is 1..9999; month/day must form a valid Gregorian date. */
        DateTime( i32 year, i32 month, i32 day );
        /** @brief Constructs a time of day. Hours: 0..23; minutes/seconds: 0..59. Leap seconds are unsupported. */
        DateTime( i32 year, i32 month, i32 day, i32 hour, i32 minute, i32 second, DateTimeKind kind = DateTimeKind::eUnspecified );
        /** @brief Adds the millisecond field (0..999), preserving the requested Kind. */
        DateTime( i32 year, i32 month, i32 day, i32 hour, i32 minute, i32 second, i32 millisecond, DateTimeKind kind = DateTimeKind::eUnspecified );
        /** @brief Adds millisecond and microsecond fields, each in 0..999. */
        DateTime( i32 year, i32 month, i32 day, i32 hour, i32 minute, i32 second, i32 millisecond, i32 microsecond, DateTimeKind kind = DateTimeKind::eUnspecified );

        DateTime( const DateTime& other ) = default;
        DateTime( DateTime&& other ) = default;

        auto operator=( const DateTime& other ) -> DateTime& = default;
        auto operator=( DateTime&& other ) -> DateTime& = default;

        MKT_NODISCARD auto GetTicks() const -> i64;
        MKT_NODISCARD auto GetKind() const -> DateTimeKind;
        MKT_NODISCARD auto GetYear() const -> i32;
        MKT_NODISCARD auto GetMonth() const -> i32;
        MKT_NODISCARD auto GetDay() const -> i32;
        MKT_NODISCARD auto GetDayOfYear() const -> i32;
        MKT_NODISCARD auto GetDayOfWeek() const -> DayOfWeek;
        MKT_NODISCARD auto GetHour() const -> i32;
        MKT_NODISCARD auto GetMinute() const -> i32;
        MKT_NODISCARD auto GetSecond() const -> i32;
        MKT_NODISCARD auto GetMillisecond() const -> i32;
        MKT_NODISCARD auto GetMicrosecond() const -> i32;
        MKT_NODISCARD auto GetNanosecond() const -> i32;
        /** @brief Returns midnight on the same date, retaining Kind. */
        MKT_NODISCARD auto GetDate() const -> DateTime;
        MKT_NODISCARD auto GetTimeOfDay() const -> Duration;

        MKT_NODISCARD static auto MinValue() -> DateTime;
        MKT_NODISCARD static auto MaxValue() -> DateTime;
        MKT_NODISCARD static auto UtcNow() -> DateTime;
        MKT_NODISCARD static auto Now() -> DateTime;
        MKT_NODISCARD static auto Today() -> DateTime;
        MKT_NODISCARD static auto IsLeapYear( i32 year ) -> bool;
        MKT_NODISCARD static auto DaysInMonth( i32 year, i32 month ) -> i32;
        /** @brief Changes interpretation without converting or changing ticks. */
        MKT_NODISCARD static auto SpecifyKind( const DateTime& value, DateTimeKind kind ) -> DateTime;

        /** @brief Adds time, preserving Kind. All arithmetic throws std::out_of_range on overflow. */
        MKT_NODISCARD auto Add( Duration value ) const -> DateTime;
        MKT_NODISCARD auto AddTicks( i64 value ) const -> DateTime;
        MKT_NODISCARD auto AddMicroseconds( f64 value ) const -> DateTime;
        MKT_NODISCARD auto AddMilliseconds( f64 value ) const -> DateTime;
        MKT_NODISCARD auto AddSeconds( f64 value ) const -> DateTime;
        MKT_NODISCARD auto AddMinutes( f64 value ) const -> DateTime;
        MKT_NODISCARD auto AddHours( f64 value ) const -> DateTime;
        MKT_NODISCARD auto AddDays( f64 value ) const -> DateTime;
        /** @brief Calendar arithmetic clamps the day to the destination month's last day, like .NET. */
        MKT_NODISCARD auto AddMonths( i32 months ) const -> DateTime;
        MKT_NODISCARD auto AddYears( i32 years ) const -> DateTime;
        MKT_NODISCARD auto Subtract( Duration value ) const -> DateTime;
        MKT_NODISCARD auto Subtract( const DateTime& value ) const -> Duration;

        /** @brief Converts using the host timezone; Unspecified is treated as UTC for this conversion. */
        MKT_NODISCARD auto ToLocalTime() const -> DateTime;
        /** @brief Converts using the host timezone; Unspecified is treated as local for this conversion. */
        MKT_NODISCARD auto ToUniversalTime() const -> DateTime;
        /** @brief Bridges C++ system-clock instants; returned DateTime has Utc Kind. */
        MKT_NODISCARD static auto FromSystemTime( SystemTime time ) -> DateTime;
        /** @brief Converts to UTC first; 100 ns precision covers the entire DateTime range. */
        MKT_NODISCARD auto ToSystemTime() const -> SystemTime;
        /** @brief Windows FILETIME interoperability: 100 ns ticks since 1601-01-01 UTC. */
        MKT_NODISCARD static auto FromFileTimeUtc( i64 fileTime ) -> DateTime;
        MKT_NODISCARD static auto FromFileTime( i64 fileTime ) -> DateTime;
        /** @brief Local values convert to UTC; Unspecified is interpreted as UTC, matching .NET. */
        MKT_NODISCARD auto ToFileTimeUtc() const -> i64;
        /** @brief Converts to UTC first; dates before 1601 throw std::out_of_range. */
        MKT_NODISCARD auto ToFileTime() const -> i64;

        /** @brief Formats .NET-style invariant standard/custom patterns. Empty format means G. */
        MKT_NODISCARD auto ToString( eastl::string_view format = {} ) const -> eastl::string;
        /** @brief Parses invariant ISO-8601 or invariant numeric dates; offsets normalize to UTC. */
        MKT_NODISCARD static auto Parse( eastl::string_view text ) -> DateTime;
        /** @brief Parses a supported pattern exactly; invalid input throws std::invalid_argument. */
        MKT_NODISCARD static auto ParseExact( eastl::string_view text, eastl::string_view format ) -> DateTime;
        /** @brief On failure returns false and resets result to MinValue. */
        MKT_NODISCARD static auto TryParse( eastl::string_view text, DateTime& result ) noexcept -> bool;
        MKT_NODISCARD static auto TryParseExact( eastl::string_view text, eastl::string_view format, DateTime& result ) noexcept -> bool;

        MKT_NODISCARD auto CompareTo( const DateTime& other ) const -> i32;
        MKT_NODISCARD auto Equals( const DateTime& other ) const -> bool;
        MKT_NODISCARD static auto Compare( const DateTime& left, const DateTime& right ) -> i32;
        MKT_NODISCARD auto operator==( const DateTime& other ) const -> bool;
        MKT_NODISCARD auto operator<=>( const DateTime& other ) const -> std::strong_ordering;
        MKT_NODISCARD auto operator+( Duration value ) const -> DateTime;
        MKT_NODISCARD auto operator-( Duration value ) const -> DateTime;
        MKT_NODISCARD auto operator-( const DateTime& other ) const -> Duration;

        ~DateTime() = default;

    private:
        i64 mTicks{};
        DateTimeKind mKind{ DateTimeKind::eUnspecified };
        // Preserve the first occurrence of a repeated local hour when round-tripping UTC.
        bool mAmbiguousDaylightTime{};
    };

}// namespace mikoto::core

#endif// MIKOTO_CORE_DATETIME_HH
