//
// Created by kate on 11/11/23.
//

#ifndef MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
#define MIKOTO_PHYSICALLY_BASED_MATERIAL_HH

#include <memory>

#include <Common/Types.hh>
#include <Common/Common.hh>

#include <Renderer/Material/Material.hh>
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    class PhysicallyBasedMaterial : public Material {
    public:
        explicit PhysicallyBasedMaterial( std::string_view name = "PBR" )
            :   Material{ name, Material::Type::MATERIAL_TYPE_PBR }
        {

        }

        PhysicallyBasedMaterial( const PhysicallyBasedMaterial& other ) = default;
        PhysicallyBasedMaterial( PhysicallyBasedMaterial&& other ) = default;

        auto operator=( const PhysicallyBasedMaterial& other ) -> PhysicallyBasedMaterial& = default;
        auto operator=( PhysicallyBasedMaterial&& other ) -> PhysicallyBasedMaterial& = default;

        MKT_NODISCARD auto HasAlbedoMap() const -> bool { return m_HasAlbedoMap; }
        MKT_NODISCARD auto HasNormalMap() const -> bool { return m_HasNormalMap; }
        MKT_NODISCARD auto HasMetallicMap() const -> bool { return m_HasMetallicMap; }
        MKT_NODISCARD auto HasRoughnessMap() const -> bool { return m_HasRoughnessMap; }
        MKT_NODISCARD auto HasAmbientOcclusionMap() const -> bool { return m_HasAoMap; }

        MKT_NODISCARD auto GetAlbedoMap() -> std::shared_ptr<Texture2D> { return m_AlbedoMap; }
        MKT_NODISCARD auto GetNormal() -> std::shared_ptr<Texture2D> { return m_NormalMap; }
        MKT_NODISCARD auto GetMetallic() -> std::shared_ptr<Texture2D> { return m_MetallicMap; }
        MKT_NODISCARD auto GetRoughness() -> std::shared_ptr<Texture2D> { return m_RoughnessMap; }
        MKT_NODISCARD auto GetAO() -> std::shared_ptr<Texture2D> { return m_AmbientOcclusionMap; }

    protected:
        std::shared_ptr<Texture2D> m_AlbedoMap{};
        std::shared_ptr<Texture2D> m_NormalMap{};
        std::shared_ptr<Texture2D> m_MetallicMap{};
        std::shared_ptr<Texture2D> m_RoughnessMap{};
        std::shared_ptr<Texture2D> m_AmbientOcclusionMap{};

        bool m_HasAlbedoMap{};
        bool m_HasNormalMap{};
        bool m_HasMetallicMap{};
        bool m_HasRoughnessMap{};
        bool m_HasAoMap{};
    };
}


#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
