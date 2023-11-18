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
#include <Common/Types.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    class StandardMaterial : public Material {
    public:
        explicit StandardMaterial( std::string_view name = "Material - Standard" )
            :   Material{ name, Material::Type::MATERIAL_TYPE_STANDARD }
        {

        }

        MKT_NODISCARD auto GetShininess() const -> float { return m_Shininess; }
        MKT_NODISCARD auto GetColor() const -> const glm::vec4& { return m_Color; }
        MKT_NODISCARD auto GetDiffuseMap() -> std::shared_ptr<Texture2D> { return m_DiffuseTexture; }
        MKT_NODISCARD auto GetSpecularMap() -> std::shared_ptr<Texture2D> { return m_SpecularTexture; }

        MKT_NODISCARD auto HasSpecularMap() const -> bool { return m_HasSpecular; }
        MKT_NODISCARD auto HasDiffuseMap() const -> bool { return m_HasDiffuse; }

        auto SetShininess(float value) -> void { if (value > 0.0f) { m_Shininess = value; } }
        auto SetColor(auto&&... args) -> void { m_Color = glm::vec4(std::forward<decltype(args)>(args)...); }

        MKT_NODISCARD static auto GetName() -> const std::string& {
            static std::string result{ "StandardMaterial" };
            return result;
        }

    protected:
        glm::vec4 m_Color{ 0.20f, 0.20f, 0.20f, 1.0f };
        std::shared_ptr<Texture2D> m_DiffuseTexture{};
        std::shared_ptr<Texture2D> m_SpecularTexture{};

        bool m_HasDiffuse{ false };
        bool m_HasSpecular{ false };

        float m_Shininess{};
    };
}

#endif// MIKOTO_DEFAULT_MATERIAL_HH
