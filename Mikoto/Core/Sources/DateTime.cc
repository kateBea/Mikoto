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

#include <ctime>
#include <cerrno>
#include <limits>
#include <stdexcept>

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <fmt/format.h>

#include <glm/common.hpp>

#include <Core/DateTime.hh>

namespace mikoto::core {

    // Gregorian calculations use the standard calendar rather than platform C
    // time functions, which cannot represent the full .NET range everywhere.
    // https://learn.microsoft.com/en-us/cpp/standard-library/year-month-day-class
    static auto CalendarDate( i32 year, i32 month, i32 day ) -> std::chrono::year_month_day {
        if ( year < 1 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31 ) {
            throw std::out_of_range{ "Date fields are outside the Gregorian range." };
        }

        const std::chrono::year_month_day date{ std::chrono::year{ year },
            std::chrono::month{ static_cast<unsigned>( month ) }, std::chrono::day{ static_cast<unsigned>( day ) } };

        if ( !date.ok() ) {
            throw std::out_of_range{ "The day does not exist in this month." };
        }

        return date;
    }

    static auto CalendarTicks( i32 year, i32 month, i32 day ) -> i64 {
        const auto date{ std::chrono::sys_days{ CalendarDate( year, month, day ) } };
        return date.time_since_epoch().count() * DateTime::kTicksPerDay + DateTime::kUnixEpochTicks;
    }

    static auto CalendarFields( const DateTime& value ) -> std::chrono::year_month_day {
        const auto days{ value.GetTicks() / DateTime::kTicksPerDay - DateTime::kUnixEpochTicks / DateTime::kTicksPerDay };
        return std::chrono::year_month_day{ std::chrono::sys_days{ std::chrono::days{ days } } };
    }

