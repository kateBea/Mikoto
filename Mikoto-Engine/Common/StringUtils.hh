//
// Created by kate on 10/12/23.
//

#ifndef MIKOTO_STRING_UTILS_HH
#define MIKOTO_STRING_UTILS_HH

#include <string>
#include <string_view>
#include <iterator>
#include <cctype>

#include "fmt/format.h"

#include "Common.hh"
#include "Types.hh"

namespace Mikoto::StringUtils {
    /**
     * @brief Returns a string which is the concatenation of the string.
     * representation of the given values.
     * @see ConcatStr(...)
     * */
    template<typename T, typename... Args>
    inline auto ConcatStr_H(const T& first, Args&&... args) -> std::string {
        std::string result{};
        result.append(fmt::to_string(first));

        std::string expansion{};
        if constexpr (sizeof...(args))
            expansion = std::move(ConcatStr_H(args...));

        result.append(expansion);

        return result;
    }

    /**
     * @brief Returns a string which is the concatenation of the string
     * representation of the given values.
     * @param args list of values.
     * @tparam Args types of the given values.
     * */
    template<typename... Args>
    inline auto ConcatStr(Args&&... args) -> decltype(auto) {
        return ConcatStr_H(std::forward<Args>(args)...);
    }

    MKT_NODISCARD inline auto MakePanelName(std::string_view panelIcon, std::string_view panelName) -> std::string {
        return fmt::format("{} {}", panelIcon, panelName);
    }

    /**
     * @brief Remove any leading white spaces from str, from both ends.
     * @param str string to be trimmed.
     * @returns copy of str without leading whitespaces from both ends.
     * */
    MKT_NODISCARD inline auto Trim(std::string_view str) -> std::string {
        auto leftCharIt{ str.begin() };
        auto rightCharIt{ std::prev(str.end()) };

        while (leftCharIt != str.end() && std::isspace(*leftCharIt)) {
            leftCharIt = std::next(leftCharIt);
        }

        while (rightCharIt != std::prev(str.begin()) && std::isspace(*rightCharIt)) {
            rightCharIt = std::prev(rightCharIt);
        }

        return std::string{ str.substr(leftCharIt - str.begin(), rightCharIt - str.begin() + 1) };
    }

    /**
     * @brief Returns true if two character sequences are equal.
     * @param str1 Null terminated string to compare.
     * @param str2 Null terminated string to compare.
     * @returns True if both string are the same, false otherwise.
     * */
    MKT_NODISCARD inline constexpr auto Equal(std::string_view str1, std::string_view str2) -> bool {
        return str1.compare(str2) == 0;
    }

    template<typename CharType>
    inline constexpr auto ReplaceWith(std::string& str, CharType oldVal, CharType newVal) -> void {
        std::replace(str.begin(), str.end(), oldVal, newVal);
    }
}

#endif // MIKOTO_STRING_UTILS_HH
