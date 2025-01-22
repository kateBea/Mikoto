/**
 * Material.hh
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
        explicit Material(const std::string_view name = "Base Material", const Type type = Type::MATERIAL_TYPE_STANDARD)
            :   m_Type{ type }, m_Name{ name }
        {

        }

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetType() const -> Type { return m_Type; }
        MKT_NODISCARD auto IsActive() const -> bool { return m_Apply; }

        auto SetName(const std::string_view newName) -> void {
            m_Name = newName;
        }

        auto SetActive(const bool value) {
            m_Apply = value;
        }

        MKT_NODISCARD static constexpr auto GetTypeStr(Type type) -> std::string_view {
            switch(type) {
                case Type::MATERIAL_TYPE_STANDARD:
                    return MKT_STRINGIFY(MATERIAL_TYPE_STANDARD);
                case Type::MATERIAL_TYPE_PBR:
                    return MKT_STRINGIFY(MATERIAL_TYPE_PBR);
            }

            return "UNKNOWN_MATERIAL_TYPE";
        }

        MKT_NODISCARD static auto CreateStandardMaterial(const std::any& spec) -> std::shared_ptr<Material>;

        virtual ~Material() = default;

    private:
        Type m_Type{};
        std::string m_Name{};

        bool m_Apply{ true };
        glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };
}


#endif // MIKOTO_MATERIAL_HH
