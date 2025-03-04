/**
 * StandardMaterial.hh
 * Created by kate on 6/30/23.
 * */

#ifndef MIKOTO_MATERIAL_HH
#define MIKOTO_MATERIAL_HH

// C++ Standard Library
#include <string>
#include <string_view>
#include <memory>

// Third-Party Headers
#include <glm/glm.hpp>

// Project Headers
#include <Common/Common.hh>
#include <Material/Texture/Texture2D.hh>
#include <Models/Enums.hh>

namespace Mikoto {

    class Material {
    public:
        explicit Material(const std::string_view name = "Base Material", const MaterialType type = MaterialType::STANDARD)
            :   m_Type{ type }, m_Name{ name }
        {

        }

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetType() const -> MaterialType { return m_Type; }
        MKT_NODISCARD auto GetColor() const -> const glm::vec4& { return m_Color; }
        MKT_NODISCARD auto IsActive() const -> bool { return m_Apply; }

        auto SetActive(const bool value) -> void { m_Apply = value; }
        auto SetName(const std::string_view newName) -> void { m_Name = newName; }
        auto SetColor(auto&&... args) -> void { m_Color = glm::vec4(std::forward<decltype(args)>(args)...); }

        virtual auto SetTexture( Texture* map, MapType type ) -> void = 0;

        MKT_NODISCARD static constexpr auto GetTypeStr( const MaterialType type) -> std::string_view {
            switch(type) {
                case MaterialType::STANDARD:
                    return MKT_STRINGIFY(MATERIAL_TYPE_STANDARD);
                case MaterialType::PBR:
                    return MKT_STRINGIFY(MATERIAL_TYPE_PBR);
            }

            return "UNKNOWN_MATERIAL_TYPE";
        }

        virtual ~Material() = default;

    protected:
        MaterialType m_Type{};
        std::string m_Name{};

        bool m_Apply{ true };
        glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };
}


#endif // MIKOTO_MATERIAL_HH
