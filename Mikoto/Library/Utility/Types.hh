/**
 * Types.hh
 * Created by kate on 8/5/2023.
 * */

#ifndef MIKOTO_TYPES_HH
#define MIKOTO_TYPES_HH

// C++ Standard Library
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_map>

namespace Mikoto {
    using Path_T = std::filesystem::path;

    using Int8_T = std::int8_t;
    using Int16_T = std::int16_t;
    using Int32_T = std::int32_t;
    using Int64_T = std::int64_t;

    using UInt8_T = std::uint8_t;
    using UInt16_T = std::uint16_t;
    using UInt32_T = std::uint32_t;
    using UInt64_T = std::uint64_t;

    using UShort_T = unsigned short;
    using UChar_T = unsigned char;
    using ULong_T = unsigned short;
    using ULongLong_T = unsigned long long;

    using Short_T = unsigned short;
    using Long_T = unsigned long;
    using LongLong_T = long long;

    using Size_T = std::size_t;

    using CStr_T = const char*;

    template<typename T>
    using Scope_T = std::unique_ptr<T>;

    template<typename T>
    using Ref_T = std::shared_ptr<T>;

    template<typename Value>
    using Registry_T = std::unordered_map<Size_T, Value>;

    /**
     * @brief Creates a unique pointer to the given type.
     * @tparam T type of the object to create a unique pointer to.
     * @tparam Args types of the arguments to pass to the constructor of the object.
     * @param args arguments to pass to the constructor of the object.
     * @return a unique pointer to the object.
     * */
    template<typename T, typename... Args>
    constexpr auto CreateScope( Args &&...args ) -> Scope_T<T> {
        return std::make_unique<T>( std::forward<Args>( args )... );
    }

    /**
     * @brief Creates a shared pointer to the given type.
     * @tparam T type of the object to create a shared pointer to.
     * @tparam Args types of the arguments to pass to the constructor of the object.
     * @param args arguments to pass to the constructor of the object.
     * @return a shared pointer to the object.
     * */
    template<typename T, typename... Args>
    constexpr auto CreateRef( Args &&...args ) -> Ref_T<T> {
        return std::make_shared<T>( std::forward<Args>( args )... );
    }
}

#endif// MIKOTO_TYPES_HH
