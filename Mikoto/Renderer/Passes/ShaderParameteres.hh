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
    static constexpr UInt32 MAX_RENDERABLE_ENTITIES{ 100'000 };
    static constexpr UInt32 MAX_NUM_JOINTS{ 128 };

    // Deprecated
    struct ShaderMaterialParams {
        Mat4F Transform{};

        Vec4F Albedo{};
        Vec4F Factors{}; // Metallness, Roughness, AO

        Int32 BonesID{ -1 }; // If it has bones its != -1
        UInt64 AnimatorID{}; // If it has bones its != -1

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
        UInt32 MaterialIndex{};
        Int32 BonesID{ -1 }; // If it has bones its != -1

        Vec4F Albedo{};
        Int32 AlbedoIndex{INVALID_TEXTURE_INDEX };

        float AlphaCutoff{ 0.5f };
        float MetallicFactor{ 1.0f };
        float RoughnessFactor{ 1.0f };
        float OcclusionStrength{ 1.0f };

        Vec3F EmissiveFactors{ 1.0f, 1.0f, 1.0f };
        float EmissiveIntensity{ 1.0 };

        Int32 NormalIndex{ INVALID_TEXTURE_INDEX };
        Int32 MetallicIndex{ INVALID_TEXTURE_INDEX };
        Int32 RoughnessIndex{ INVALID_TEXTURE_INDEX };
        Int32 AoIndex{ INVALID_TEXTURE_INDEX };
        Int32 EmissiveIndex{ INVALID_TEXTURE_INDEX };
    };

    struct ShaderMaterial {
        Vec4F BaseColorFactor{};
        Vec4F EmissiveFactor{};
        Vec4F DiffuseFactor{};
        Vec4F SpecularFactor{};

        float Workflow{};

        float MetallicFactor;
        float RoughnessFactor;
        float AlphaMask;
        float AlphaMaskCutoff;
        float EmissiveStrength;

        Int32 BaseColorTextureSet{};
        Int32 PhysicalDescriptorTextureSet{};
        Int32 NormalTextureSet{};
        Int32 OcclusionTextureSet{};
        Int32 EmissiveTextureSet{};

        // Texture indices
        Int32 NormalIndex{ INVALID_TEXTURE_INDEX };
        Int32 MetallicIndex{ INVALID_TEXTURE_INDEX };
        Int32 RoughnessIndex{ INVALID_TEXTURE_INDEX };
        Int32 AoIndex{ INVALID_TEXTURE_INDEX };
        Int32 EmissiveIndex{ INVALID_TEXTURE_INDEX };
    };

    struct EnvironmentConstants {
        Vec4F GridSize{};
        float Exposure{};
        float Gamma{};
        float MaxReflectionLOD{};

        Int32 IsSkyboxActive{};
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