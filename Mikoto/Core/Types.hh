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

#ifndef MIKOTO_TYPES_HH
#define MIKOTO_TYPES_HH

#include <cstdint>
#include <cstddef>

#include <EASTL/utility.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <ankerl/unordered_dense.h>

namespace ankerl::unordered_dense {
    template <typename A, typename B>
    struct hash<eastl::pair<A, B>> : tuple_hash_helper<A, B> {
        using is_avalanching = void;

        auto operator()(eastl::pair<A, B> const& t) const noexcept -> std::uint64_t {
            return tuple_hash_helper<A, B>::calc_hash(t, eastl::index_sequence_for<A, B>{});
        }
    };
}

namespace mikoto::core {

    using float2 = glm::vec2;
    using float3 = glm::vec3;
    using float4 = glm::vec4;

    using float4x4 = glm::mat4;
    using float3x3 = glm::mat3;

    using uvec2 = glm::uvec2;
    using uvec3 = glm::uvec3;
    using uvec4 = glm::uvec4;

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

    using size_t = std::size_t;
    using byte_t = unsigned char;

    using c_str = const char *;
}

#endif // MIKOTO_TYPES_HH
