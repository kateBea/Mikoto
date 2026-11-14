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

#ifndef MIKOTO_CORE_TYPES_HH
#define MIKOTO_CORE_TYPES_HH

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <Core/Profiler.hh>

// TODO: for this I need to make a workaround for mixing with string views
// Motivation: This right now does not work, I need to convert the view to a string first
// auto do_something(eastl::string_view view) -> void {
//     ankerl::unordered_dense::map<eastl::string, u64> map{};
//     map[view] = 77;
// }

namespace ankerl::unordered_dense {

    template <>
    struct hash<eastl::string> {
        using is_avalanching = void;

        /**
         * @brief Hashes an EASTL string's exact byte range.
         * @param str Source character data.
         * @returns 64-bit hash compatible with std::string_view hashing.
         */
        auto operator()(eastl::string const& str) const noexcept -> std::uint64_t {
            return hash<std::string_view>{}( std::string_view{ str.data(), str.size() } );
        }
    };

    template <>
    struct hash<eastl::string_view> {
        using is_avalanching = void;

        /**
         * @brief Hashes an EASTL string's exact byte range.
         * @param sv Borrowed character range.
         * @returns 64-bit hash compatible with std::string_view hashing.
         */
        auto operator()(eastl::string_view const& sv) const noexcept -> std::uint64_t {
            return hash<std::string_view>{}( std::string_view{ sv.data(), sv.size() } );
        }
    };

}// namespace ankerl::unordered_dense

template<typename A, typename B>
struct ankerl::unordered_dense::hash<eastl::pair<A, B>> : tuple_hash_helper<A, B> {
    using is_avalanching = void;

    auto operator()( eastl::pair<A, B> const &t ) const noexcept -> std::uint64_t {
        return tuple_hash_helper<A, B>::calc_hash( t, eastl::index_sequence_for<A, B>{} );
    }
};

namespace mikoto::core {

    using float2 = glm::vec2;
    using float3 = glm::vec3;
    using float4 = glm::vec4;

    using float4x4 = glm::mat4;
    using float3x3 = glm::mat3;

    using uint2 = glm::uvec2;
    using uint3 = glm::uvec3;
    using uint4 = glm::uvec4;

    using quat = glm::quat;

    using f32 = float;
    using f64 = double;

    using i8 = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using ushort = unsigned short;
    using uchar = unsigned char;
    using ulong = unsigned short;
    using ull = unsigned long long;

    using ishort = short;
    using ilong = long;
    using ill = long long;

    using usize = std::size_t;
    using ubyte = unsigned char;
    using unicode = char32_t;
    using cstr = const char *;

    using ptr = std::uintptr_t;

    using c_str = const char *;
}

#endif// MIKOTO_CORE_TYPES_HH
