/**
 * StandardMaterial.hh
 * Created by kate on 6/30/23.
 * */

#ifndef MIKOTO_MATERIAL_HH
#define MIKOTO_MATERIAL_HH

// C++ Standard Library
#include <string>
#include <string_view>

// Third-Party Headers
#include <glm/glm.hpp>

// Project Headers
#include <Common/Common.hh>
#include <Common/ReferenceCounted.hh>
#include <Library/Data/ResourcePool.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    class Material : public IResource {
    public:
        explicit Material(const std::string_view name = "Base Material")
            :   m_Name{ name }
        {}

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetColor() const -> const Vec4F& { return m_Color; }

        // Material name
        auto SetName(const std::string_view newName) -> void {
            m_Name = newName;
        }

        // Material base color
        auto SetColor( auto&&... args ) -> void {
            m_Color = Vec4F( std::forward<decltype( args )>( args )... );
        }

        ~Material() override = default;

    protected:
        std::string m_Name{};
        Vec4F m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    using MaterialHandle = Ref<Material>;
}


#endif // MIKOTO_MATERIAL_HH
