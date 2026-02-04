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

#ifndef MIKOTO_SHADER_RENDER_PARAMS_HH
#define MIKOTO_SHADER_RENDER_PARAMS_HH

#include <string>
#include <string_view>
#include <vector>
#include <array>

#include <glm/glm.hpp>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    static constexpr UInt32 MAX_LIGHTS{ 10000 };
    static constexpr UInt32 MAX_CUBE_MAP_FACES{ 6 };
    static constexpr UInt32 MAX_RENDERABLE_ENTITIES{ 524'288 }; // 2^19

    struct ShaderMaterialParams {
        Mat4F Transform{};

        Vec4F Albedo{};
        Vec4F Factors{}; // Metallness, Roughness, AO

        Int32 AlbedoIndex{};
        Int32 NormalIndex{};
        Int32 MetallicIndex{};
        Int32 RoughnessIndex{};
        Int32 AoIndex{};
    };

    struct ShaderCameraParams {
        Mat4F View{};
        Mat4F Projection{};
        Vec4F CameraPosition{};
    };

    struct alignas(sizeof(Vec4F)) ShaderLightTypeParams {
        Vec4F Position{};
        Vec4F Direction{};

        Vec3F Diffuse{};

        float CutOff{};
        float OuterCutOff{};
        float Intensity{};
        float Radius{};

        Int32 ActiveLightType{};
    };

    enum class ShaderColorDisplayMode {
        NORMAL = 1,
        COLOR = 2,
        METAL = 3,
        AO = 4,
        ROUGH = 5,
    };

    enum class ShaderActiveLightType {
        LIGHT_TYPE_INACTIVE = -1,
        LIGHT_TYPE_POINT = 1,
        LIGHT_TYPE_SPOT = 2,
        LIGHT_TYPE_DIRECTIONAL = 3,
    };

    struct ShaderLightListParams {
        std::vector<ShaderLightTypeParams> Lights{
            MAX_LIGHTS,
            ShaderLightTypeParams()
        };

        Int32 ActiveLightsCount{};
        Int32 DisplayMode{};
    };

}

#endif //MIKOTO_SHADER_RENDER_PARAMS_HH