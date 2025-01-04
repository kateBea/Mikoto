//
// Created by kate on 10/12/23.
//

#ifndef MIKOTO_STRING_UTILS_HH
#define MIKOTO_STRING_UTILS_HH

#include <string>
#include <string_view>
#include <iterator>
#include <cctype>
#include <algorithm>

#include "fmt/format.h"
#include "fmt/color.h"
#include "fmt/core.h"
#include "fmt/ranges.h"

#include <STL/Utility/Types.hh>
#include <Common/Common.hh>

#define MKT_COLOR_PRINT_FORMATTED(COLOR, ...)                                                               \
    fmt::print(fmt::fg(COLOR), __VA_ARGS__)

#define MKT_COLOR_STYLE_PRINT_FORMATTED(COLOR, STYLE, ...)                                              \
    fmt::print(fmt::fg(COLOR) | STYLE, __VA_ARGS__)


namespace Mikoto::StringUtils {
    /**
     * @brief Returns a string which is the concatenation of the string.
     * representation of the given values. Use ConcatStr, methods with *_H are
     * designated as helpers for internal usage.
     * @see ConcatStr(...)
     * */
    template<typename T, typename... Args>
    inline static auto ConcatStr_H(const T& first, Args&&... args) -> std::string {
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

    /**
     * Utility function to make panel names for ImGui windows.
     * @param panelIcon Panel's icon value.
     * @param panelName Name of the panel.
     * @returns The panel's name including the icon.
     * */
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
        auto rightCharIt{ str.rbegin() };

        while (leftCharIt != str.end()) {
            if (std::isspace(*leftCharIt)) {
                leftCharIt = std::next(leftCharIt);
            }
            else {
                break;
            }
        }

        while (rightCharIt != str.rend()) {
            if (std::isspace(*rightCharIt)) {
                rightCharIt = std::next(rightCharIt);
            }
            else {
                break;
            }
        }

        auto right{  rightCharIt.base() };

        return std::string{ str.substr(leftCharIt - str.begin(), right - leftCharIt) };
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


    /**
     *
     * */
    template<typename... Args>
    inline auto ConcatenatePath(const Path_T& first, Args&&... routes) -> Path_T {
        Path_T result{};
        result = result / first;

        std::filesystem::path expansion{};
        if constexpr (sizeof...(routes))
            expansion = std::move(ConcatenatePath(routes...));

        result = result / expansion;

        return result;
    }

    /**
     * @brief Make a a path to a char string. Transforms wide char strings to byte
     * char strings. On Windows std::filesystem::string returns a string of wide
     * char types (wchar_t), whereas on linux it returns a string of char.
     * @param path Path to the file
     * @returns String of byte sized .characters.
     * */
    inline auto GetByteChar(const Path_T &path) -> std::string {
        std::string fileDir(4096, '\0');
#if defined(_WIN32) || defined(_WIN64)
        wcstombs_s(nullptr, fileDir.data(), fileDir.size(), path.c_str(), 4096);
#else
        std::copy(path.native().begin(), path.native().end(), fileDir.begin());
#endif
        return fileDir;
    }
}

#endif // MIKOTO_STRING_UTILS_HH
