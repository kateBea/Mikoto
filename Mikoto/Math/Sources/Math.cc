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

#include <limits>
#include <iomanip>
#include <iostream>

#include <EASTL/utility.h>
#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Math/Math.hh>

namespace mikoto::math {

    using namespace mikoto::core;

    static auto DumpMat4( const core::float4x4& m, core::usize index ) -> void {
        std::cout << "index: " << index << "\n";
        std::cout << "=========================\n";

        for ( core::i32 row{ 0 }; row < 4; ++row ) {
            for ( core::i32 col{ 0 }; col < 4; ++col ) {
                std::cout << std::setw( 10 )
                          << std::setprecision( 5 )
                          << std::fixed
                          << m[col][row] << " ";
            }

            std::cout << "\n";
        }

        std::cout << "=========================" << std::endl;
    }

    static auto DumpMat4Beautify( const core::float4x4& m, core::usize index ) -> void {
        eastl::string out{};

        out += string::Format( "index: {}\n", index );
        out += "=========================\n";

        for ( core::i32 row{ 0 }; row < 4; ++row ) {
            out += string::Format(
                "{:>10.5f} {:>10.5f} {:>10.5f} {:>10.5f}\n",
                m[0][row],
                m[1][row],
                m[2][row],
                m[3][row] );
        }

        out += "=========================\n";

        MKT_COLOR_PRINT_FORMATTED_FLUSH(
            MKT_FMT_COLOR_BLUE_VIOLET,
            "{}",
            out );
    }

    auto Floor( const double value ) -> double {
        return glm::floor( value );
    }

    auto Log2( const double value ) -> double {
        return glm::log2( value );
    }

    auto ToRadians( double value ) -> double {
        return glm::radians( value );
    }

    auto ToDegrees( double value ) -> double {
        return glm::degrees( value );
    }

    auto Abs( double value ) -> double {
        return glm::abs( value );
    }

    auto Lerp( float a, float b, float f ) -> double {
        return a + f * ( b - a );
    }

    auto NextPowerOf2( core::u32 value ) -> core::u32 {
        // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
        // Compute the next highest power of 2 for a 32-bit integer.
        value--;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value++;

        return value;
    }

    auto DumpMat4FList( eastl::span<const core::float4x4> matrices ) -> void {
        for ( core::usize i{}; i < matrices.size(); ++i ) {
            DumpMat4( matrices[i], i );
        }
    }

    auto DumpMat4FListBeautify( eastl::span<const core::float4x4> matrices ) -> void {
        for ( core::usize i{}; i < matrices.size(); ++i ) {
            DumpMat4Beautify( matrices[i], i );
        }
    }

    auto MakePos( f32 x, f32 y, f32 z ) -> float4 {
        return { x, y, z, 1.0f };
    }

    auto MakeDir( f32 x, f32 y, f32 z ) -> float4 {
        return { x, y, z, .0f };
    }

    auto Round( f64 value, const usize decimalsCount ) -> f64 {
        const double factor{ glm::pow( 10, decimalsCount ) };
        return glm::round( value * factor ) / factor;
    }

    auto SmoothDamp( float current, float target, float& velocity, float smoothTime, float dt ) -> float {
        MKT_BEGIN_PROFILER();

        if ( dt <= 0.0f ) {
            return current;
        }

        if ( smoothTime <= 0.0f || dt >= smoothTime * 100.0f ) {
            velocity = 0.0f;
            return target;
        }

        const double omega{ 2.0 / glm::max( smoothTime, 0.0001f ) };
        const double change{ double( current ) - target };
        const double temporary = ( velocity + omega * change ) * dt;
        const double decay{ glm::exp( -omega * dt ) };
        const float result{ float( target + ( change + temporary ) * decay ) };

        velocity = float( ( velocity - omega * temporary ) * decay );

        // A target reversal can leave velocity pointing past the new target.
        if ( ( target - current ) * ( result - target ) > 0.0f ) {
            velocity = 0.0f;
            return target;
        }

        return result;
    }

