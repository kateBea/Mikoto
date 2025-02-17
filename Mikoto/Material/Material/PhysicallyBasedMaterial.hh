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

    class PhysicallyBasedMaterial : public Material {
    public:
        explicit PhysicallyBasedMaterial( const std::string_view name = "PBR" )
            :   Material{ name, MaterialType::PBR }
        {

        }

        MKT_NODISCARD auto HasAlbedoMap() const -> bool { return m_AlbedoMap != nullptr; }
        MKT_NODISCARD auto HasNormalMap() const -> bool { return m_NormalMap != nullptr; }
        MKT_NODISCARD auto HasMetallicMap() const -> bool { return m_MetallicMap != nullptr; }
        MKT_NODISCARD auto HasRoughnessMap() const -> bool { return m_RoughnessMap != nullptr; }
        MKT_NODISCARD auto HasAmbientOcclusionMap() const -> bool { return m_AmbientOcclusionMap != nullptr; }

        MKT_NODISCARD auto GetAlbedoMap() const -> Texture2D* { return m_AlbedoMap; }
        MKT_NODISCARD auto GetNormal() const -> Texture2D* { return m_NormalMap; }
        MKT_NODISCARD auto GetMetallic() const -> Texture2D* { return m_MetallicMap; }
        MKT_NODISCARD auto GetRoughness() const -> Texture2D* { return m_RoughnessMap; }
        MKT_NODISCARD auto GetAO() const -> Texture2D* { return m_AmbientOcclusionMap; }

        ~PhysicallyBasedMaterial() override = default;

    protected:
        Texture2D* m_AlbedoMap{};
        Texture2D* m_NormalMap{};
        Texture2D* m_MetallicMap{};
        Texture2D* m_RoughnessMap{};
        Texture2D* m_AmbientOcclusionMap{};
    };
}


#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
