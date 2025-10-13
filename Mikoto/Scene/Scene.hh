/**
 * Scene.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_SCENE_HH
#define MIKOTO_SCENE_HH

// C++ Standard Library
#include <memory>
#include <string_view>

// Third-Party Libraries
#include <entt/entt.hpp>
#include <ankerl/unordered_dense.h>

// Project Headers
#include <Assets/Model.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Scene/Entity.hh>

namespace Mikoto {

    struct EntityCreateInfo {
        std::string_view Name{};
        const Entity* Root{};
        ModelHandle Model{};

        auto WithName( std::string_view name ) -> EntityCreateInfo&;
        auto WithRoot( const Entity* root ) -> EntityCreateInfo&;
        auto WithModelMesh( ModelHandle modelMesh ) -> EntityCreateInfo&;
    };

    class Scene final {
    public:
        explicit Scene( std::string_view name = "Mikoto" );

        auto UpdateIdle( double deltaTime ) -> void;
        auto UpdateSimulate( double deltaTime ) -> void;

        auto RemoveEntity( UInt64 uniqueID ) -> void;

        auto FindByID( UInt64 uniqueID ) -> Entity*;
        auto FindFirstByName( std::string_view name ) -> Entity*;

        auto CreateEntity( const EntityCreateInfo& createInfo ) -> Entity*;

        MKT_NODISCARD auto GetName() const -> const std::string&;

        MKT_NODISCARD auto GetEntities() const -> decltype(auto);

        auto Clear() -> void;

        ~Scene();

    private:
        auto DestroyEntity( UInt64 uniqueID ) -> bool;
        auto AddEmptyEntity( std::string_view tagName, const Entity* root ) -> Entity*;

    private:
        std::string m_Name{};
        entt::registry m_Registry{};

        //ankl::unordered_dense::map<Size, EntityRef> m_Entities{};
    };
}

#endif// MIKOTO_SCENE_HH
