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
        std::string Name{};

        Texture2D* AlbedoMap{ nullptr };
        Texture2D* NormalMap{ nullptr };
        Texture2D* MetallicMap{ nullptr };
        Texture2D* RoughnessMap{ nullptr };
        Texture2D* AmbientOcclusionMap{ nullptr };

        auto SetAlbedoTexture( Texture2D* texture ) -> PBRMaterialCreateSpec&;
    };

    enum class PBRTextureType {
        TEXTURE_TYPE_ALBEDO,              // Base color texture
        TEXTURE_TYPE_NORMAL,              // Normal map
        TEXTURE_TYPE_ROUGHNESS,           // Roughness map
        TEXTURE_TYPE_METALLIC,            // Metalness map
        TEXTURE_TYPE_AMBIENT_OCCLUSION,   // Ambient occlusion map
        TEXTURE_TYPE_EMISSIVE,            // Emissive map
    };

    enum class PBRBlendMode {
        BLEND_MODE_OPAQUE,    // Fully opaque (no transparency)
        BLEND_MODE_MASKED,    // Cutout transparency (alpha test)
        BLEND_MODE_BLEND,     // Alpha blending for transparency
        BLEND_MODE_ADDITIVE,  // Additive blending (glowing effects)
        BLEND_MODE_MULTIPLY,  // Multiplicative blending (darkening effects)
    };

    enum class PBRSamplerMode {
        SAMPLER_MODE_NEAREST,    // Nearest neighbor filtering (pixelated)
        SAMPLER_MODE_LINEAR,     // Linear filtering (smooth)
        SAMPLER_MODE_ANISOTROPIC,// Anisotropic filtering (better texture quality at angles)
    };

    class PBRMaterial : public Material {
    public:
        struct Parameters {
            float Metallic{ 0.2f };
            float Roughness{ 0.4f };
            float AmbientOcclusion{ 0.4f };
            float Emissive{ 0.4f };
            float ReflectanceFactor{ 0.4f };
        };

    public:
        auto RemoveTextureType( PBRTextureType type ) -> void;
        auto SetTextureType( PBRTextureType type, Texture2D* texture ) -> void;

        MKT_NODISCARD auto IsOpaque() const -> bool;

        MKT_NODISCARD auto HasTextureType( PBRTextureType type ) const -> bool;
        MKT_NODISCARD auto GetTextureType( PBRTextureType type ) const -> Texture2D*;

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
            : Material{ createInfo.Name},
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

        Texture2D* m_AlbedoMap{};
        Texture2D* m_NormalMap{};
        Texture2D* m_MetallicMap{};
        Texture2D* m_RoughnessMap{};
        Texture2D* m_AmbientOcclusionMap{};
    };
}// namespace Mikoto


#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
