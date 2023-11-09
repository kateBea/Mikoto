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

// Project Headers
#include "Common/Common.hh"

#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    struct DefaultMaterialCreateSpec {
        std::shared_ptr<Texture2D> DiffuseMap{ nullptr };
        std::shared_ptr<Texture2D> SpecularMap{ nullptr };
    };

    class Material {
    public:
        enum class Type {
            MATERIAL_TYPE_UNKNOWN,
            MATERIAL_TYPE_STANDARD,
            COUNT,
        };

        explicit Material(std::string_view name = "Base Material", Type type = Type::MATERIAL_TYPE_UNKNOWN)
            :   m_Type{ type }, m_Name{ name }
        {

        }

        Material(const Material& other) = default;
        Material(Material&& other) = default;

        auto operator=(const Material& other) -> Material& = default;
        auto operator=(Material&& other) -> Material& = default;

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetType() const -> Type { return m_Type; }
        auto SetName(std::string_view newName) -> void { m_Name = newName; }

        MKT_NODISCARD static constexpr auto GetTypeStr(Type type) -> std::string_view {
            switch(type) {
                case Type::MATERIAL_TYPE_UNKNOWN:   return "MATERIAL_TYPE_UNKNOWN";
                case Type::MATERIAL_TYPE_STANDARD:  return "MATERIAL_TYPE_STANDARD";
                case Type::COUNT:                   return "Unknown";
            }
        }

        MKT_NODISCARD static auto CreateStandardMaterial(const DefaultMaterialCreateSpec& spec) -> std::shared_ptr<Material>;

        virtual ~Material() = default;

    private:
        Type m_Type{};
        std::string m_Name{};
    };
}


#endif // MIKOTO_MATERIAL_HH
