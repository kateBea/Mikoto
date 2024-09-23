/**
 * Scene.cc
 * Created by kate on 6/24/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>
#include <algorithm>

// Third-Party Libraries
#include <entt/entt.hpp>

// Project Headers
#include <Assets/AssetsManager.hh>
#include <Common/Random.hh>
#include <Common/RenderingUtils.hh>
#include <Renderer/Renderer.hh>

#include "Scene/Component.hh"
#include "Scene/EditorCamera.hh"
#include "Scene/Entity.hh"
#include "Scene/Scene.hh"

namespace Mikoto {

    static auto DestroyEntity(entt::registry reg, entt::entity ent) {

    }

    auto Scene::OnRuntimeUpdate(double ts) -> void {
        UpdateScripts();

        std::shared_ptr<SceneCamera> mainCam{};
        bool sceneHasMainCam{ false };
        auto viewForCameraLookUp{ m_Registry.view<TransformComponent, CameraComponent>() };

        for (const auto& entity : viewForCameraLookUp) {
            TransformComponent& transform{ viewForCameraLookUp.get<TransformComponent>(entity) };
            CameraComponent& camera{ viewForCameraLookUp.get<CameraComponent>(entity) };
            sceneHasMainCam = camera.IsMainCamera();

            if (sceneHasMainCam) {
                mainCam = camera.GetCameraPtr();

                // The camera's position and rotation depend on its transform component
                mainCam->SetPosition(transform.GetTranslation());
                mainCam->SetRotation(transform.GetRotation());
                break;
            }
        }

        if (sceneHasMainCam) {
            ScenePrepareData prepareData{};
            prepareData.RuntimeCamera = &(*mainCam);

            Renderer::BeginScene(prepareData);

            auto view{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };

            for (auto& sceneObject: view) {
                TagComponent& tag{ view.get<TagComponent>(sceneObject) };
                TransformComponent& transform{ view.get<TransformComponent>(sceneObject) };
                RenderComponent& renderComponent{ view.get<RenderComponent>(sceneObject) };
                MaterialComponent& material{ view.get<MaterialComponent>(sceneObject) };

                GameObject& objectData{renderComponent.GetObjectData() };

                Renderer::Submit(std::to_string(tag.GetGUID()), objectData, transform.GetTransform(), material.GetMaterialInfo().MeshMat);
            }

            Renderer::Flush();
            Renderer::EndScene();
        }
    }

    auto Scene::CreateEmptyObject(std::string_view tagName, Entity *root, UInt64_T guid) -> Entity {
        Entity newEntity{m_Registry.create(), m_Registry };
        newEntity.AddComponent<TagComponent>(tagName, guid);
        newEntity.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

        // if root != nullptr, this entity must be children of the node that has
        // root as a root entity; a first step is to find the given node
        // We start the lookup from the very first node from the hierarchy
        EntityNode* rootNode{ nullptr };
        if (root && !m_Hierarchy.empty()) {
            for (auto& node : m_Hierarchy) {
                rootNode = FindNode(node, *root);

                if (rootNode) {
                    break;
                }
            }

            if (rootNode) {
                rootNode->Children.emplace_back(EntityNode{
                        .Root{ newEntity.Get() , m_Registry },
                        .Children{}
                });
            }
        } else {
            m_Hierarchy.push_back(EntityNode{
                    .Root{newEntity.Get() , m_Registry },
                    .Children{},
            });
        }

#if !defined(NDEBUG)
        MKT_CORE_LOGGER_INFO("{} as children of {}", tagName, rootNode ?
            rootNode->Root.GetComponent<TagComponent>().GetTag() : tagName);
#endif

        return newEntity;
    }


    auto Scene::CreatePrefab(std::string_view tagName, Model *model, Entity *root, UInt64_T guid) -> Entity {
        Entity child{};

        if (model) {
            EntityNode rootNode{};
            EntityNode* rootNodeLookupResult{ nullptr };
            if (root && !m_Hierarchy.empty()) {
                rootNodeLookupResult = FindNode(m_Hierarchy[0], *root);

                rootNode = {
                        .Root{ rootNodeLookupResult->Root.Get(), *rootNodeLookupResult->Root.m_Registry },
                        .Children{},
                };
            } else {
                // Create an entity for the model. The model serves as a rootEntity entity for all of its children
                // which are separated meshes
                Entity rootEntity{ m_Registry.create(), m_Registry };
                rootEntity.AddComponent<TagComponent>(tagName, guid);
                rootEntity.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

                rootNode = {
                        .Root{ rootEntity.Get(), m_Registry },
                        .Children{},
                };
            }

            // The default behavior is to treat each mesh as an individual game object
            for (auto& mesh : model->GetMeshes()) {
                child = { m_Registry.create(), m_Registry };

                // Setup tag and transform
                child.AddComponent<TagComponent>(mesh.GetName(), GenerateGUID());
                child.AddComponent<TransformComponent>(ENTITY_INITIAL_POSITION, ENTITY_INITIAL_SIZE, ENTITY_INITIAL_ROTATION);

                // Setup material component
                auto& material{ child.AddComponent<MaterialComponent>() };

                // Setup renderer component
                auto& renderData{ child.AddComponent<RenderComponent>() };

                DefaultMaterialCreateSpec spec{};

                for (auto& textureIt: mesh.GetTextures()) {
                    switch ( (textureIt)->GetType() ) {
                        case MapType::TEXTURE_2D_DIFFUSE:
                            spec.DiffuseMap = textureIt;
                            break;
                        case MapType::TEXTURE_2D_SPECULAR:
                            spec.SpecularMap = textureIt;
                            break;

                        case MapType::TEXTURE_2D_INVALID:
                        case MapType::TEXTURE_2D_EMISSIVE:
                        case MapType::TEXTURE_2D_NORMAL:
                        case MapType::TEXTURE_2D_ROUGHNESS:
                        case MapType::TEXTURE_2D_METALLIC:
                        case MapType::TEXTURE_2D_AMBIENT_OCCLUSION:
                        case MapType::TEXTURE_2D_COUNT:
                            MKT_APP_LOGGER_INFO("Unused type");
                    }
                }

                renderData.GetObjectData().MeshData.Data = std::addressof(mesh);
                material.GetMaterialInfo().MeshMat = Material::CreateStandardMaterial(spec);

                rootNode.Children.emplace_back( EntityNode{
                    .Root { child.Get(), m_Registry },
                    .Children{},
                });
            }

            m_Hierarchy.emplace_back(std::move(rootNode));
        }

        return child;
    }


    auto Scene::FindNode(EntityNode &root, Entity &target) -> EntityNode* {
        if (root.Root == target) {
            return std::addressof(root);
        }

        for (auto& node : root.Children) {
            auto result{ FindNode(node, target) };

            if (result) {
                return result;
            }
        }

        return nullptr;
    }

    auto Scene::HierarchyLookup(Entity& target) -> EntityNode* {
        EntityNode* root{};
        for (auto& node : m_Hierarchy) {
            root = FindNode(node, target);

            if (root){
                break;
            }
        }

        return root;
    }

    auto Scene::FindNodeIterator(std::vector<EntityNode>::iterator root, Entity &target) -> std::vector<EntityNode>::iterator {
        // TODO: vector iterator incompatibility

        if (root != m_Hierarchy.end() && root->Root == target) {
            return root;
        }

        for (auto node{ root->Children.begin() }; node != root->Children.end(); ++node) {
            auto result{ FindNodeIterator(node, target) };

            if (result != root->Children.end()) {
                return result;
            }
        }

        return m_Hierarchy.end();
    }


    auto Scene::DestroyEntity(Entity& entity) -> bool {
        EntityNode* root{ HierarchyLookup(entity) };

        if (root) {
            DestroyRecursive(*root);

            // HOTFIX: Temporary, delete the root node. Not the best container for this kind of operation
            auto it{ FindNodeIterator(m_Hierarchy.begin(), entity) };
            if (it != m_Hierarchy.end()) {
                m_Hierarchy.erase(it);
            }
        }
#if !defined(NDEBUG)
        else {
            MKT_CORE_LOGGER_WARN("Did not find root entity to start deletion from");
        }
#endif
        return true;
    }

    auto Scene::DestroyRecursive( EntityNode& root ) -> void {
        auto node{ root.Children.begin() };

        while (node != root.Children.end()) {
            DestroyRecursive(*node);
            node = root.Children.erase(node);

            // Not increment past the end iterator
            if (node != root.Children.end()) {
                ++node;
            }
        }

        m_Registry.destroy(root.Root.Get());
        root.Root.Invalidate();
    }

    auto Scene::OnViewPortResize(UInt32_T width, UInt32_T height) -> void {
        // Resize non-fixed aspect ratio cameras
        auto view{ m_Registry.view<TransformComponent, CameraComponent>() };

        for (const auto& entity : view) {
            TransformComponent& transform{ view.get<TransformComponent>(entity) };
            CameraComponent& camera{ view.get<CameraComponent>(entity) };

            if (!camera.IsAspectRatioFixed())
                camera.GetCameraPtr()->SetViewportSize(width, height);

        }
    }

    auto Scene::OnEditorUpdate(MKT_UNUSED_VAR double timeStep, const EditorCamera& camera) -> void {
        // Prepare scene data
        ScenePrepareData prepareData{};
        prepareData.StaticCamera = std::addressof(camera);

        // Begin the scene
        Renderer::BeginScene( prepareData );

        // Setup lighting data
        Size_T dirIndexCount{}, spotIndexCount{}, pointIndexCount{};

        Renderer::SetLightsViewPos( glm::vec4{ camera.GetPosition(), 1.0f } );

        for ( auto& lightSource: m_Registry.view<LightComponent>() ) {
            LightComponent& lightComponent{ m_Registry.get<LightComponent>( lightSource ) };

            switch ( lightComponent.GetType() ) {
                case LightType::DIRECTIONAL_LIGHT_TYPE:
                    lightComponent.GetDirLightData().Position =
                            glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
                    Renderer::SetDirLightInfo( lightComponent.GetDirLightData(), dirIndexCount++ );
                    break;
                case LightType::POINT_LIGHT_TYPE:
                    lightComponent.GetPointLightData().Position =
                            glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
                    Renderer::SetPointLightInfo( lightComponent.GetPointLightData(), pointIndexCount++ );
                    break;
                case LightType::SPOT_LIGHT_TYPE:
                    lightComponent.GetSpotLightData().Position =
                            glm::vec4{ m_Registry.get<TransformComponent>( lightSource ).GetTranslation(), 1.0f };
                    Renderer::SetSpotLightInfo( lightComponent.GetSpotLightData(), spotIndexCount++ );
                    break;
            }

            Renderer::SetActivePointLightsCount( pointIndexCount );
            Renderer::SetActiveDirLightsCount( dirIndexCount );
            Renderer::SetActiveSpotLightsCount( spotIndexCount );
        }

        auto view{ m_Registry.view<TagComponent, TransformComponent, RenderComponent, MaterialComponent>() };
        for ( auto& sceneObject: view ) {
            TagComponent& tag{ view.get<TagComponent>( sceneObject ) };
            TransformComponent& transform{ view.get<TransformComponent>( sceneObject ) };
            RenderComponent& renderComponent{ view.get<RenderComponent>( sceneObject ) };
            MaterialComponent& material{ view.get<MaterialComponent>( sceneObject ) };

            GameObject& objectData{ renderComponent.GetObjectData() };

            if ( tag.IsVisible() && objectData.MeshData.Data != nullptr ) {
                Renderer::Submit(std::to_string(tag.GetGUID()), objectData, transform.GetTransform(), material.GetMaterialInfo().MeshMat);
            }
        }

        Renderer::EndScene();
        Renderer::Flush();
    }

    auto Scene::UpdateScripts() -> void {
        auto view{ m_Registry.view<TagComponent, TransformComponent, NativeScriptComponent>() };

        for (const auto& entity : view) {
            TagComponent& tag{ view.get<TagComponent>(entity) };
            TransformComponent& transform{ view.get<TransformComponent>(entity) };
            NativeScriptComponent& script{ view.get<NativeScriptComponent>(entity) };

            // Update scripts
            {

            }

        }
    }

    auto Scene::Clear() -> void {
        auto view{ m_Registry.view<TagComponent>() };
        m_Registry.destroy(view.begin(), view.end());
    }

    auto Scene::GetActiveScene() -> Scene* {
        return s_ActiveScene;
    }

    auto Scene::SetActiveScene(Scene* scene) -> void {
        s_ActiveScene = scene;
    }

    auto Scene::FetchSceneMetaData() -> SceneMetaData& {
        m_MetaData.EntityCount = m_Registry.view<TagComponent>().size();

        auto viewLights{ m_Registry.view<LightComponent>() };
        m_MetaData.LightsCount = viewLights.size();

        for (auto& light : viewLights) {
            LightComponent& lightComponent{ m_Registry.get<LightComponent>(light) };

            // Directional light count
            if (lightComponent.GetType() == LightType::DIRECTIONAL_LIGHT_TYPE) {
                m_MetaData.DirLightsCount += 1;
                m_MetaData.DirActiveLightCount += lightComponent.IsActive() ? 1 : 0;
            }

            // Point light count
            if (lightComponent.GetType() == LightType::POINT_LIGHT_TYPE) {
                m_MetaData.PointLightsCount += 1;
                m_MetaData.PointActiveLightCount += lightComponent.IsActive() ? 1 : 0;
            }

            // Spot-light count
            if (lightComponent.GetType() == LightType::SPOT_LIGHT_TYPE) {
                m_MetaData.SpotLightsCount += 1;
                m_MetaData.SpotActiveLightCount += lightComponent.IsActive() ? 1 : 0;
            }
        }

        m_MetaData.ActiveLightCount = m_MetaData.DirActiveLightCount + m_MetaData.PointActiveLightCount + m_MetaData.SpotActiveLightCount;

        return m_MetaData;
    }
}