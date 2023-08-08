/**
 * Material.hh
 * Created by kate on 6/30/23.
 * */

#ifndef KATE_ENGINE_MATERIAL_HH
#define KATE_ENGINE_MATERIAL_HH

// C++ Standard Library
#include <string>
#include <string_view>
#include <memory>

// Project Headers
#include <Utility/Common.hh>

namespace kaTe {
    class Material {
    public:
        enum class Type {
            NONE,
            STANDARD,
            COUNT,
        };

        explicit Material(std::string_view name = "Base Material", Type type = Type::STANDARD) : m_Name{ name }, m_Type{ type } {}

        Material(const Material& other) = default;
        Material(Material&& other) = default;

        auto operator=(const Material& other) -> Material& = default;
        auto operator=(Material&& other) -> Material& = default;

        KT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        auto SetName(std::string_view newName) -> void { m_Name = newName; }

        KT_NODISCARD static auto Create(Type matType) -> std::shared_ptr<Material>;

        virtual ~Material() = default;
    private:
        std::string m_Name{};
        Type m_Type{};
    };
}


#endif//KATE_ENGINE_MATERIAL_HH
