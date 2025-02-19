//
// Created by kate on 11/9/23.
//

#ifndef MIKOTO_DEFAULT_MATERIAL_HH
#define MIKOTO_DEFAULT_MATERIAL_HH

#include <string>
#include <string_view>
#include <utility>

#include <glm/glm.hpp>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Material/Texture/Texture2D.hh>
#include <Material/Core/Material.hh>

namespace Mikoto {

    struct StandardMaterialCreateInfo {
        std::string name{};

        Texture2D* DiffuseMap{ nullptr };
        Texture2D* SpecularMap{ nullptr };
    };

    class StandardMaterial : public Material {
    public:

        MKT_NODISCARD auto GetShininess() const -> float { return m_Shininess; }
        MKT_NODISCARD auto GetDiffuseMap() const -> Texture2D* { return m_DiffuseTexture; }
        MKT_NODISCARD auto GetSpecularMap() const -> Texture2D* { return m_SpecularTexture; }

        // Textures may use an empty texture as a placeholder
        MKT_NODISCARD auto HasSpecularMap() const -> bool { return m_HasSpecularTexture; }
        MKT_NODISCARD auto HasDiffuseMap() const -> bool { return m_HasDiffuseTexture; }

        auto SetShininess( const float value ) -> void { if (value > 0.0f) { m_Shininess = value; } }
        auto SetColor(auto&&... args) -> void { m_Color = glm::vec4(std::forward<decltype(args)>(args)...); }

        MKT_NODISCARD static auto Create(const StandardMaterialCreateInfo& createInfo) -> Scope_T<StandardMaterial>;

    protected:
        explicit StandardMaterial( const StandardMaterialCreateInfo& createInfo )
            :   Material{ createInfo.name, MaterialType::STANDARD },
                m_DiffuseTexture{ createInfo.DiffuseMap },
                m_SpecularTexture{ createInfo.SpecularMap }
        {

        }

    protected:
        Texture2D* m_DiffuseTexture{};
        Texture2D* m_SpecularTexture{};

        bool m_HasSpecularTexture{ true };
        bool m_HasDiffuseTexture{ true };

        float m_Shininess{ 32.0f };
    };
}

#endif// MIKOTO_DEFAULT_MATERIAL_HH
