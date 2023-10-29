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
        std::shared_ptr<Texture2D> DiffuseMap{};
    };

    class Material {
    public:
        enum class Type {
            MATERIAL_TYPE_UNKNOWN,
            MATERIAL_TYPE_STANDARD,
            MATERIAL_TYPE_COLORED,
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


        MKT_NODISCARD static auto CreateColoredMaterial() -> std::shared_ptr<Material>;
        MKT_NODISCARD static auto CreateStandardMaterial(const DefaultMaterialCreateSpec& spec) -> std::shared_ptr<Material>;

        virtual ~Material() = default;

    private:
        Type m_Type{};
        std::string m_Name{};
    };
}


#endif // MIKOTO_MATERIAL_HH
