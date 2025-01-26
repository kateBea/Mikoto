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
#include <Models/SceneMetaData.hh>
#include <STL/Data/GenTree.hh>
#include <STL/Random/Random.hh>
#include <STL/Utility/Types.hh>
#include <Scene/Entity/Entity.hh>
#include <Renderer/Core/RendererBackend.hh>

namespace Mikoto {

    struct SceneRenderData {
        double TimeStep{};
        SceneCamera* Camera{};
        glm::vec4 ClearColor{};
    };

    class Scene final {
    public:

        explicit Scene( const std::string_view name = "Mikoto" ) : m_Name{ name } {}

        auto operator=( Scene&& other ) -> Scene& = default;
        auto operator==( const Scene& other ) const noexcept -> bool { return m_Name == other.m_Name; }

        ~Scene();

        auto Render( const SceneRenderData& data ) -> void;

        auto CreateEntity(std::string_view tagName, const Entity* root = nullptr, Model* model = nullptr, UInt64_T guid = GenerateGUID());
        auto FindEntity(std::string_view tagName);
        auto DestroyEntity( Entity& target ) -> bool;

        auto Clear() -> void;

        auto ResizeViewport( UInt32_T width, UInt32_T height ) -> void;

        MKT_NODISCARD auto GetRegistry() -> entt::registry& { return m_Registry; }
        MKT_NODISCARD auto GetHierarchy() -> GenTree<Entity>& { return m_Hierarchy; }
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }
        MKT_NODISCARD auto GetSceneMetaData() -> SceneMetaData&;

        // To remove
        auto CreateEmptyEntity( std::string_view tagName, const Entity* root = nullptr, UInt64_T guid = GenerateGUID() ) -> Entity;
        auto CreatePrefabEntity( std::string_view tagName, Model* model, Entity* root = nullptr, UInt64_T guid = GenerateGUID() ) -> Entity;

    private:
        std::string m_Name{};
        entt::registry m_Registry{};

        GenTree<Entity> m_Hierarchy{};

        Ref_T<SceneCamera> m_SceneCamera{};
        Ref_T<IRendererBackend> m_SceneRenderer{};

        SceneMetaData m_MetaData{};
    };
}

#endif// MIKOTO_SCENE_HH
