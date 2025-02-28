//
// Created by kate on 11/11/23.
//

#ifndef MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
#define MIKOTO_PHYSICALLY_BASED_MATERIAL_HH

#include <memory>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Material/Core/Material.hh>
#include <Material/Texture/Texture2D.hh>

namespace Mikoto {
    struct PBRMaterialCreateSpec {
        std::string Name{};

        Texture2D* AlbedoMap{ nullptr };
        Texture2D* NormalMap{ nullptr };
        Texture2D* MetallicMap{ nullptr };
        Texture2D* RoughnessMap{ nullptr };
        Texture2D* AmbientOcclusionMap{ nullptr };
    };

    class PBRMaterial : public Material {
    public:
        MKT_NODISCARD virtual auto HasAlbedoMap() const -> bool = 0;
        MKT_NODISCARD virtual auto HasNormalMap() const -> bool = 0;
        MKT_NODISCARD virtual auto HasMetallicMap() const -> bool = 0;
        MKT_NODISCARD virtual auto HasRoughnessMap() const -> bool  = 0;
        MKT_NODISCARD virtual auto HasAmbientOcclusionMap() const -> bool = 0;

        MKT_NODISCARD auto GetAlbedoMap() const -> Texture2D* { return m_AlbedoMap; }
        MKT_NODISCARD auto GetNormalMap() const -> Texture2D* { return m_NormalMap; }
        MKT_NODISCARD auto GetMetallicMap() const -> Texture2D* { return m_MetallicMap; }
        MKT_NODISCARD auto GetRoughnessMap() const -> Texture2D* { return m_RoughnessMap; }
        MKT_NODISCARD auto GetAOMap() const -> Texture2D* { return m_AmbientOcclusionMap; }

        MKT_NODISCARD auto GetMetallicFactor() const -> float { return m_Metallic; }
        MKT_NODISCARD auto GetAlbedoFactors() const -> const glm::vec4& { return m_Color; }
        MKT_NODISCARD auto GetRoughnessFactor() const -> float { return m_Roughness; }
        MKT_NODISCARD auto GetAmbientOcclusionFactor() const -> float { return m_AmbientOcclusion; }

        MKT_NODISCARD auto SetMetallicFactor(const float value) -> void { m_Metallic = value; }
        MKT_NODISCARD auto SetAlbedoFactors(const glm::vec4& value) -> void { m_Color = value; }
        MKT_NODISCARD auto SetRoughnessFactor(const float value) -> void { m_Roughness = value; }
        MKT_NODISCARD auto SetAmbientOcclusionFactor(const float value) -> void { m_AmbientOcclusion = value; }

        virtual auto RemoveMap( MapType type ) -> void = 0;

        MKT_NODISCARD static auto Create( const PBRMaterialCreateSpec& createInfo ) -> Scope_T<PBRMaterial>;

        ~PBRMaterial() override = default;

    protected:
        explicit PBRMaterial( const PBRMaterialCreateSpec& createInfo )
            : Material{ createInfo.Name, MaterialType::PBR },
              m_AlbedoMap{ createInfo.AlbedoMap },
              m_NormalMap{ createInfo.NormalMap },
              m_MetallicMap{ createInfo.MetallicMap },
              m_RoughnessMap{ createInfo.RoughnessMap },
              m_AmbientOcclusionMap{ createInfo.AmbientOcclusionMap }
        {}

        explicit PBRMaterial( const std::string_view name = "PBR" )
            : Material{ name, MaterialType::PBR } {
        }

    protected:

        float m_Metallic{ 0.2f };
        float m_Roughness{ 0.4f };
        float m_AmbientOcclusion{ 0.4f };

        Texture2D* m_AlbedoMap{};
        Texture2D* m_NormalMap{};
        Texture2D* m_MetallicMap{};
        Texture2D* m_RoughnessMap{};
        Texture2D* m_AmbientOcclusionMap{};
    };
}// namespace Mikoto


#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
