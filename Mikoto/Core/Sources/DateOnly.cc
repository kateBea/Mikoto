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

#include <stdexcept>

#include <EASTL/array.h>

#include <Core/DateOnly.hh>

namespace mikoto::core {

    // DateOnly shares calendar/formatting behavior with DateTime, but accepts no
    // time or timezone fields. Its standard round-trip/RFC patterns are date-only.
    // https://learn.microsoft.com/en-us/dotnet/api/system.dateonly.tostring
    static auto DateOnlyFormat( eastl::string_view format ) -> eastl::string {
        if ( format.empty() ) {
            return "MM/dd/yyyy";
        }
        if ( format.size() == 1 ) {
            switch ( format[0] ) {
                case 'd': return "MM/dd/yyyy";
                case 'D': return "dddd, dd MMMM yyyy";
                case 'o': case 'O': return "yyyy-MM-dd";
                case 'r': case 'R': return "ddd, dd MMM yyyy";
                case 'm': case 'M': return "MMMM dd";
                case 'y': case 'Y': return "yyyy MMMM";
                default: throw std::invalid_argument{ "Unsupported DateOnly standard format." };
            }
        }

        char quote{};
        for ( usize index{}; index < format.size(); ++index ) {
            const char symbol{ format[index] };

            if ( symbol == '\\' ) {
                ++index;
                continue;
            }
            if ( quote != 0 ) {
                if ( symbol == quote ) {
                    quote = 0;
                }
                continue;
            }
            if ( symbol == '\'' || symbol == '"' ) {
                quote = symbol;
                continue;
            }
            if ( eastl::string_view{ "HhmsfFtzK" }.find( symbol ) != eastl::string_view::npos ) {
                throw std::invalid_argument{ "DateOnly formats cannot contain time or timezone fields." };
            }
        }

        return eastl::string{ format };
    }

    DateOnly::DateOnly( i32 year, i32 month, i32 day )
        : mDayNumber{ static_cast<i32>( DateTime{ year, month, day }.GetTicks() / DateTime::kTicksPerDay ) } {
    }

    auto DateOnly::GetYear() const -> i32 {
        return ToDateTime().GetYear();
    }

    auto DateOnly::GetMonth() const -> i32 {
        return ToDateTime().GetMonth();
    }

    auto DateOnly::GetDay() const -> i32 {
        return ToDateTime().GetDay();
    }

    auto DateOnly::GetDayOfYear() const -> i32 {
        return ToDateTime().GetDayOfYear();
    }

    auto DateOnly::GetDayOfWeek() const -> DayOfWeek {
        return static_cast<DayOfWeek>( ( mDayNumber + 1 ) % 7 );
    }

    auto DateOnly::GetDayNumber() const -> i32 {
        return mDayNumber;
    }

    auto DateOnly::MinValue() -> DateOnly {
        return DateOnly{};
    }

    auto DateOnly::MaxValue() -> DateOnly {
        return FromDayNumber( kMaxDayNumber );
    }

    auto DateOnly::FromDayNumber( i32 dayNumber ) -> DateOnly {
        if ( dayNumber < 0 || dayNumber > kMaxDayNumber ) {
            throw std::out_of_range{ "DateOnly day number must be in 0..3652058." };
        }

        DateOnly result{};
        result.mDayNumber = dayNumber;
        return result;
    }

    auto DateOnly::FromDateTime( const DateTime& value ) -> DateOnly {
        return FromDayNumber( static_cast<i32>( value.GetTicks() / DateTime::kTicksPerDay ) );
    }

    auto DateOnly::AddDays( i32 days ) const -> DateOnly {
        if ( days < -mDayNumber || days > kMaxDayNumber - mDayNumber ) {
            throw std::out_of_range{ "DateOnly addition exceeds years 1..9999." };
        }
        return FromDayNumber( mDayNumber + days );
    }

    auto DateOnly::AddMonths( i32 months ) const -> DateOnly {
        return FromDateTime( ToDateTime().AddMonths( months ) );
    }

    auto DateOnly::AddYears( i32 years ) const -> DateOnly {
        return FromDateTime( ToDateTime().AddYears( years ) );
    }

    auto DateOnly::ToDateTime( DateTime::Duration timeOfDay, DateTimeKind kind ) const -> DateTime {
        if ( timeOfDay.count() < 0 || timeOfDay.count() >= DateTime::kTicksPerDay ) {
            throw std::out_of_range{ "Time of day must be within [00:00, 24:00)." };
        }
        return DateTime{ mDayNumber * DateTime::kTicksPerDay + timeOfDay.count(), kind };
    }

    auto DateOnly::ToDateTime( i32 hour, i32 minute, i32 second, i32 millisecond, DateTimeKind kind ) const -> DateTime {
        return DateTime{ GetYear(), GetMonth(), GetDay(), hour, minute, second, millisecond, kind };
    }

    auto DateOnly::ToString( eastl::string_view format ) const -> eastl::string {
        return ToDateTime().ToString( DateOnlyFormat( format ) );
    }

    auto DateOnly::Parse( eastl::string_view text ) -> DateOnly {
        DateOnly result{};
        if ( !TryParse( text, result ) ) {
            throw std::invalid_argument{ "Unsupported or invalid invariant DateOnly text." };
        }
        return result;
    }

    auto DateOnly::ParseExact( eastl::string_view text, eastl::string_view format ) -> DateOnly {
        DateOnly result{};
        if ( !TryParseExact( text, format, result ) ) {
            throw std::invalid_argument{ "DateOnly text does not match the requested format." };
        }
        return result;
    }

    auto DateOnly::TryParse( eastl::string_view text, DateOnly& result ) noexcept -> bool {
        static const eastl::array<eastl::string_view, 4> formats{ "yyyy-MM-dd", "MM/dd/yyyy", "D", "R" };
        const auto whitespace{ []( char value ) { return value == ' ' || value == '\t' || value == '\r' || value == '\n'; } };

        while ( !text.empty() && whitespace( text.front() ) ) {
            text.remove_prefix( 1 );
        }
        while ( !text.empty() && whitespace( text.back() ) ) {
            text.remove_suffix( 1 );
        }

        for ( const auto format: formats ) {
            if ( TryParseExact( text, format, result ) ) {
                return true;
            }
        }
        result = MinValue();
        return false;
    }

    auto DateOnly::TryParseExact( eastl::string_view text, eastl::string_view format, DateOnly& result ) noexcept -> bool {
        try {
            if ( format.empty() ) {
                throw std::invalid_argument{ "ParseExact requires a date format." };
            }
            result = FromDateTime( DateTime::ParseExact( text, DateOnlyFormat( format ) ) );
            return true;
        } catch ( const std::exception& ) {
            result = MinValue();
            return false;
        }
    }

    auto DateOnly::CompareTo( const DateOnly& other ) const -> i32 {
        return Compare( *this, other );
    }

    auto DateOnly::Equals( const DateOnly& other ) const -> bool {
        return *this == other;
    }

    auto DateOnly::Compare( const DateOnly& left, const DateOnly& right ) -> i32 {
        return left.mDayNumber < right.mDayNumber ? -1 : left.mDayNumber > right.mDayNumber ? 1 : 0;
    }

}// namespace mikoto::core
