//
// Created by kate on 10/12/23.
//

#ifndef MIKOTO_STRING_UTILS_HH
#define MIKOTO_STRING_UTILS_HH

#include <string>
#include <string_view>

#include <fmt/format.h>

#include <Utility/Types.hh>
#include <Utility/Common.hh>

namespace Mikoto::StringUtils {
    /**
     * Returns a string which is the concatenation of the string
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
     * Returns a string which is the concatenation of the string
     * representation of the given values.
     * @param args list of values
     * @tparam Args types of the given values
     * */
    template<typename... Args>
    inline auto ConcatStr(Args&&... args) -> decltype(auto) {
        return ConcatStr_H(std::forward<Args>(args)...);
    }

    MKT_NODISCARD inline auto MakePanelName(std::string_view panelIcon, std::string_view panelName) -> std::string {
        return fmt::format("{} {}", panelIcon, panelName);
    }

    MKT_NODISCARD inline auto ToCString(char8_t* str) -> const char* {
        return reinterpret_cast<const char*>(str);
    }
}

#endif // MIKOTO_STRING_UTILS_HH
