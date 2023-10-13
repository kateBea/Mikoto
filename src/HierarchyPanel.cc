/**
 * HierarchyPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <Utility/StringUtils.hh>

#include <Core/Logger.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>
#include <Scene/SceneManager.hh>
#include <Editor/HierarchyPanel.hh>

#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

namespace Mikoto {
    static constexpr auto GetHierarchyName() -> std::string_view {
        return "Hierarchy";
    }

    HierarchyPanel::HierarchyPanel()
        :   Panel{}
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_MERGE, GetHierarchyName());
    }

    auto HierarchyPanel::OnUpdate(float ts) -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible));

            m_PanelIsHovered = ImGui::IsWindowHovered();
            m_PanelIsFocused = ImGui::IsWindowFocused();

            // table with entity label, entity type and visibility

            // each entity (and its nodes) make a row
            SceneManager::ForEachWithComponents<TagComponent>(
                    [](Entity& entity) -> void {
                        DrawEntityNode(entity);
                        OnEntityRightClickMenu(entity);
                    });

            // Deselect the game object
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
                SceneManager::DisableCurrentlyActiveEntity();
            }

            // Right click on blank space
            BlankSpacePopupMenu();

            ImGui::End();
        }
    }

    auto HierarchyPanel::DrawEntityNode(Entity& target) -> void {
        TagComponent& tag{ target.GetComponent<TagComponent>() };

        bool thisEntityIsSelected{ target == SceneManager::GetCurrentlySelectedEntity() };
        static constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_AllowItemOverlap |
                                         ImGuiTreeNodeFlags_Framed |
                                         ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_FramePadding };

        static const ImGuiTreeNodeFlags flags{  styleFlags | (thisEntityIsSelected ? ImGuiTreeNodeFlags_Selected : 0) };
        static constexpr ImGuiTreeNodeFlags childNodeFlags{ styleFlags | ImGuiTreeNodeFlags_DefaultOpen };

        bool expanded{ ImGui::TreeNodeEx((void*)(target.m_EntityHandle), flags, "%s", fmt::format(" {} {}", ICON_MD_WIDGETS, tag.GetTag()).c_str()) };

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            SceneManager::SetCurrentlyActiveEntity(target);
        }

        if (expanded) {
            // Temporary just to test nested nodes
            if (ImGui::TreeNodeEx((void*)83124423, childNodeFlags, "%s", "Entity item")) {

                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

    }

    auto HierarchyPanel::OnEntityRightClickMenu(Entity& target) -> void {
        static constexpr ImGuiPopupFlags popupItemFlags{ ImGuiPopupFlags_MouseButtonRight };

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
                SceneManager::DestroyEntityFromCurrentlyActiveScene(target);
            }

            ImGui::EndPopup();
        }
    }

    auto HierarchyPanel::BlankSpacePopupMenu() -> void {
        static constexpr ImGuiPopupFlags popupWindowFlags{ ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight };

        if (ImGui::BeginPopupContextWindow("##HierarchyMenuOptions", popupWindowFlags)) {
            EntityCreateInfo entityCreateInfo{};

            if (ImGui::MenuItem("Empty Object")) {
                static Size_T emptyObjectCounter{ 0 };
                entityCreateInfo.Name = fmt::format("Empty Object {}", emptyObjectCounter);
                entityCreateInfo.IsPrefab = false;
                entityCreateInfo.PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;
                SceneManager::AddEntity(entityCreateInfo);

                MKT_CORE_LOGGER_INFO("Added new empty object");
                ++emptyObjectCounter;
            }

            if (ImGui::BeginMenu("3D Object")) {
                if (ImGui::MenuItem("Cube")) {
                    static Size_T cubeCounter{ 0 };
                    entityCreateInfo.Name = fmt::format("Cube Object {}", cubeCounter);
                    entityCreateInfo.IsPrefab = true;
                    entityCreateInfo.PrefabType = PrefabSceneObject::CUBE_PREFAB_OBJECT;
                    SceneManager::AddEntity(entityCreateInfo);

                    MKT_CORE_LOGGER_INFO("Added new cube prefab");
                    ++cubeCounter;
                }

                if (ImGui::MenuItem("Sprite")) {
                    static Size_T spriteCounter{ 0 };
                    entityCreateInfo.Name = fmt::format("Sprite Object {}", spriteCounter);
                    entityCreateInfo.IsPrefab = true;
                    entityCreateInfo.PrefabType = PrefabSceneObject::SPRITE_PREFAB_OBJECT;
                    SceneManager::AddEntity(entityCreateInfo);

                    MKT_CORE_LOGGER_INFO("Added new sprite prefab");
                    ++spriteCounter;
                }

                if (ImGui::MenuItem("Cone")) {
                    static Size_T coneCounter{ 0 };
                    entityCreateInfo.Name = fmt::format("Cone Object {}", coneCounter);
                    entityCreateInfo.IsPrefab = true;
                    entityCreateInfo.PrefabType = PrefabSceneObject::CONE_PREFAB_OBJECT;
                    SceneManager::AddEntity(entityCreateInfo);

                    MKT_CORE_LOGGER_INFO("Added new cone prefab");
                    ++coneCounter;
                }

                if (ImGui::MenuItem("Cylinder")) {
                    static Size_T cylinderCounter{ 0 };
                    entityCreateInfo.Name = fmt::format("Cylinder Object {}", cylinderCounter);
                    entityCreateInfo.IsPrefab = true;
                    entityCreateInfo.PrefabType = PrefabSceneObject::CYLINDER_PREFAB_OBJECT;
                    SceneManager::AddEntity(entityCreateInfo);

                    MKT_CORE_LOGGER_INFO("Added new cylinder prefab");
                    ++cylinderCounter;
                }

                if (ImGui::MenuItem("Sphere")) {
                    static Size_T sphereCounter{ 0 };
                    entityCreateInfo.Name = fmt::format("Sphere Object {}", sphereCounter);
                    entityCreateInfo.IsPrefab = true;
                    entityCreateInfo.PrefabType = PrefabSceneObject::SPHERE_PREFAB_OBJECT;
                    SceneManager::AddEntity(entityCreateInfo);

                    MKT_CORE_LOGGER_INFO("Added new sphere prefab");
                    ++sphereCounter;
                }

                if (ImGui::MenuItem("Sponza")) {
                    static Size_T sponzaCounter{ 0 };
                    entityCreateInfo.Name = fmt::format("Sponza Object {}", sponzaCounter);
                    entityCreateInfo.IsPrefab = true;
                    entityCreateInfo.PrefabType = PrefabSceneObject::SPONZA_PREFAB_OBJECT;
                    SceneManager::AddEntity(entityCreateInfo);

                    MKT_CORE_LOGGER_INFO("Added new sponza prefab");
                    ++sponzaCounter;
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    }
}