    auto SmoothDamp( const core::float3& current, const core::float3& target, core::float3& velocity, float smoothTime, float dt ) -> core::float3 {
        MKT_BEGIN_PROFILER();
        core::float3 result{};

        for ( int i = 0; i < 3; ++i ) {
            result[i] = SmoothDamp( current[i], target[i], velocity[i], smoothTime, dt );
        }

        return result;
    }

    auto SmoothDampAngle( float current, float target, float& velocity, float smoothTime, float dt ) -> float {
        MKT_BEGIN_PROFILER();
        constexpr float tau{ 6.2831853071795864769f };
        return SmoothDamp( current, current + std::remainder( target - current, tau ), velocity, smoothTime, dt );
    }

    auto MakeGizmoRotationFrame( const core::float4x4& centeredWorld ) -> core::float4x4 {
        core::float3 position{}, rotation{}, scale{};
        auto frame{ math::DecomposeTransform( centeredWorld, position, rotation, scale )
                        ? math::EntityRotationMatrix( rotation )
                        : core::float4x4{ 1.0f } };
        frame[3] = centeredWorld[3];
        return frame;
    }

    auto MakeGizmoTransform( const core::float4x4& world, const core::float3& localCenter ) -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        return glm::translate( world, localCenter );
    }

    auto GizmoToLocalTransform( const core::float4x4& gizmo, const core::float3& localCenter, const core::float4x4& parentWorld, core::float4x4& local ) -> bool {
        MKT_BEGIN_PROFILER();
        const core::f32 determinant{ glm::determinant( parentWorld ) };

        if ( determinant == 0.0f ) {
            return false;
        }

        auto result{ glm::inverse( parentWorld ) * glm::translate( gizmo, -localCenter ) };
        // These are affine transforms; inversion can round the homogeneous 1.
        result[0][3] = result[1][3] = result[2][3] = 0.0f;
        result[3][3] = 1.0f;
        local = result;
        return true;
    }

    auto EntityRotationMatrix( const core::float3& degrees ) -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        const auto radians = glm::radians( degrees );
        return glm::eulerAngleYXZ( radians.y, radians.x, radians.z );
    }

    auto EntityRotationDegrees( const core::float4x4& rotation ) -> core::float3 {
        MKT_BEGIN_PROFILER();
        core::float3 radians{};
        glm::extractEulerAngleYXZ( rotation, radians.y, radians.x, radians.z );
        return glm::degrees( radians );
    }

    auto RecomputeTransform( const core::float3& position, const core::float3& size, const core::float3& angles ) -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        // Compute scale matrix
        const core::float4x4 scale{ glm::scale( constants::kIdentityMat, size ) };

        // Compute rotation matrix
        const core::float4x4 rotation{ EntityRotationMatrix( angles ) };

        return glm::translate( constants::kIdentityMat, position ) * rotation * scale;
    }

    auto RecomputeTransform( const core::float3& position, const core::float3& scale, const core::float3& angles, const core::float3& pivot ) -> glm::mat4 {
        MKT_BEGIN_PROFILER();
        const core::float4x4 T{ glm::translate( core::float4x4( 1.0f ), position ) };
        const core::float4x4 Tp{ glm::translate( core::float4x4( 1.0f ), pivot ) };
        const core::float4x4 Tn{ glm::translate( core::float4x4( 1.0f ), -pivot ) };

        const core::float4x4 R{ EntityRotationMatrix( angles ) };

        const core::float4x4 S{ glm::scale( glm::mat4( 1.0f ), scale ) };

        return T * Tp * R * S * Tn;
    }

    auto Recompose( core::float4x4& transform, const core::float3& translation, const core::float3& rotation, const core::float3& scale ) -> void {
        MKT_BEGIN_PROFILER();
        transform = RecomputeTransform( translation, scale, rotation );
    }

    auto DecomposeTransform( const core::float4x4& transform, core::float3& translation, core::float3& rotation, core::float3& scale ) -> bool {
        MKT_BEGIN_PROFILER();
        core::float3 position{}, signedScale{}, skew{};
        core::float4 perspective{};
        core::quat orientation{};

        if ( !glm::decompose( transform, signedScale, orientation, position, skew, perspective ) ) {
            return false;
        }

        const auto angles{ EntityRotationDegrees( glm::mat4_cast( glm::normalize( orientation ) ) ) };
        translation = position;
        rotation = angles;
        scale = signedScale;
        return true;
    }

    auto Decompose( const core::float4x4& transform, core::float3& translation, core::float3& rotation, core::float3& scale ) -> void {
        MKT_BEGIN_PROFILER();
        // Both directions use degrees in YXZ order. glm::decompose also retains
        // reflection signs and separates shear before extracting the rotation.
        if ( !DecomposeTransform( transform, translation, rotation, scale ) ) {
            // Zero scale has no unique rotation; keep the last valid angles.
            translation = core::float3{ transform[3] };
            scale = { glm::length( core::float3{ transform[0] } ), glm::length( core::float3{ transform[1] } ), glm::length( core::float3{ transform[2] } ) };
        }
    }

    namespace random {

        auto GetSeed() -> std::random_device& {
            MKT_BEGIN_PROFILER();
            thread_local std::random_device seed{};
            return seed;
        }

        auto GetRandomUInt64( std::random_device& seed ) -> core::u64 {
            MKT_BEGIN_PROFILER();
            thread_local std::mt19937_64 mt{ seed() };

            return std::uniform_int_distribution<core::u64>{
                ( std::numeric_limits<core::u64>::min )(),
                ( std::numeric_limits<core::u64>::max )(),
            }( mt );
        }

        auto GetRandomUInt32( std::random_device& seed ) -> core::u32 {
            MKT_BEGIN_PROFILER();
            thread_local std::mt19937 mt{ seed() };

            return std::uniform_int_distribution<core::u32>{
                ( std::numeric_limits<core::u32>::min )(),
                ( std::numeric_limits<core::u32>::max )(),
            }( mt );
        }

        auto GetRandomInt( core::i32 lowerBound, core::i32 upperbound, std::random_device& seed ) -> core::i32 {
            MKT_BEGIN_PROFILER();

            if ( lowerBound > upperbound ) {
                MKT_THROW_RUNTIME_ERROR( fmt::format( "Invalid range for random integer generation. Lower bound is {}, upperbound is {}", lowerBound, upperbound ) );
            }

            thread_local std::mt19937 mt{ seed() };
            return std::uniform_int_distribution{ lowerBound, upperbound }( mt );
        }

        auto GetRandomReal( double lowerBound, double upperbound, std::random_device& seed ) -> double {
            MKT_BEGIN_PROFILER();

            if ( lowerBound > upperbound ) {
                MKT_THROW_RUNTIME_ERROR( fmt::format( "Invalid range for random integer generation. Lower bound is {}, upperbound is {}", lowerBound, upperbound ) );
            }

            thread_local std::mt19937 mt{ seed() };
            return std::uniform_real_distribution{ lowerBound, upperbound }( mt );
        }

        auto GetRandomIntNumberList( core::usize length, core::i32 lowerBound, core::i32 upperbound, std::random_device& seed ) -> std::vector<core::i32> {
            MKT_BEGIN_PROFILER();
            std::vector<core::i32> result{};

            core::usize count{};
            result.reserve( length );

            for ( ; count < length; ++count ) {
                result.emplace_back( GetRandomInt( lowerBound, upperbound, seed ) );
            }

            return result;
        }

        auto GetRandomRealNumberList( core::usize length, double lowerBound, double upperbound, std::random_device& seed ) -> std::vector<double> {
            MKT_BEGIN_PROFILER();
            std::vector<double> result{};

            core::usize count{};
            result.reserve( length );

            for ( ; count < length; ++count ) {
                result.emplace_back( GetRandomReal( lowerBound, upperbound, seed ) );
            }

            return result;
        }

    }// namespace random

}// namespace mikoto::math
