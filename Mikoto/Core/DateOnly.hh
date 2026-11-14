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

#ifndef MIKOTO_CORE_DATE_ONLY_HH
#define MIKOTO_CORE_DATE_ONLY_HH

#include <Core/DateTime.hh>

namespace mikoto::core {

    /**
     * @brief Immutable Gregorian calendar date, following System.DateOnly.
     * @details Represents 0001-01-01 through 9999-12-31 without a time or timezone.
     * DayNumber is zero-based from 0001-01-01. Formatting uses invariant English
     * patterns; see DateTime.md. Arithmetic returns a new value.
     * @see https://learn.microsoft.com/en-us/dotnet/api/system.dateonly
     * @see https://github.com/dotnet/runtime/blob/main/src/libraries/System.Private.CoreLib/src/System/DateOnly.cs
     */
    class DateOnly {
    public:
        static constexpr i32 kMaxDayNumber{ 3652058 };

        /** @brief Constructs MinValue (0001-01-01). */
        DateOnly() = default;
        /** @brief Constructs a valid date; invalid fields throw std::out_of_range. */
        DateOnly( i32 year, i32 month, i32 day );

        DateOnly( const DateOnly& other ) = default;
        DateOnly( DateOnly&& other ) = default;

        auto operator=( const DateOnly& other ) -> DateOnly& = default;
        auto operator=( DateOnly&& other ) -> DateOnly& = default;

        MKT_NODISCARD auto GetYear() const -> i32;
        MKT_NODISCARD auto GetMonth() const -> i32;
        MKT_NODISCARD auto GetDay() const -> i32;
        MKT_NODISCARD auto GetDayOfYear() const -> i32;
        MKT_NODISCARD auto GetDayOfWeek() const -> DayOfWeek;
        MKT_NODISCARD auto GetDayNumber() const -> i32;

        MKT_NODISCARD static auto MinValue() -> DateOnly;
        MKT_NODISCARD static auto MaxValue() -> DateOnly;
        /** @brief Creates a date from a zero-based day number in 0..3652058. */
        MKT_NODISCARD static auto FromDayNumber( i32 dayNumber ) -> DateOnly;
        /** @brief Takes the calendar fields without performing a timezone conversion. */
        MKT_NODISCARD static auto FromDateTime( const DateTime& value ) -> DateOnly;

        /** @brief Adds whole days; out-of-range results throw std::out_of_range. */
        MKT_NODISCARD auto AddDays( i32 days ) const -> DateOnly;
        /** @brief Adds calendar months/years, clamping the day as System.DateOnly does. */
        MKT_NODISCARD auto AddMonths( i32 months ) const -> DateOnly;
        MKT_NODISCARD auto AddYears( i32 years ) const -> DateOnly;
        /** @brief Combines the date with a time in [00:00, 24:00); Duration replaces .NET TimeOnly. */
        MKT_NODISCARD auto ToDateTime( DateTime::Duration timeOfDay = {}, DateTimeKind kind = DateTimeKind::eUnspecified ) const -> DateTime;
        MKT_NODISCARD auto ToDateTime( i32 hour, i32 minute, i32 second = 0, i32 millisecond = 0, DateTimeKind kind = DateTimeKind::eUnspecified ) const -> DateTime;

        /** @brief Supports d/D, o/O, r/R, m/M, y/Y and date-only custom patterns. Empty means d. */
        MKT_NODISCARD auto ToString( eastl::string_view format = {} ) const -> eastl::string;
        MKT_NODISCARD static auto Parse( eastl::string_view text ) -> DateOnly;
        MKT_NODISCARD static auto ParseExact( eastl::string_view text, eastl::string_view format ) -> DateOnly;
        /** @brief On failure returns false and resets result to MinValue. */
        MKT_NODISCARD static auto TryParse( eastl::string_view text, DateOnly& result ) noexcept -> bool;
        MKT_NODISCARD static auto TryParseExact( eastl::string_view text, eastl::string_view format, DateOnly& result ) noexcept -> bool;

        MKT_NODISCARD auto CompareTo( const DateOnly& other ) const -> i32;
        MKT_NODISCARD auto Equals( const DateOnly& other ) const -> bool;
        MKT_NODISCARD static auto Compare( const DateOnly& left, const DateOnly& right ) -> i32;
        MKT_NODISCARD auto operator==( const DateOnly& other ) const -> bool = default;
        MKT_NODISCARD auto operator<=>( const DateOnly& other ) const -> std::strong_ordering = default;

        ~DateOnly() = default;

    private:
        i32 mDayNumber{};
    };

}// namespace mikoto::core

#endif// MIKOTO_CORE_DATE_ONLY_HH
