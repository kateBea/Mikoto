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

#include <../../Core/Common.hh>
#include <Library/Utility/Types.hh>
#include <array>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace mikoto::renderer {

    static constexpr Int32 INVALID_TEXTURE_INDEX{ -1 };
    static constexpr UInt32 MAX_LIGHTS{ 10000 };
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
        Int32 SpecularGlossinessSet{};
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
        Int32 SpecularGlossinessIndex{ INVALID_TEXTURE_INDEX };

        Int32 IsBloomy{ MKT_SHADER_FALSE };
    };

    struct ShaderMesh {
        Mat4F Transform{};
        Mat4F InverseModelView{};

        // For vertex pulling, this tells the offset into the  array of vertices
        UInt32 MeshNodeOffsetVertex{};
        UInt32 MeshNodeOffsetIndex{};

        Int32 MatricesIndex{ -1 };
        Int32 HasArmature{ MKT_SHADER_FALSE };
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