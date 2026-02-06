//    Copyright 2025 ケイト
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

#include <cmath>

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Math/Math.hh>

namespace Mikoto {

    auto Math::Floor( const double value ) -> double {
        return glm::floor( value );
    }

    auto Math::Log2( const double value ) -> double {
        return std::log2( value );
    }

    auto Math::ToRadians( double value ) -> double {
        return glm::radians( value );
    }

    auto Math::ToDegrees( double value ) -> double {
        return glm::degrees( value );
    }

    auto Math::Lerp( float a, float b, float f ) -> double {
        return a + f * (b - a);
    }

    auto Math::Decompose( const Mat4F &transform, Vec3F &translation, Vec3F &rotation, Vec3F &scale ) -> void {
        float matrixTranslation[3]{}, matrixRotation[3]{}, matrixScale[3]{};
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr( transform ), matrixTranslation, matrixRotation, matrixScale);

        translation.x = matrixTranslation[0];
        translation.y = matrixTranslation[1];
        translation.z = matrixTranslation[2];

        rotation.x = ToRadians( matrixRotation[0] );
        rotation.y = ToRadians( matrixRotation[1] );
        rotation.z = ToRadians( matrixRotation[2] );

        scale.x = matrixScale[0];
        scale.y = matrixScale[1];
        scale.z = matrixScale[2];
    }
}