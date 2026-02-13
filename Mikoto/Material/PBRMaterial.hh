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

#ifndef MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
#define MIKOTO_PHYSICALLY_BASED_MATERIAL_HH

#include <ankerl/unordered_dense.h>

#include <Assets/Texture.hh>
#include <Common/Common.hh>
#include <Material/Material.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {

    enum class PBR_Workflow {
        MetallicRoughness,
        SpecularGlossiness,
        Unlit
    };

    enum class PBR_AlphaMode {
        Opaque,
        Mask,
        Blend
    };

    struct MaterialProperties {
        std::string Name{};

        Int32 Index{ -1 };

        PBR_Workflow Workflow{ PBR_Workflow::MetallicRoughness };

        // Base color/Albedo
        Vec4F BaseColorFactor{1.f, 1.f, 1.f, 1.f};
        std::string BaseColorTexture{};

        // Metallic-Roughness workflow
        float MetallicFactor{ 1.f };
        float RoughnessFactor{ 1.f };
        std::string MetallicRoughnessTexture{};

        // Specular-Glossiness workflow (FBX/OBJ/glTF extension)
        Vec3F DiffuseFactor{1.f, 1.f, 1.f};
        std::string DiffuseTexture{};
        Vec3F SpecularFactor{1.f, 1.f, 1.f};

        std::string SpecularGlossinessTexture{};
        float GlossinessFactor{ 1.f };

        // Normal mapping
        std::string NormalTexture{};
        float NormalScale{ 1.f };

        // Occlusion
        std::string OcclusionTexture{};
        float OcclusionStrength{ 1.f };

        // Emissive
        Vec3F EmissiveFactor{0.f, 0.f, 0.f};
        float EmissiveStrength{ 1.f };
        std::string EmissiveTexture{};

        // Alpha
        PBR_AlphaMode alphaMode{ PBR_AlphaMode::Opaque };
        float AlphaCutoff{ 0.5f };

        // UV sets
        Int32 BaseColorTexCoord{};
        Int32 MetallicRoughnessTexCoord{};
        Int32 NormalTexCoord{};
        Int32 OcclusionTexCoord{};
        Int32 EmissiveTexCoord{};

        ankerl::unordered_dense::map<std::string, TextureHandle> TexturesByUri{};
    };

    class PBRMaterial final : public Material {
    public:

        explicit PBRMaterial( std::string_view name = "PBR" );
        explicit PBRMaterial( const MaterialProperties& props );

        auto RemoveTextureType( MapType type ) -> void;
        auto SetTextureType( MapType type, const TextureHandle &texture ) -> void;

        MKT_NODISCARD auto IsOpaque() const -> bool;
        MKT_NODISCARD auto IsTransparent() const -> bool;

        MKT_NODISCARD auto HasTextureType( MapType type ) const -> bool;
        MKT_NODISCARD auto GetTextureType( MapType type ) const -> TextureHandle;

        auto SetBlendMode( Blending alphaMode ) -> void;
        auto SetSamplingMode( SamplerFilter filtering ) -> void;

        auto SetAlpha( float alpha ) -> void;
        auto SetMetallicFactor( float metallic ) -> void;
        auto SetRoughnessFactor( float roughness ) -> void;
        auto SetReflectanceFactor( float reflectance ) -> void;
        auto SetAoFactor( float ao ) -> void;
        auto SetEmissiveFactors( const Vec3F& factors ) -> void;
        auto SetEmissiveIntensity( float emissive ) -> void;

        MKT_NODISCARD auto GetAlpha() const -> float;
        MKT_NODISCARD auto GetMetallicFactor() const -> float;
        MKT_NODISCARD auto GetRoughnessFactor() const -> float;
        MKT_NODISCARD auto GetAoFactor() const -> float;
        MKT_NODISCARD auto GetReflectanceFactor() const -> float;
        MKT_NODISCARD auto GetEmissiveFactors() const -> const Vec3F&; // Basically the color of light emitted
        MKT_NODISCARD auto GetEmissiveIntensity() const -> float;

        ~PBRMaterial() override ;

    private:

        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        Blending m_BlendMode{ Blending::MODE_OPAQUE };
        SamplerFilter m_Filtering{ SamplerFilter::FILTER_NEAREST };

        float m_Alpha{ 1.0f };
        float m_Metallic{ 0.2f };
        float m_Roughness{ 5.4f };
        float m_Emissive{ 0.0f }; // Objects do not emit light by default
        Vec3F m_EmissiveFactors{ 0.0f, 0.0f, 0.0f }; // Emissive color
        float m_AmbientOcclusion{ 0.4f };
        float m_ReflectanceFactor{ 0.4f };

        // Note: Materials reference both textures and samplers.
        // Textures provide the image data, while samplers define how that data is read
        // (filtering, addressing, LOD behavior, etc.).
        // This separation allows the same texture to be reused across materials
        // with different sampling settings.
        ankerl::unordered_dense::map<MapType, TextureHandle> m_Textures{};
        ankerl::unordered_dense::map<MapType, SamplerHandle> m_Samplers{};
    };
}// namespace Mikoto


#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
