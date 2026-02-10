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

    struct MaterialCreateInfo {

    };

    enum class PBR_WorkflowType : Int32 {
        METALLIC_ROUGHNESS = 0,
        SPECULAR_GLOSSINESS = 1,
    };

    enum class PBR_AlphaMode : Int32 {
        ALPHA_OPAQUE = 0,
        ALPHA_MASK = 1,
        ALPHA_BLEND = 2,
    };

    class PBRMaterial final : public Material {
    public:

        explicit PBRMaterial( std::string_view name = "PBR" );

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
