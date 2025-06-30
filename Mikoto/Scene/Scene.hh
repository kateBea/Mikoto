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
#include <Assets/MeshFactory.hh>
#include <Assets/Model.hh>
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Scene/Entity.hh>

namespace Mikoto {

    struct EntityCreateInfo {
        std::string Name{};
        const Entity* Root{};
        ModelHandle ModelMesh{};

        auto WithName( std::string_view name ) -> EntityCreateInfo&;
        auto WithRoot( const Entity* root ) -> EntityCreateInfo&;
        auto WithModelMesh( const Model* modelMesh ) -> EntityCreateInfo&;
    };

    class Scene final {
    public:
        using EntiContainer_T = ankerl::unordered_dense::map<UInt64_T, Scope_T<Entity>>;

        explicit Scene( std::string_view name = "Mikoto" );

        auto UpdateIdle( double deltaTime ) -> void;
        auto UpdateSimulate( double deltaTime ) -> void;

        auto RemoveEntity( UInt64_T uniqueID ) -> void;

        auto FindByID( UInt64_T uniqueID ) -> Entity*;
        auto FindFirstByName( std::string_view name ) -> Entity*;

        auto CreateEntity( const EntityCreateInfo& createInfo ) -> Entity*;

        MKT_NODISCARD auto GetName() const -> const std::string&;

        MKT_NODISCARD auto GetEntities() -> EntiContainer_T&;
        MKT_NODISCARD auto GetEntities() const -> const EntiContainer_T&;

        auto Clear() -> void;

        ~Scene();

    private:
        auto RemoveQueuedEntities() -> void;
        auto DestroyEntity( UInt64_T uniqueID ) -> bool;
        auto AddEmptyEntity( std::string_view tagName, const Entity* root ) -> Entity*;

    private:
        auto OnTextComponentAttach(entt::registry& reg, entt::entity entity) -> void;
        auto OnRenderComponentAttach(entt::registry& reg, entt::entity entity) -> void;
        auto OnRigidBodyComponentAttach(entt::registry& reg, entt::entity entity) -> void;

        static auto ConstructCommonComponents(Entity* entity, std::string_view name) -> void;

        static auto CreateRigidBody( entt::entity entity, TransformComponent& transformComponent, RigidBodyComponent& rigidBodyComponent ) -> void;

    private:
        std::string m_Name{};
        entt::registry m_Registry{};

        std::vector<UInt64_T> m_ToRemoveEntities{};

        EntiContainer_T m_Entities{};
    };
}

#endif// MIKOTO_SCENE_HH
