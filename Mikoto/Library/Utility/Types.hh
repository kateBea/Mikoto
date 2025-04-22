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

#include <ankerl/unordered_dense.h>

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

    using Byte_T = unsigned char;

    template<typename T>
    using Scope_T = std::unique_ptr<T>;

    template<typename T>
    using Ref_T = std::shared_ptr<T>;

    template<typename Value>
    using Registry_T = ankerl::unordered_dense::map<Size_T, Value>;

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

    /**
     * @brief Converts a pointer to a byte array.
     * @tparam T type of the object to convert.
     * @param value pointer to the object to convert.
     * @return a pointer to the byte array.
     */
    template<typename T>
    auto AsBytes( T* value ) -> Byte_T* {
        return reinterpret_cast<Byte_T*>( value );
    }


    /**
    * @brief Utility function to cast a value to a specified output type.
    *
    * This function uses `static_cast` to convert the input value to the desired output type.
    * It perfectly forwards the input to preserve value category (lvalue/rvalue).
    *
    * @tparam Output The type to cast the input value to.
    * @param value The input value to be casted. It can be an lvalue or rvalue.
    * @return The input value converted to the specified Output type.
    *
    * @note This function performs a `static_cast`. It is the caller's responsibility to ensure
    *       that the cast is valid and safe. For example, this will not perform runtime checks
    *       like `dynamic_cast`, and may result in undefined behavior if used incorrectly.
    *
    * @example
    * int x = 42;
    * float y = As<float>(x); // y = 42.0f
    *
    * std::unique_ptr<Base> base = std::make_unique<Derived>();
    * std::unique_ptr<Derived> derived = As<std::unique_ptr<Derived>>(std::move(base));
    */
    template<typename Output>
    constexpr auto Cast(auto&& value) -> Output {
        return static_cast<Output>(std::forward<decltype(value)>(value));
    }

    /**
    * @brief Reinterprets a reference or pointer as another type.
    *
    * This function wraps `reinterpret_cast` to convert the given value to the specified `Output` type.
    * It is `constexpr` and perfect-forwarding aware.
    *
    * @tparam Output The type to cast to.
    * @param value The input value to be reinterpreted. Can be lvalue or rvalue reference.
    * @return The value reinterpreted as type `Output`.
    *
    * @note It is the caller's responsibility to ensure the cast is valid and safe.
    *       Misuse can easily lead to undefined behavior.
    *
    * @example
    *     int x = 42;
    *     auto floatPtr = Reinterpret<float*>(&x);
    */
    template<typename Output>
    constexpr auto Reinterpret(auto&& value) -> Output* {
        return reinterpret_cast<Output>(std::forward<decltype(value)>(value));
    }

    /**
    * @brief Performs a `dynamic_cast` to the specified output pointer type.
    *
    * This utility function simplifies the syntax of `dynamic_cast` by inferring the input type
    * and casting it to a pointer of the specified output type. It is typically used with polymorphic
    * types where runtime type checking is necessary.
    *
    * @tparam Output The target type to cast to (must be a pointer type).
    * @param value The object to cast, usually a reference or pointer to a base class.
    * @return A pointer to the casted object if successful; nullptr otherwise.
    *
    * @note The input must be a polymorphic type (i.e., have at least one virtual function)
    *       for `dynamic_cast` to work correctly.
    *
    * @example
    * @code
    * Base* base = new Derived();
    * Derived* derived = Dynamic<Derived>(base);
    * if (derived) {
    *     derived->DoSomething();
    * }
    * @endcode
    */
    template<typename Output>
    constexpr auto Dynamic(auto&& value) -> Output* {
        return dynamic_cast<Output*>(std::forward<decltype(value)>(value));
    }

    /**
    * @brief Concept that ensures a type derives from the BaseType.
    *
    * @tparam DerivedType Type to check.
    * @tparam BaseType Type to check.
    */
    template<typename DerivedType, typename BaseType>
    concept IsDerivedFrom = std::derived_from<DerivedType, BaseType>;
}

#endif// MIKOTO_TYPES_HH
