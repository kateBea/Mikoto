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
    static constexpr UInt32 MAX_RENDERABLE_ENTITIES{ 500'000 };
    static constexpr UInt32 MAX_NUM_JOINTS{ 128 };

    struct ShaderMaterial {
        Vec4F BaseColorFactor{};
        Vec4F EmissiveFactor{};
        Vec4F DiffuseFactor{};
        Vec4F SpecularFactor{};

        Int32 Workflow{};

        float MetallicFactor{};
        float RoughnessFactor{};
        float AlphaMask{};
        float AlphaMaskCutoff{};
        float EmissiveStrength{};

        Int32 BaseColorTextureSet{};
        Int32 MetallicRoughnessTextureSet{};
        Int32 SpecilarGlossinessSet{};
        Int32 NormalTextureSet{};
        Int32 OcclusionTextureSet{};
        Int32 EmissiveTextureSet{};

        // Texture indices
        Int32 AlbedoIndex{ INVALID_TEXTURE_INDEX };
        Int32 DiffuseIndex{ INVALID_TEXTURE_INDEX };
        Int32 NormalIndex{ INVALID_TEXTURE_INDEX };
        Int32 MetallicIndex{ INVALID_TEXTURE_INDEX };
        Int32 RoughnessIndex{ INVALID_TEXTURE_INDEX };
        Int32 AoIndex{ INVALID_TEXTURE_INDEX };
        Int32 EmissiveIndex{ INVALID_TEXTURE_INDEX };
        Int32 MetallicRoughnessIndex{ INVALID_TEXTURE_INDEX };
        Int32 SpecilarGlossinessIndex{ INVALID_TEXTURE_INDEX };
    };

    struct ShaderMesh {
        Mat4F Transform{};
        Mat4F InverseModelView{};

        Int32 AnimatorID{ -1 };
    };

    struct FinalShadingConstants {
        Vec4F GridSize{};
        float Exposure{};
        float Gamma{};
        float MaxReflectionLOD{};

        UInt32 ActiveLights{};

        Int32 EnableSSAO{ MKT_SHADER_FALSE };
        Int32 UseBlurred{ MKT_SHADER_FALSE };
        float SSAOIntensity{ 1.f };

        Int32 PrefilteredCubeMipLevels{};
        float ScaleIBLAmbient{ 1.0f };
        Int32 Step{ 1 };

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