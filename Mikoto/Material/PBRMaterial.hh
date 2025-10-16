//
// Created by kate on 11/11/23.
//

#ifndef MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
#define MIKOTO_PHYSICALLY_BASED_MATERIAL_HH

#include <memory>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Material/Texture2D.hh>

namespace Mikoto {

    struct PBRMaterialCreateSpec {
        TextureHandle AlbedoMap{};
        TextureHandle NormalMap{};
        TextureHandle MetallicMap{};
        TextureHandle RoughnessMap{};
        TextureHandle AmbientOcclusionMap{};
    };

    enum class PBRBlendMode {
        BLEND_MODE_OPAQUE,    // Fully opaque (no transparency)
        BLEND_MODE_MASKED,    // Cutout transparency (alpha test)
        BLEND_MODE_BLEND,     // Alpha blending for transparency
        BLEND_MODE_ADDITIVE,  // Additive blending (glowing effects)
        BLEND_MODE_MULTIPLY,  // Multiplicative blending (darkening effects)
    };

    enum class PBRSamplerMode {
        SAMPLER_MODE_NEAREST,
        SAMPLER_MODE_LINEAR,
        SAMPLER_MODE_ANISOTROPIC,
    };

    class PBRMaterial final : public Material {
    public:
        struct Parameters {
            float Metallic{ 0.2f };
            float Roughness{ 0.4f };
            float Emissive{ 0.4f };
            float AmbientOcclusion{ 0.4f };
            float ReflectanceFactor{ 0.4f };
        };

    public:
        auto RemoveTextureType( TextureType type ) -> void;
        auto SetTextureType( TextureType type, TextureHandle texture ) -> void;

        MKT_NODISCARD auto IsOpaque() const -> bool;
        MKT_NODISCARD auto IsTransparent() const -> bool;

        MKT_NODISCARD auto HasTextureType( TextureType type ) const -> bool;
        MKT_NODISCARD auto GetTextureType( TextureType type ) const -> TextureHandle;

        auto SetEmissiveFactor( const glm::vec4& emissive ) -> void;
        auto SetRoughnessFactor( float roughness ) -> void;
        auto SetMetalicFactor( float metallic ) -> void;
        auto SetReflectanceFactor( float reflectance ) -> void;
        auto SetBlendMode( PBRBlendMode alphaMode ) -> void;
        auto SetSamplingMode( PBRSamplerMode sampler ) -> void;

        MKT_NODISCARD auto GetParameters() const -> const Parameters&;

        ~PBRMaterial() override = default;

    protected:
        explicit PBRMaterial( const PBRMaterialCreateSpec& createInfo )
            : Material{ "PBR Material" },
              m_AlbedoMap{ createInfo.AlbedoMap },
              m_NormalMap{ createInfo.NormalMap },
              m_MetallicMap{ createInfo.MetallicMap },
              m_RoughnessMap{ createInfo.RoughnessMap },
              m_AmbientOcclusionMap{ createInfo.AmbientOcclusionMap } {}

        explicit PBRMaterial( const std::string_view name = "PBR" )
            : Material{ name } {
        }

    protected:
        Parameters m_Params{};

        TextureHandle m_AlbedoMap{};
        TextureHandle m_NormalMap{};
        TextureHandle m_MetallicMap{};
        TextureHandle m_RoughnessMap{};
        TextureHandle m_AmbientOcclusionMap{};
    };
}// namespace Mikoto


#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
