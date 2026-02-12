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

#define MKT_SHADER_TRUE 1
#define MKT_SHADER_FALSE 0

namespace Mikoto {

    static constexpr Int32 INVALID_TEXTURE_INDEX{ -1 };
    static constexpr UInt32 MAX_LIGHTS{ 10000 };
    static constexpr UInt32 MAX_CUBE_MAP_FACES{ 6 };
    static constexpr UInt32 MAX_RENDERABLE_ENTITIES{ 524'288 }; // 2^19
    static constexpr UInt32 MAX_NUM_JOINTS{ 128 }; // 2^19

    // Deprecated
    struct ShaderMaterialParams {
        Mat4F Transform{};

        Vec4F Albedo{};
        Vec4F Factors{}; // Metallness, Roughness, AO

        Vec3F EmissiveFactors{ 1.0f, 1.0f, 1.0f };
        float EmissiveIntensity{ 1.0 }; // 1.0 default

        float Alpha{ 1.0 };

        Int32 AlbedoIndex{INVALID_TEXTURE_INDEX };
        Int32 NormalIndex{ INVALID_TEXTURE_INDEX };
        Int32 MetallicIndex{ INVALID_TEXTURE_INDEX };
        Int32 RoughnessIndex{ INVALID_TEXTURE_INDEX };
        Int32 AoIndex{ INVALID_TEXTURE_INDEX };
        Int32 EmissiveIndex{ INVALID_TEXTURE_INDEX };
    };

    struct MeshParameters {
        Mat4F Transform{};
        Mat4F JointMatrix[MAX_NUM_JOINTS]{};

        UInt32 JointCount{};
    };

    struct MaterialParameters {
        // --- Base Color ---
        Vec4F BaseColorFactor{ 1, 1, 1, 1 };
        Int32 AlbedoIndex{ INVALID_TEXTURE_INDEX };
        Int32 BaseColorTexCoord{ 0 };

        // --- Metallic-Roughness ---
        float MetallicFactor{ 1.0f };
        float RoughnessFactor{ 1.0f };
        Int32 MetallicRoughnessIndex{ INVALID_TEXTURE_INDEX };
        Int32 MetallicRoughnessTexCoord{ 0 };

        // --- Normal Map ---
        Int32 NormalIndex{ INVALID_TEXTURE_INDEX };
        float NormalScale{ 1.0f };
        Int32 NormalTexCoord{ 0 };

        // --- Occlusion ---
        Int32 OcclusionIndex{ INVALID_TEXTURE_INDEX };
        float OcclusionStrength{ 1.0f };
        Int32 OcclusionTexCoord{ 0 };

        // --- Emissive ---
        Vec3F EmissiveFactor{ 1, 1, 1 };
        float EmissiveStrength{ 1.0f };
        Int32 EmissiveIndex{ INVALID_TEXTURE_INDEX };
        Int32 EmissiveTexCoord{ 0 };

        // --- Alpha ---
        float AlphaCutoff{ 0.5f };
        Int32 AlphaMode{ 0 };// 0=OPAQUE, 1=MASK, 2=BLEND

        // --- Workflow ---
        Int32 Workflow{ 0 };// 0 = Metallic-Roughness, 1 = Specular-Glossiness
    };

    struct ShaderCameraParams {
        Mat4F View{};
        Mat4F Projection{};
        Vec4F CameraPosition{};
    };

    struct ShaderLightTypeParams {
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