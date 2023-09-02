/**
 * HierarchyPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <Core/Logger.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Editor/Panels/HierarchyPanel.hh>

namespace Mikoto {
    auto HierarchyPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin("Hierarchy");
            if (auto ptr{ m_Context.lock() }) {
                auto view{ ptr->m_Registry.view<TagComponent>() };

                for (const auto& entity : view) {
                    Entity current{ entity, ptr };
                    DrawEntityNode(current);
                    EntityPopupMenu(current);
                }

                if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
                    m_ContextSelection.Invalidate();

                BlankSpacePopupMenu();
            }
            else
                MKT_CORE_LOGGER_ERROR("Panel context has expired and no longer exists!");
            ImGui::End();
        }
    }

    auto HierarchyPanel::DrawEntityNode(Entity& target) -> void {
        TagComponent& tag{ target.GetComponent<TagComponent>() };
        bool thisEntityIsSelected{ target == m_ContextSelection };
        ImGuiTreeNodeFlags flags{  (thisEntityIsSelected ? ImGuiTreeNodeFlags_Selected : 0) |
                                 ImGuiTreeNodeFlags_OpenOnArrow |  ImGuiTreeNodeFlags_SpanAvailWidth };
        ImGuiTreeNodeFlags childNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen };

        bool expanded{ ImGui::TreeNodeEx((void*)(target.m_EntityHandle), flags, "%s", tag.GetTag().c_str()) };
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            m_ContextSelection = target;

        if (expanded) {
            // Temporary just to test nested nodes
            if (ImGui::TreeNodeEx((void*)83124423, childNodeFlags, "%s", "Entity item")) {

                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

    }

    auto HierarchyPanel::EntityPopupMenu(Entity& target) -> void {
        std::shared_ptr<Scene> m_ContextSelectionScene{ m_ContextSelection.m_Scene.lock() };

        if (m_ContextSelectionScene) {
            ImGuiPopupFlags popupItemFlags{ ImGuiPopupFlags_MouseButtonRight };
            if (ImGui::BeginPopupContextItem(nullptr, popupItemFlags)) {
                if (ImGui::BeginMenu("Add component")) {
                    constexpr bool menuItemSelected{ false };    // these menu items should not display as selected
                    constexpr const char* menuItemShortcut{ nullptr }; // no shortcuts for now

                    // NOTE: Menu item will remain disabled if the entity already has a specific component preventing from reapplying it

                    if (ImGui::MenuItem("Sprite", menuItemShortcut, menuItemSelected, !target.HasComponent<SpriteRendererComponent>())) {
                        target.AddComponent<SpriteRendererComponent>();
                        ImGui::CloseCurrentPopup();
                    }

                    if (ImGui::MenuItem("Material", menuItemShortcut, menuItemSelected, !target.HasComponent<MaterialComponent>())) {
                        target.AddComponent<MaterialComponent>();
                        ImGui::CloseCurrentPopup();
                    }

                    if (ImGui::MenuItem("Camera", menuItemShortcut, menuItemSelected, !target.HasComponent<CameraComponent>())) {
                        target.AddComponent<CameraComponent>(std::make_shared<SceneCamera>());
                        ImGui::CloseCurrentPopup();
                    }

                    if (ImGui::MenuItem("Script", menuItemShortcut, menuItemSelected, !target.HasComponent<NativeScriptComponent>())) {
                        target.AddComponent<NativeScriptComponent>();
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }

                if (ImGui::MenuItem("Remove object")) {
                    m_ContextSelectionScene->DestroyEntity(target);
                    m_ContextSelection.Invalidate();
                }

                ImGui::EndPopup();
            }
        }
    }

    auto HierarchyPanel::BlankSpacePopupMenu() -> void {
        if (auto ptr{ m_Context.lock() }) {
            ImGuiPopupFlags popupWindowFlags{ ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight };
            if (ImGui::BeginPopupContextWindow("##HierarchyMenuOptions", popupWindowFlags)) {
                if (ImGui::MenuItem("Empty Object")) {
                    static Size_T emptyObjectCounter{ 0 };
                    auto newEntity{ Scene::CreateEmptyObject(fmt::format("Object {}", emptyObjectCounter), ptr) };
                    ++emptyObjectCounter;
                }

                if (ImGui::BeginMenu("3D Object")) {
                    if (ImGui::MenuItem("Cube")) {
                        static Size_T cubeCounter{ 0 };
                        auto ret{ Scene::CreatePrefabObject(fmt::format("Cube {}", cubeCounter), ptr, PrefabSceneObject::CUBE_PREFAB_OBJECT)};
                        MKT_CORE_LOGGER_INFO("Added cube to scene");
                        ++cubeCounter;
                    }

                    if (ImGui::MenuItem("Sprite")) {
                        static Size_T spriteCounter{ 0 };
                        auto ret{ Scene::CreatePrefabObject(fmt::format("Sprite {}", spriteCounter), ptr, PrefabSceneObject::SPRITE_PREFAB_OBJECT) };
                        MKT_CORE_LOGGER_INFO("Added Sprite to scene");
                        ++spriteCounter;
                    }

                    if (ImGui::MenuItem("Cone")) {
                        static Size_T coneCounter{ 0 };
                        auto ret{ Scene::CreatePrefabObject(fmt::format("Cone {}", coneCounter), ptr, PrefabSceneObject::CONE_PREFAB_OBJECT)};
                        MKT_CORE_LOGGER_INFO("Added Cone to scene");
                        ++coneCounter;
                    }

                    if (ImGui::MenuItem("Cylinder")) {
                        static Size_T cylinderCounter{ 0 };
                        auto ret{ Scene::CreatePrefabObject(fmt::format("Cylinder {}", cylinderCounter), ptr, PrefabSceneObject::CYLINDER_PREFAB_OBJECT)};
                        MKT_CORE_LOGGER_INFO("Added Cylinder to scene");
                        ++cylinderCounter;
                    }

                    if (ImGui::MenuItem("Sphere")) {
                        static Size_T sphereCounter{ 0 };
                        auto ret{ Scene::CreatePrefabObject(fmt::format("Sphere {}", sphereCounter), ptr, PrefabSceneObject::SPHERE_PREFAB_OBJECT)};
                        MKT_CORE_LOGGER_INFO("Added Sphere to scene");
                        ++sphereCounter;
                    }

                    if (ImGui::MenuItem("Sponza")) {
                        static Size_T sponzaCounter{ 0 };
                        auto ret{ Scene::CreatePrefabObject(fmt::format("Sponza {}", sponzaCounter), ptr, PrefabSceneObject::SPONZA_PREFAB_OBJECT)};
                        MKT_CORE_LOGGER_INFO("Added Sponza to scene");
                        ++sponzaCounter;
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndPopup();
            }
        }
    }

    auto HierarchyPanel::OnEvent(Event& event) -> void {

    }

    HierarchyPanel::HierarchyPanel(const std::shared_ptr<Scene>& scene, const Path_T &iconPath)
        :   Panel{ iconPath }
    {
        m_PanelIsHovered = true;
        m_PanelIsFocused = false;
        m_PanelIsVisible = false;

        SetScene(scene);
    }

    auto HierarchyPanel::SetScene(const std::shared_ptr<Scene>& scene) -> void {
        m_Context = scene;
    }
}