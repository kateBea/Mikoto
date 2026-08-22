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

#ifndef MIKOTO_CORE_HH
#define MIKOTO_CORE_HH

#include <cassert>
#include <type_traits>

#include <cpptrace/cpptrace.hpp>

// =======================
//  Utility Macros
// =======================
#define MKT_NODISCARD   [[nodiscard]]
#define MKT_UNUSED_FUNC [[maybe_unused]]
#define MKT_UNUSED_VAR  [[maybe_unused]]

#define MIKOTO_STRINGIFY_IMPL(x) #x
#define MIKOTO_STRINGIFY(x) MIKOTO_STRINGIFY_IMPL(x)

#define MIKOTO_CONCAT_IMPL(a, b) a##b
#define MIKOTO_CONCAT(a, b) MIKOTO_CONCAT_IMPL(a, b)

#define BIT_SET(N) (1ULL << N)
#define MKT_STRINGIFY(x) #x

#define DISABLE_COPY_AND_MOVE_FOR( CLASS_NAME )   \
    CLASS_NAME( const CLASS_NAME& ) = delete;     \
    auto operator=( const CLASS_NAME& ) = delete; \
    CLASS_NAME( CLASS_NAME&& ) = delete;          \
    auto operator=( CLASS_NAME&& ) = delete

#define DISABLE_COPY_FOR( CLASS_NAME )        \
    CLASS_NAME( const CLASS_NAME& ) = delete; \
    auto operator=( const CLASS_NAME& ) = delete

namespace mikoto::core {
    template<typename TO_T, typename FROM_T>
    constexpr auto rc_cast( FROM_T const* ptr ) {
        return reinterpret_cast<TO_T>( const_cast<FROM_T*>( ptr ) );
    }

    template<typename TO_T, typename FROM_T>
    constexpr auto r_cast( FROM_T* ptr ) {
        return reinterpret_cast<TO_T>( ptr );
    }

    template<typename TO_T, typename FROM_T>
    constexpr auto as( FROM_T const* ptr ) {
        return static_cast<TO_T>( const_cast<FROM_T*>( ptr ) );
    }

    template<typename TO_T, typename FROM_T>
    constexpr auto as( FROM_T* ptr ) {
        return static_cast<TO_T>( ptr );
    }

    template<typename TO_T, typename FROM_T>
    constexpr auto as( const FROM_T& data ) {
        return static_cast<TO_T>( data );
    }

    template<typename TO_T, typename FROM_T>
    constexpr auto dc_cast( FROM_T const* ptr ) {
        return dynamic_cast<TO_T>( const_cast<FROM_T*>( ptr ) );
    }

    template<typename TO_T, typename FROM_T>
    constexpr auto d_cast( FROM_T* ptr ) {
        return dynamic_cast<TO_T>( ptr );
    }

    template<typename T>
    MKT_NODISCARD auto align(T size, T alignment) -> T {
        return (size + alignment - 1) & ~(alignment - 1);
    }

    // For usage in the backends to cast
    // interfaces to their corresponding implementations
    template<typename T, typename U>
    auto checked_cast( U* u ) -> T {
        static_assert( std::is_pointer_v<T>, "T must be a pointer type" );

        static_assert( std::is_polymorphic_v<U>, "U must be polymorphic" );
        static_assert( !std::is_same_v<T, U*>, "Redundant checked_cast" );

#if !defined( NDEBUG )
        if ( !u ) {
            return nullptr;
        }

        T t{ dynamic_cast<T>( u ) };

        if (!t) {
            cpptrace::generate_trace().print();
            assert( false && "Invalid type cast" );
        }

        return t;
#else
        return static_cast<T>( u );
#endif
    }
}// namespace mikoto::core

#endif // MIKOTO_CORE_HH