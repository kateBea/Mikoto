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

#include <iomanip>
#include <iostream>

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Math/Math.hh>

namespace mikoto::math {

    using namespace mikoto::core;

    // static auto DumpMat4( const float4x4& m, size_type index ) -> void {
    //     std::cout << "index: " << index << "\n";
    //     std::cout << "=========================\n";
    //
    //     for ( i32 row{ 0 }; row < 4; ++row ) {
    //         for ( i32 col{ 0 }; col < 4; ++col ) {
    //             std::cout << std::setw( 10 )
    //                       << std::setprecision( 5 )
    //                       << std::fixed
    //                       << m[col][row] << " ";
    //         }
    //         std::cout << "\n";
    //     }
    //
    //     // endl to flush
    //     std::cout << "=========================" << std::endl;
    // }
    //
    // static auto DumpMat4Beautify( const float4x4& m, size_type index ) -> void {
    //     std::string out{};
    //
    //     out += ::Format( "index: {}\n", index );
    //     out += "=========================\n";
    //
    //     for ( Int32 row{ 0 }; row < 4; ++row ) {
    //         out += StringUtil::Format(
    //                 "{:>10.5f} {:>10.5f} {:>10.5f} {:>10.5f}\n",
    //                 m[0][row],
    //                 m[1][row],
    //                 m[2][row],
    //                 m[3][row] );
    //     }
    //
    //     out += "=========================\n";
    //
    //     MKT_COLOR_PRINT_FORMATTED_FLUSH(
    //             MKT_FMT_COLOR_BLUE_VIOLET,
    //             "{}",
    //             out );
    // }

    auto Floor( const double value ) -> double {
        return glm::floor( value );
    }

    auto Log2( const double value ) -> double {
        return std::log2( value );
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
        return a + f * (b - a);
    }

    auto Recompose( float4x4& transform, const float3& translation, const float3& rotation, const float3& scale ) -> void {
        float matrixTranslation[3]{}, matrixRotation[3]{}, matrixScale[3]{};

        matrixTranslation[0] = translation.x;
        matrixTranslation[1] = translation.y;
        matrixTranslation[2] = translation.z;

        matrixRotation[0] = ToDegrees( rotation.x );
        matrixRotation[1] = ToDegrees( rotation.y );
        matrixRotation[2] = ToDegrees( rotation.z );

        matrixScale[0] = scale.x;
        matrixScale[1] = scale.y;
        matrixScale[2] = scale.z;

        ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, glm::value_ptr( transform ));
    }

    auto Decompose( const float4x4 &transform, float3 &translation, float3 &rotation, float3 &scale ) -> void {
        float matrixTranslation[3]{}, matrixRotation[3]{}, matrixScale[3]{};
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr( transform ), matrixTranslation, matrixRotation, matrixScale);

        translation.x = matrixTranslation[0];
        translation.y = matrixTranslation[1];
        translation.z = matrixTranslation[2];

        rotation.x = matrixRotation[0];
        rotation.y = matrixRotation[1];
        rotation.z = matrixRotation[2];

        scale.x = matrixScale[0];
        scale.y = matrixScale[1];
        scale.z = matrixScale[2];
    }

    auto NextPowerOf2( core::u32 value ) -> u32 {
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

    auto DumpMat4FList( const std::vector<glm::mat4>& m ) -> void {
        // for ( Size i{}; i < m.size(); ++i ) {
        //     DumpMat4( m[i], i );
        // }
    }

    auto DumpMat4FListBeautify( const std::vector<glm::mat4>& m ) -> void {
        // for ( Size i{}; i < m.size(); ++i ) {
        //     DumpMat4Beautify( m[i], i );
        // }
    }
}