    static auto AddUnits( const DateTime& value, f64 amount, i64 ticksPerUnit ) -> DateTime {
        const f64 limit{ static_cast<f64>( DateTime::kMaxTicks ) / static_cast<f64>( ticksPerUnit ) };

        if ( !( amount >= -limit && amount <= limit ) ) {
            throw std::out_of_range{ "DateTime addition is outside the supported range." };
        }

        // Preserve integer precision before truncating the fractional tick, as .NET does.
        const f64 integral{ glm::trunc( amount ) };
        const i64 ticks{ static_cast<i64>( integral ) * ticksPerUnit +
                         static_cast<i64>( ( amount - integral ) * static_cast<f64>( ticksPerUnit ) ) };
        return value.AddTicks( ticks );
    }

#if __cpp_lib_chrono >= 201907L
    static auto LocalOffset( const DateTime& value, bool daylight = false ) -> std::chrono::seconds {
        const std::chrono::local_time<DateTime::Duration> local{ DateTime::Duration{ value.GetTicks() - DateTime::kUnixEpochTicks } };
        const auto info{ std::chrono::current_zone()->get_info( local ) };

        if ( info.result == std::chrono::local_info::unique ) {
            return info.first.offset;
        }

        if ( info.result == std::chrono::local_info::ambiguous && daylight ) {
            return info.first.offset;
        }

        // .NET assumes standard time for ambiguous/invalid manually created local times.
        return info.first.save == std::chrono::minutes{ 0 } ? info.first.offset : info.second.offset;
    }
#else
    // Older standard libraries have calendars but no timezone database. Keep the
    // fallback thread-safe and let the host C runtime define its supported range.
    static auto LocalFields( std::time_t time ) -> std::tm {
        std::tm fields{};
#if defined( _MSC_VER )
        if ( localtime_s( &fields, &time ) != 0 ) {
#else
        if ( !localtime_r( &time, &fields ) ) {
#endif
            throw std::out_of_range{ "The host timezone cannot represent this date." };
        }
        return fields;
    }

    static auto MatchesLocalFields( const std::tm& fields, const DateTime& value ) -> bool {
        return fields.tm_year + 1900 == value.GetYear() && fields.tm_mon + 1 == value.GetMonth() &&
               fields.tm_mday == value.GetDay() && fields.tm_hour == value.GetHour() &&
               fields.tm_min == value.GetMinute() && fields.tm_sec == value.GetSecond();
    }

    static auto LocalOffset( const DateTime& value, bool daylight = false ) -> std::chrono::seconds {
        std::tm fields{};
        fields.tm_year = value.GetYear() - 1900;
        fields.tm_mon = value.GetMonth() - 1;
        fields.tm_mday = value.GetDay();
        fields.tm_hour = value.GetHour();
        fields.tm_min = value.GetMinute();
        fields.tm_sec = value.GetSecond();
        fields.tm_isdst = -1;

        errno = 0;
        auto instant{ std::mktime( &fields ) };

        if ( instant == std::time_t{ -1 } && errno == EOVERFLOW ) {
            throw std::out_of_range{ "The host timezone cannot represent this date." };
        }

        auto preferred{ fields };
        preferred.tm_year = value.GetYear() - 1900;
        preferred.tm_mon = value.GetMonth() - 1;
        preferred.tm_mday = value.GetDay();
        preferred.tm_hour = value.GetHour();
        preferred.tm_min = value.GetMinute();
        preferred.tm_sec = value.GetSecond();
        preferred.tm_isdst = daylight ? 1 : 0;
        const auto candidate{ std::mktime( &preferred ) };

        if ( MatchesLocalFields( LocalFields( candidate ), value ) ) {
            instant = candidate;
        }

        const auto wallSeconds{ std::chrono::floor<std::chrono::seconds>(
            DateTime::Duration{ value.GetTicks() - DateTime::kUnixEpochTicks } ).count() };
        return std::chrono::seconds{ wallSeconds - static_cast<i64>( instant ) };
    }
#endif

    struct DateToken {
        char mSymbol{};
        usize mCount{};
        eastl::string mLiteral{};
    };

    static const eastl::array<eastl::string_view, 12> kMonthNames{
        "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"
    };
    static const eastl::array<eastl::string_view, 7> kDayNames{
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };

    // Invariant patterns mirror .NET; culture providers and alternate calendars
    // deliberately do not enter Core's small date/time API.
    // https://learn.microsoft.com/en-us/dotnet/standard/base-types/standard-date-and-time-format-strings
    static auto ExpandDateFormat( eastl::string_view format ) -> eastl::string {
        if ( format.empty() ) {
            return "MM/dd/yyyy HH:mm:ss";
        }
        if ( format.size() != 1 ) {
            return eastl::string{ format };
        }

        switch ( format[0] ) {
            case 'd': return "MM/dd/yyyy";
            case 'D': return "dddd, dd MMMM yyyy";
            case 't': return "HH:mm";
            case 'T': return "HH:mm:ss";
            case 'g': return "MM/dd/yyyy HH:mm";
            case 'G': return "MM/dd/yyyy HH:mm:ss";
            case 'f': return "dddd, dd MMMM yyyy HH:mm";
            case 'F': return "dddd, dd MMMM yyyy HH:mm:ss";
            case 'm': case 'M': return "MMMM dd";
            case 'y': case 'Y': return "yyyy MMMM";
            case 'o': case 'O': return "yyyy-MM-dd'T'HH:mm:ss.fffffffK";
            case 's': return "yyyy-MM-dd'T'HH:mm:ss";
            case 'u': return "yyyy-MM-dd HH:mm:ss'Z'";
            case 'r': case 'R': return "ddd, dd MMM yyyy HH:mm:ss 'GMT'";
            default: throw std::invalid_argument{ "Unsupported DateTime standard format." };
        }
    }

    static auto TokenizeDateFormat( eastl::string_view format ) -> eastl::vector<DateToken> {
        eastl::vector<DateToken> tokens{};

        for ( usize index{}; index < format.size(); ) {
            const char symbol{ format[index++] };
            DateToken token{};

            if ( symbol == '\'' || symbol == '"' ) {
                bool closed{};

                while ( index < format.size() ) {
                    char next{ format[index++] };
                    if ( next == symbol ) {
                        closed = true;
                        break;
                    }
                    if ( next == '\\' && index < format.size() ) {
                        next = format[index++];
                    }
                    token.mLiteral += next;
                }

                if ( !closed ) {
                    throw std::invalid_argument{ "Unterminated date format literal." };
                }
            }
            else if ( symbol == '\\' ) {
                if ( index == format.size() ) {
                    throw std::invalid_argument{ "A date format escape needs a character." };
                }
                token.mLiteral += format[index++];
            }
            else {
                char field{ symbol };
                const bool single{ field == '%' };

                if ( single ) {
                    if ( index == format.size() || format[index] == '%' ) {
                        throw std::invalid_argument{ "Invalid single date format specifier." };
                    }
                    field = format[index++];
                }

                if ( eastl::string_view{ "dMyHhmsfFtzKg" }.find( field ) != eastl::string_view::npos ) {
                    token.mSymbol = field;
                    token.mCount = 1;

                    while ( !single && index < format.size() && format[index] == field ) {
                        ++index;
                        ++token.mCount;
                    }

                    if ( ( ( field == 'f' || field == 'F' ) && token.mCount > 7 ) ||
                         ( field == 'z' && token.mCount > 3 ) || ( field == 'K' && token.mCount != 1 ) ) {
                        throw std::invalid_argument{ "Invalid date format field length." };
                    }
                }
                else {
                    token.mLiteral += field;
                }
            }

            tokens.push_back( eastl::move( token ) );
        }

        return tokens;
    }

    static auto AppendNumber( eastl::string& result, i64 number, usize width ) -> void {
        result += fmt::format( "{:0{}}", number, width ).c_str();
    }

    static auto AppendOffset( eastl::string& result, std::chrono::seconds offset, usize width ) -> void {
        const i64 totalMinutes{ offset.count() / 60 };
        const i64 magnitude{ glm::abs( totalMinutes ) };
        result += totalMinutes < 0 ? '-' : '+';
        AppendNumber( result, magnitude / 60, width == 1 ? 1 : 2 );

        if ( width == 3 ) {
            result += ':';
            AppendNumber( result, magnitude % 60, 2 );
        }
    }

    static auto FormatDate( const DateTime& value, eastl::string_view format ) -> eastl::string {
        const auto expanded{ ExpandDateFormat( format ) };
        const auto tokens{ TokenizeDateFormat( expanded ) };
        eastl::string result{};

        for ( const auto& token: tokens ) {
            const auto count{ token.mCount };

            switch ( token.mSymbol ) {
                case 'd':
                    if ( count <= 2 ) {
                        AppendNumber( result, value.GetDay(), count );
                    }
                    else {
                        const auto name{ kDayNames[static_cast<usize>( value.GetDayOfWeek() )] };
                        result.append( name.data(), count == 3 ? 3 : name.size() );
                    }
                    break;
                case 'M':
                    if ( count <= 2 ) {
                        AppendNumber( result, value.GetMonth(), count );
                    }
                    else {
                        const auto name{ kMonthNames[value.GetMonth() - 1] };
                        result.append( name.data(), count == 3 ? 3 : name.size() );
                    }
                    break;
                case 'y': AppendNumber( result, count <= 2 ? value.GetYear() % 100 : value.GetYear(), count ); break;
                case 'H': AppendNumber( result, value.GetHour(), glm::min( count, usize{ 2 } ) ); break;
                case 'h': AppendNumber( result, ( value.GetHour() + 11 ) % 12 + 1, glm::min( count, usize{ 2 } ) ); break;
                case 'm': AppendNumber( result, value.GetMinute(), glm::min( count, usize{ 2 } ) ); break;
                case 's': AppendNumber( result, value.GetSecond(), glm::min( count, usize{ 2 } ) ); break;
                case 't': result.append( value.GetHour() < 12 ? "AM" : "PM", count == 1 ? 1 : 2 ); break;
                case 'g': result += "A.D."; break;
                case 'f': case 'F': {
                    const auto fraction{ fmt::format( "{:07}", value.GetTicks() % DateTime::kTicksPerSecond ) };
                    usize length{ count };

                    while ( token.mSymbol == 'F' && length > 0 && fraction[length - 1] == '0' ) {
                        --length;
                    }
                    if ( length == 0 && !result.empty() && result.back() == '.' ) {
                        result.pop_back();
                    }
                    result.append( fraction.data(), length );
                    break;
                }
                case 'z': {
                    const auto offset{ std::chrono::duration_cast<std::chrono::seconds>(
                        DateTime::Duration{ value.GetTicks() - DateTime::kUnixEpochTicks } - value.ToSystemTime().time_since_epoch() ) };
                    AppendOffset( result, value.GetKind() == DateTimeKind::eUtc ? std::chrono::seconds{} : offset, count );
                    break;
                }
                case 'K':
                    if ( value.GetKind() == DateTimeKind::eUtc ) {
                        result += 'Z';
                    }
                    else if ( value.GetKind() == DateTimeKind::eLocal ) {
                        // Recover the correct offset even for a repeated local hour.
                        const auto offset{ std::chrono::duration_cast<std::chrono::seconds>(
                            DateTime::Duration{ value.GetTicks() - DateTime::kUnixEpochTicks } - value.ToSystemTime().time_since_epoch() ) };
                        AppendOffset( result, offset, 3 );
                    }
                    break;
                default: result += token.mLiteral; break;
            }
        }

        return result;
    }

    static auto EqualAscii( eastl::string_view left, eastl::string_view right ) -> bool {
        if ( left.size() != right.size() ) {
            return false;
        }
        for ( usize index{}; index < left.size(); ++index ) {
            const auto lower{ []( char value ) { return value >= 'A' && value <= 'Z' ? value + ( 'a' - 'A' ) : value; } };
            if ( lower( left[index] ) != lower( right[index] ) ) {
                return false;
            }
        }
        return true;
    }

    static auto ReadNumber( eastl::string_view text, usize& position, usize minimum, usize maximum ) -> i32 {
        const usize begin{ position };
        i32 result{};

        while ( position < text.size() && position - begin < maximum && text[position] >= '0' && text[position] <= '9' ) {
            const i32 digit{ text[position++] - '0' };
            if ( result > ( std::numeric_limits<i32>::max() - digit ) / 10 ) {
                throw std::out_of_range{ "Date/time number is too large." };
            }
            result = result * 10 + digit;
        }

        if ( position - begin < minimum ) {
            throw std::invalid_argument{ "Missing digits in date/time." };
        }

        return result;
    }

    static auto ReadName( eastl::string_view text, usize& position, const auto& names, bool abbreviated ) -> i32 {
        for ( usize index{}; index < names.size(); ++index ) {
            const usize length{ abbreviated ? 3 : names[index].size() };

            if ( EqualAscii( text.substr( position, length ), names[index].substr( 0, length ) ) ) {
                position += length;
                return static_cast<i32>( index );
            }
        }
        throw std::invalid_argument{ "Unknown invariant month/day name." };
    }

    static auto ReadOffset( eastl::string_view text, usize& position, usize width ) -> i32 {
        if ( position == text.size() || ( text[position] != '+' && text[position] != '-' ) ) {
            throw std::invalid_argument{ "A timezone offset needs a sign." };
        }

        const bool negative{ text[position++] == '-' };
        const auto hours{ ReadNumber( text, position, width == 1 ? 1 : 2, 2 ) };
        i32 minutes{};

        if ( width == 3 ) {
            if ( position == text.size() || text[position++] != ':' ) {
                throw std::invalid_argument{ "A timezone offset needs HH:mm." };
            }
            minutes = ReadNumber( text, position, 2, 2 );
        }

        if ( hours > 14 || minutes > 59 || ( hours == 14 && minutes != 0 ) ) {
            throw std::out_of_range{ "Timezone offset exceeds fourteen hours." };
        }
        return ( negative ? -1 : 1 ) * ( hours * 60 + minutes );
    }

    // Token parsing is intentionally invariant and exact. Parse() tries a small
    // documented set of ISO/numeric layouts rather than guessing the user's culture.
    // https://learn.microsoft.com/en-us/dotnet/standard/base-types/custom-date-and-time-format-strings
    static auto ParseDate( eastl::string_view text, eastl::string_view format ) -> DateTime {
        const auto expanded{ ExpandDateFormat( format ) };
        const auto tokens{ TokenizeDateFormat( expanded ) };
        i32 year{}, month{ 1 }, day{ 1 }, hour{}, minute{}, second{}, expectedWeekday{ -1 }, meridiem{ -1 };
        i32 offsetMinutes{};
        i64 fraction{};
        bool hasYear{}, hasMonth{}, hasDay{}, twelveHour{}, hasOffset{};
        usize position{};

        for ( usize index{}; index < tokens.size(); ++index ) {
            const auto& token{ tokens[index] };
            const usize count{ token.mCount };
            const usize digits{ glm::min( count, usize{ 2 } ) };

            switch ( token.mSymbol ) {
                case 'y':
                    year = ReadNumber( text, position, count == 1 ? 1 : count, count <= 2 ? 2 : count );
                    if ( count <= 2 ) {
                        year += year <= 49 ? 2000 : 1900;
                    }
                    hasYear = true;
                    break;
                case 'M':
                    month = count <= 2 ? ReadNumber( text, position, digits, 2 ) : ReadName( text, position, kMonthNames, count == 3 ) + 1;
                    hasMonth = true;
                    break;
                case 'd':
                    if ( count <= 2 ) {
                        day = ReadNumber( text, position, digits, 2 );
                        hasDay = true;
                    }
                    else {
                        expectedWeekday = ReadName( text, position, kDayNames, count == 3 );
                    }
                    break;
                case 'H': hour = ReadNumber( text, position, digits, 2 ); break;
                case 'h': hour = ReadNumber( text, position, digits, 2 ); twelveHour = true; break;
                case 'm': minute = ReadNumber( text, position, digits, 2 ); break;
                case 's': second = ReadNumber( text, position, digits, 2 ); break;
                case 'f': case 'F': {
                    const usize begin{ position };
                    fraction = ReadNumber( text, position, token.mSymbol == 'f' ? count : 0, count );
                    for ( usize length{ position - begin }; length < 7; ++length ) {
                        fraction *= 10;
                    }
                    break;
                }
                case 't': {
                    const usize length{ count == 1 ? 1u : 2u };
                    const auto value{ text.substr( position, length ) };
                    meridiem = EqualAscii( value, length == 1 ? "A" : "AM" ) ? 0 : EqualAscii( value, length == 1 ? "P" : "PM" ) ? 1 : -1;
                    if ( meridiem == -1 ) {
                        throw std::invalid_argument{ "Expected AM or PM." };
                    }
                    position += length;
                    break;
                }
                case 'z': offsetMinutes = ReadOffset( text, position, count ); hasOffset = true; break;
                case 'K':
                    if ( position < text.size() && text[position] == 'Z' ) {
                        ++position;
                        hasOffset = true;
                    }
                    else if ( position < text.size() && ( text[position] == '+' || text[position] == '-' ) ) {
                        offsetMinutes = ReadOffset( text, position, 3 );
                        hasOffset = true;
                    }
                    break;
                case 'g':
                    if ( !EqualAscii( text.substr( position, 4 ), "A.D." ) ) {
                        throw std::invalid_argument{ "Expected the A.D. era." };
                    }
                    position += 4;
                    break;
                default:
                    if ( text.substr( position, token.mLiteral.size() ) != token.mLiteral ) {
                        // F suppresses its preceding decimal separator when no fraction remains.
                        if ( token.mLiteral == "." && index + 1 < tokens.size() && tokens[index + 1].mSymbol == 'F' ) {
                            break;
                        }
                        throw std::invalid_argument{ "Date/time does not match the format." };
                    }
                    position += token.mLiteral.size();
                    break;
            }
        }

        if ( position != text.size() ) {
            throw std::invalid_argument{ "Trailing characters in date/time." };
        }

        if ( !hasYear ) {
            const auto today{ DateTime::Today() };
            year = today.GetYear();
            if ( !hasMonth && !hasDay ) {
                month = today.GetMonth();
                day = today.GetDay();
            }
        }

        if ( twelveHour ) {
            if ( hour < 1 || hour > 12 ) {
                throw std::out_of_range{ "A twelve-hour clock requires hours 1..12." };
            }
            hour = hour % 12 + ( meridiem == 1 ? 12 : 0 );
        }
        else if ( meridiem != -1 && ( hour < 12 ? 0 : 1 ) != meridiem ) {
            throw std::invalid_argument{ "AM/PM disagrees with the hour." };
        }

        DateTime result{ DateTime{ year, month, day, hour, minute, second }.AddTicks( fraction ) };
        if ( expectedWeekday != -1 && static_cast<i32>( result.GetDayOfWeek() ) != expectedWeekday ) {
            throw std::invalid_argument{ "Weekday does not match the date." };
        }

        if ( hasOffset ) {
            return DateTime::SpecifyKind( result.AddTicks( -static_cast<i64>( offsetMinutes ) * DateTime::kTicksPerMinute ), DateTimeKind::eUtc );
        }
        if ( format.size() == 1 && ( format[0] == 'u' || format[0] == 'r' || format[0] == 'R' ) ) {
            return DateTime::SpecifyKind( result, DateTimeKind::eUtc );
        }

        return result;
    }

    DateTime::DateTime( i64 ticks, DateTimeKind kind ) : mTicks{ ticks }, mKind{ kind } {
        if ( ticks < 0 || ticks > kMaxTicks ) {
            throw std::out_of_range{ "DateTime ticks must represent years 1..9999." };
        }
        if ( kind != DateTimeKind::eUnspecified && kind != DateTimeKind::eUtc && kind != DateTimeKind::eLocal ) {
            throw std::invalid_argument{ "Invalid DateTimeKind." };
        }
    }

    DateTime::DateTime( i32 year, i32 month, i32 day )
        : DateTime{ CalendarTicks( year, month, day ) } {
    }

    DateTime::DateTime( i32 year, i32 month, i32 day, i32 hour, i32 minute, i32 second, DateTimeKind kind )
        : DateTime{ year, month, day, hour, minute, second, 0, 0, kind } {
    }

    DateTime::DateTime( i32 year, i32 month, i32 day, i32 hour, i32 minute, i32 second, i32 millisecond, DateTimeKind kind )
        : DateTime{ year, month, day, hour, minute, second, millisecond, 0, kind } {
    }

    DateTime::DateTime( i32 year, i32 month, i32 day, i32 hour, i32 minute, i32 second, i32 millisecond, i32 microsecond, DateTimeKind kind )
        : DateTime{ CalendarTicks( year, month, day ), kind } {
        if ( hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59 ||
             millisecond < 0 || millisecond > 999 || microsecond < 0 || microsecond > 999 ) {
            throw std::out_of_range{ "Invalid DateTime time-of-day fields." };
        }

        mTicks += hour * kTicksPerHour + minute * kTicksPerMinute + second * kTicksPerSecond +
                  millisecond * kTicksPerMillisecond + microsecond * kTicksPerMicrosecond;
    }

    auto DateTime::GetTicks() const -> i64 {
        return mTicks;
    }

    auto DateTime::GetKind() const -> DateTimeKind {
        return mKind;
    }

    auto DateTime::GetYear() const -> i32 {
        return static_cast<i32>( CalendarFields( *this ).year() );
    }

    auto DateTime::GetMonth() const -> i32 {
        return static_cast<unsigned>( CalendarFields( *this ).month() );
    }

    auto DateTime::GetDay() const -> i32 {
        return static_cast<unsigned>( CalendarFields( *this ).day() );
    }

    auto DateTime::GetDayOfYear() const -> i32 {
        return static_cast<i32>( ( mTicks - CalendarTicks( GetYear(), 1, 1 ) ) / kTicksPerDay ) + 1;
    }

    auto DateTime::GetDayOfWeek() const -> DayOfWeek {
        // 0001-01-01 was Monday; .NET numbers Sunday as zero.
        return static_cast<DayOfWeek>( ( mTicks / kTicksPerDay + 1 ) % 7 );
    }

    auto DateTime::GetHour() const -> i32 {
        return static_cast<i32>( mTicks / kTicksPerHour % 24 );
    }

    auto DateTime::GetMinute() const -> i32 {
        return static_cast<i32>( mTicks / kTicksPerMinute % 60 );
    }

    auto DateTime::GetSecond() const -> i32 {
        return static_cast<i32>( mTicks / kTicksPerSecond % 60 );
    }

    auto DateTime::GetMillisecond() const -> i32 {
        return static_cast<i32>( mTicks / kTicksPerMillisecond % 1000 );
    }

    auto DateTime::GetMicrosecond() const -> i32 {
        return static_cast<i32>( mTicks / kTicksPerMicrosecond % 1000 );
    }

    auto DateTime::GetNanosecond() const -> i32 {
        return static_cast<i32>( mTicks % kTicksPerMicrosecond ) * 100;
    }

    auto DateTime::GetDate() const -> DateTime {
        auto result{ *this };
        result.mTicks -= mTicks % kTicksPerDay;
        return result;
    }

    auto DateTime::GetTimeOfDay() const -> Duration {
        return Duration{ mTicks % kTicksPerDay };
    }

    auto DateTime::MinValue() -> DateTime {
        return DateTime{};
    }

    auto DateTime::MaxValue() -> DateTime {
        return DateTime{ kMaxTicks };
    }

    auto DateTime::UtcNow() -> DateTime {
        return FromSystemTime( std::chrono::floor<Duration>( std::chrono::system_clock::now() ) );
    }

    auto DateTime::Now() -> DateTime {
        return UtcNow().ToLocalTime();
    }

    auto DateTime::Today() -> DateTime {
        return Now().GetDate();
    }

    auto DateTime::IsLeapYear( i32 year ) -> bool {
        return CalendarDate( year, 1, 1 ).year().is_leap();
    }

    auto DateTime::DaysInMonth( i32 year, i32 month ) -> i32 {
        const auto date{ CalendarDate( year, month, 1 ) };
        const std::chrono::year_month_day_last last{ date.year(), std::chrono::month_day_last{ date.month() } };
        return static_cast<unsigned>( last.day() );
    }

    auto DateTime::SpecifyKind( const DateTime& value, DateTimeKind kind ) -> DateTime {
        return DateTime{ value.mTicks, kind };
    }

    auto DateTime::Add( Duration value ) const -> DateTime {
        return AddTicks( value.count() );
    }

    auto DateTime::AddTicks( i64 value ) const -> DateTime {
        if ( value > kMaxTicks - mTicks || value < -mTicks ) {
            throw std::out_of_range{ "DateTime addition exceeds years 1..9999." };
        }

        auto result{ *this };
        result.mTicks += value;
        return result;
    }

    auto DateTime::AddMicroseconds( f64 value ) const -> DateTime {
        return AddUnits( *this, value, kTicksPerMicrosecond );
    }

    auto DateTime::AddMilliseconds( f64 value ) const -> DateTime {
        return AddUnits( *this, value, kTicksPerMillisecond );
    }

    auto DateTime::AddSeconds( f64 value ) const -> DateTime {
        return AddUnits( *this, value, kTicksPerSecond );
    }

    auto DateTime::AddMinutes( f64 value ) const -> DateTime {
        return AddUnits( *this, value, kTicksPerMinute );
    }

    auto DateTime::AddHours( f64 value ) const -> DateTime {
        return AddUnits( *this, value, kTicksPerHour );
    }

    auto DateTime::AddDays( f64 value ) const -> DateTime {
        return AddUnits( *this, value, kTicksPerDay );
    }

    auto DateTime::AddMonths( i32 months ) const -> DateTime {
        if ( months < -120000 || months > 120000 ) {
            throw std::out_of_range{ "Month increment must be between -120000 and 120000." };
        }

        const i32 monthNumber{ ( GetYear() - 1 ) * 12 + GetMonth() - 1 + months };
        if ( monthNumber < 0 || monthNumber >= 9999 * 12 ) {
            throw std::out_of_range{ "DateTime addition exceeds years 1..9999." };
        }

        const i32 year{ monthNumber / 12 + 1 };
        const i32 month{ monthNumber % 12 + 1 };
        const i32 day{ glm::min( GetDay(), DaysInMonth( year, month ) ) };
        auto result{ *this };
        result.mTicks = CalendarTicks( year, month, day ) + GetTimeOfDay().count();
        return result;
    }

    auto DateTime::AddYears( i32 years ) const -> DateTime {
        if ( years < -10000 || years > 10000 ) {
            throw std::out_of_range{ "Year increment must be between -10000 and 10000." };
        }
        return AddMonths( years * 12 );
    }

    auto DateTime::Subtract( Duration value ) const -> DateTime {
        // Check before negating: Duration's minimum cannot be negated in i64.
        if ( value.count() > mTicks || value.count() < mTicks - kMaxTicks ) {
            throw std::out_of_range{ "DateTime subtraction exceeds years 1..9999." };
        }
        return AddTicks( -value.count() );
    }

    auto DateTime::Subtract( const DateTime& value ) const -> Duration {
        return Duration{ mTicks - value.mTicks };
    }

    auto DateTime::ToLocalTime() const -> DateTime {
        if ( mKind == DateTimeKind::eLocal ) {
            return *this;
        }

        const SystemTime instant{ Duration{ mTicks - kUnixEpochTicks } };
#if __cpp_lib_chrono >= 201907L
        const auto* zone{ std::chrono::current_zone() };
        const auto info{ zone->get_info( instant ) };
        const i64 localTicks{ glm::clamp( mTicks + info.offset.count() * kTicksPerSecond, i64{ 0 }, kMaxTicks ) };
        DateTime result{ localTicks, DateTimeKind::eLocal };
        const auto localInfo{ zone->get_info( std::chrono::local_time<Duration>{ Duration{ localTicks - kUnixEpochTicks } } ) };
        result.mAmbiguousDaylightTime = localInfo.result == std::chrono::local_info::ambiguous && info.offset == localInfo.first.offset;
#else
        const auto epochSeconds{ std::chrono::floor<std::chrono::seconds>( instant ).time_since_epoch().count() };
        const auto fields{ LocalFields( static_cast<std::time_t>( epochSeconds ) ) };
        DateTime result{ fields.tm_year + 1900, fields.tm_mon + 1, fields.tm_mday,
            fields.tm_hour, fields.tm_min, fields.tm_sec, DateTimeKind::eLocal };
        result.mTicks += mTicks % kTicksPerSecond;
        result.mAmbiguousDaylightTime = fields.tm_isdst > 0;
#endif
        return result;
    }

    auto DateTime::ToUniversalTime() const -> DateTime {
        if ( mKind == DateTimeKind::eUtc ) {
            return *this;
        }

        const auto offset{ LocalOffset( *this, mAmbiguousDaylightTime ) };
        return DateTime{ glm::clamp( mTicks - offset.count() * kTicksPerSecond, i64{ 0 }, kMaxTicks ), DateTimeKind::eUtc };
    }

    auto DateTime::FromSystemTime( SystemTime time ) -> DateTime {
        const i64 ticks{ time.time_since_epoch().count() };
        if ( ticks < -kUnixEpochTicks || ticks > kMaxTicks - kUnixEpochTicks ) {
            throw std::out_of_range{ "System-clock instant exceeds DateTime's range." };
        }
        return DateTime{ ticks + kUnixEpochTicks, DateTimeKind::eUtc };
    }

    auto DateTime::ToSystemTime() const -> SystemTime {
        return SystemTime{ Duration{ ToUniversalTime().mTicks - kUnixEpochTicks } };
    }

    auto DateTime::FromFileTimeUtc( i64 fileTime ) -> DateTime {
        const i64 epoch{ CalendarTicks( 1601, 1, 1 ) };
        if ( fileTime < 0 || fileTime > kMaxTicks - epoch ) {
            throw std::out_of_range{ "FILETIME exceeds DateTime's supported range." };
        }
        return DateTime{ fileTime + epoch, DateTimeKind::eUtc };
    }

    auto DateTime::FromFileTime( i64 fileTime ) -> DateTime {
        return FromFileTimeUtc( fileTime ).ToLocalTime();
    }

    auto DateTime::ToFileTimeUtc() const -> i64 {
        const i64 ticks{ mKind == DateTimeKind::eLocal ? ToUniversalTime().mTicks : mTicks };
        const i64 epoch{ CalendarTicks( 1601, 1, 1 ) };
        if ( ticks < epoch ) {
            throw std::out_of_range{ "FILETIME cannot represent dates before 1601." };
        }
        return ticks - epoch;
    }

    auto DateTime::ToFileTime() const -> i64 {
        return ToUniversalTime().ToFileTimeUtc();
    }

    auto DateTime::ToString( eastl::string_view format ) const -> eastl::string {
        return FormatDate( *this, format );
    }

    auto DateTime::Parse( eastl::string_view text ) -> DateTime {
        DateTime result{};
        if ( !TryParse( text, result ) ) {
            throw std::invalid_argument{ "Unsupported or invalid invariant DateTime text." };
        }
        return result;
    }

    auto DateTime::ParseExact( eastl::string_view text, eastl::string_view format ) -> DateTime {
        DateTime result{};
        if ( !TryParseExact( text, format, result ) ) {
            throw std::invalid_argument{ "DateTime text does not match the requested format." };
        }
        return result;
    }

    auto DateTime::TryParse( eastl::string_view text, DateTime& result ) noexcept -> bool {
        static const eastl::array<eastl::string_view, 10> formats{
            "yyyy-MM-dd'T'HH:mm:ss.FFFFFFFK", "yyyy-MM-dd'T'HH:mm:ssK", "yyyy-MM-dd'T'HH:mmK",
            "yyyy-MM-dd HH:mm:ss.FFFFFFFK", "yyyy-MM-dd HH:mm:ssK", "yyyy-MM-dd HH:mmK",
            "yyyy-MM-dd", "MM/dd/yyyy HH:mm:ss", "MM/dd/yyyy HH:mm", "MM/dd/yyyy"
        };

        // General parsing permits surrounding whitespace; exact parsing does not.
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

    auto DateTime::TryParseExact( eastl::string_view text, eastl::string_view format, DateTime& result ) noexcept -> bool {
        try {
            if ( format.empty() ) {
                throw std::invalid_argument{ "ParseExact requires a date/time format." };
            }
            result = ParseDate( text, format );
            return true;
        } catch ( const std::exception& ) {
            result = MinValue();
            return false;
        }
    }

    auto DateTime::CompareTo( const DateTime& other ) const -> i32 {
        return Compare( *this, other );
    }

    auto DateTime::Equals( const DateTime& other ) const -> bool {
        return *this == other;
    }

    auto DateTime::Compare( const DateTime& left, const DateTime& right ) -> i32 {
        return left.mTicks < right.mTicks ? -1 : left.mTicks > right.mTicks ? 1 : 0;
    }

    auto DateTime::operator==( const DateTime& other ) const -> bool {
        return mTicks == other.mTicks;
    }

    auto DateTime::operator<=>( const DateTime& other ) const -> std::strong_ordering {
        return mTicks <=> other.mTicks;
    }

    auto DateTime::operator+( Duration value ) const -> DateTime {
        return Add( value );
    }

    auto DateTime::operator-( Duration value ) const -> DateTime {
        return Subtract( value );
    }

    auto DateTime::operator-( const DateTime& other ) const -> Duration {
        return Subtract( other );
    }

}// namespace mikoto::core
