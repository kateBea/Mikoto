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
#include <utility>

#include <glm/glm.hpp>

namespace Mikoto {
    using Vec4F = glm::vec4;
    using Vec3F = glm::vec3;
    using Vec2F = glm::vec2;
    using Mat4F = glm::mat4;
    using UVec2 = glm::uvec2;

    using Path = std::filesystem::path;

    using Int8 = std::int8_t;
    using Int16 = std::int16_t;
    using Int32 = std::int32_t;
    using Int64 = std::int64_t;

    using UInt8 = std::uint8_t;
    using UInt16 = std::uint16_t;
    using UInt32 = std::uint32_t;
    using UInt64 = std::uint64_t;

    using UShort = unsigned short;
    using UChar = unsigned char;
    using ULong = unsigned short;
    using ULongLong = unsigned long long;

    using Short = unsigned short;
    using Long = unsigned long;
    using LongLong = long long;

    using Size = std::size_t;

    using CStr = const char *;

    using Byte = unsigned char;

    template<typename T>
    using Unique = std::unique_ptr<T>;

    template<typename T>
    using Shared = std::shared_ptr<T>;


    /**
     * @brief Creates a unique pointer to the given type.
     * @tparam T type of the object to create a unique pointer to.
     * @tparam Args types of the arguments to pass to the constructor of the object.
     * @param args arguments to pass to the constructor of the object.
     * @return a unique pointer to the object.
     * */
    template<typename T, typename... Args>
    constexpr auto CreateScope( Args &&...args ) -> Unique<T> {
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
    constexpr auto CreateRef( Args &&...args ) -> Shared<T> {
        return std::make_shared<T>( std::forward<Args>( args )... );
    }

    /**
     * @brief Converts a pointer to a byte array.
     * @tparam T type of the object to convert.
     * @param value pointer to the object to convert.
     * @return a pointer to the byte array.
     */
    auto AsBytes( auto &&value ) -> Byte * {
        return reinterpret_cast<Byte *>( value );
    }

    /**
    * @brief Concept that ensures a type derives from the BaseType.
    *
    * @tparam DerivedType Type to check.
    * @tparam BaseType Type to check.
    */
    template<typename DerivedType, typename BaseType>
    concept IsDerivedFrom = std::derived_from<DerivedType, BaseType>;

    template<typename From, typename To>
    concept DynamicallyCastable =
            std::is_pointer_v<From> &&
            std::is_pointer_v<To> &&
            std::is_polymorphic_v<std::remove_pointer_t<From>> &&
            requires( From from ) {
                { dynamic_cast<To>( from ) } -> std::same_as<To>;
            };

    // Bidirectional
    template<typename From, typename To>
    concept RelatedDynamicallyCastable =
            DynamicallyCastable<From, To> &&
            ( std::is_base_of_v<std::remove_pointer_t<From>, std::remove_pointer_t<To>> ||
              std::is_base_of_v<std::remove_pointer_t<To>, std::remove_pointer_t<From>> );

    /**
     * @class PathBuilder
     * A utility class to build and finalize paths in a controlled manner.
     * The class provides a fluent API for appending parts to a path, ensuring that
     * once the path is "built" using the `Build` method, no further modifications are allowed.
     * */
    class PathBuilder final {
    public:
        /**
         * @brief Appends a new part to the path if the path has not been built yet.
         * @tparam StringLikeT A type convertible to std::string_view (e.g., std::string, const char*).
         * @param path The part of the path to append.
         * @return A reference to the current PathBuilder instance to allow chaining.
         * */
        template<std::convertible_to<std::string_view> StringLikeT>
        auto WithPath( StringLikeT&& path ) -> PathBuilder& {
            if ( !build ) {
                m_Path.append( path );
            }

            return *this;
        }

        /**
         * @brief Finalizes the path and prevents further modifications.
         * @return The finalized path as a Path_T object.
         * */
        auto Build() -> Path {
            build = true;
            return m_Path;
        }

    private:
        bool build{};
        Path m_Path{};
    };
}// namespace Mikoto

#endif// MIKOTO_TYPES_HH
