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

// Project Headers
#include <Common/Common.hh>
#include <Library/Data/GenTree.hh>
#include <Library/Random/Random.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Scene/Camera/SceneCamera.hh>
#include <Scene/Scene/Entity.hh>

namespace Mikoto {

    struct EntityCreateInfo {
        std::string_view Name{};
        const Entity* Root{};
        const Model* ModelMesh{};
    };

    class Scene final {
    public:
        explicit Scene( const std::string_view name = "Mikoto" )
            : m_Name{ name } {}

        auto Update( double deltaTime ) -> void;

        auto DestroyEntity( UInt64_T uniqueID ) -> bool;
        auto FindEntity( UInt64_T uniqueID ) -> Entity*;
        auto CreateEntity( const EntityCreateInfo& createInfo ) -> Entity*;

        auto Clear() -> void;

        // Camera and renderer
        auto SetCamera( const SceneCamera& camera ) -> void;
        auto SetRenderer( RendererBackend& renderer ) -> void;

        auto OnViewPortResize(float width, float height) -> void;

        // Getters
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetHierarchy() -> GenTree<Entity*>& { return m_Hierarchy; }

        ~Scene();

    private:
        static auto SetupEntityBaseProperties(Entity& entity, std::string_view name) -> void;

        auto AddEmptyEntity(std::string_view tagName, const Entity *root) -> Entity*;

        auto RemoveFromLights( UInt64_T uniqueID ) -> void;
        auto RemoveFromEntities( UInt64_T uniqueID ) -> Scope_T<Entity>;
        auto RemoveFromHierarchy( Entity& target ) -> void;

    private:
        std::string m_Name{};
        entt::registry m_Registry{};

        // This might be here temporarily; it makes not much sense for objects to be tied
        // to another in the engine side, that is more of an editor feature that is particularly
        // useful if we want to recursively apply transformations for instance starting from a root
        // node down to its children in the hierarchy
        GenTree<Entity*> m_Hierarchy{};

        std::vector<Entity*> m_Lights{};
        std::vector<Scope_T<Entity>> m_Entities{};

        const SceneCamera* m_SceneCamera{};
        RendererBackend* m_SceneRenderer{};
    };
}// namespace Mikoto

#endif// MIKOTO_SCENE_HH
