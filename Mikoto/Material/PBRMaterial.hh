//
// Created by kate on 11/11/23.
//

#ifndef MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
#define MIKOTO_PHYSICALLY_BASED_MATERIAL_HH

#include <ankerl/unordered_dense.h>

#include <Assets/Texture.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Material/Texture2D.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {

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
        auto SetEmissiveFactor( float emissive ) -> void;

        MKT_NODISCARD auto GetAlpha() const -> float;
        MKT_NODISCARD auto GetMetallicFactor() const -> float;
        MKT_NODISCARD auto GetRoughnessFactor() const -> float;
        MKT_NODISCARD auto GetReflectanceFactor() const -> float;
        MKT_NODISCARD auto GetEmissiveFactor() const -> float;

        ~PBRMaterial() override ;

    private:

        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        Blending m_BlendMode{ Blending::MODE_OPAQUE };
        SamplerFilter m_Filtering{ SamplerFilter::FILTER_NEAREST };

        float m_Alpha{ 1.0f };
        float m_Metallic{ 0.2f };
        float m_Roughness{ 0.4f };
        float m_Emissive{ 0.4f };
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
