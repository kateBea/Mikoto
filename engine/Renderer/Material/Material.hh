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
#include <Utility/Common.hh>

namespace Mikoto {
    class Material {
    public:
        enum class Type {
            NONE,
            STANDARD,
            COUNT,
        };

        explicit Material(std::string_view name = "Base Material", Type type = Type::STANDARD)
            :   m_Type{ type }, m_Name{ name }
        {}

        Material(const Material& other) = default;
        Material(Material&& other) = default;

        auto operator=(const Material& other) -> Material& = default;
        auto operator=(Material&& other) -> Material& = default;

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        auto SetName(std::string_view newName) -> void { m_Name = newName; }

        MKT_NODISCARD static auto Create(Type matType) -> std::shared_ptr<Material>;

        virtual ~Material() = default;

    private:
        Type m_Type{};
        std::string m_Name{};
    };
}


#endif // MIKOTO_MATERIAL_HH
