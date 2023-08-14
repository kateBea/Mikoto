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
        if (m_Visible) {
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
            else KATE_CORE_LOGGER_ERROR("Panel context has expired and no longer exists!");
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
                    const bool menuItemSelected{ false };
                    const char* menuItemShortcut{ nullptr }; // no shortcuts for now

                    // NOTE: Menu item will remain disabled if the entity already has a specific component preventing from reapplying it

                    if (ImGui::MenuItem("Sprite", menuItemShortcut, menuItemSelected, !target.HasComponent<SpriteRendererComponent>())) {
                        target.AddComponent<SpriteRendererComponent>();
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

                if (ImGui::BeginMenu("Options")) {
                    if (ImGui::MenuItem("Destroy entity")) {
                        m_ContextSelectionScene->DestroyEntity(target);
                        m_ContextSelection.Invalidate();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }
        }
    }

    auto HierarchyPanel::BlankSpacePopupMenu() -> void {
        if (auto ptr{ m_Context.lock() }) {
            ImGuiPopupFlags popupWindowFlags{ ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight };
            if (ImGui::BeginPopupContextWindow("##HierarchyMenuOptions", popupWindowFlags)) {
                if (ImGui::BeginMenu("New")) {
                    if (ImGui::MenuItem("Create entity")) {
                        auto newEntity{ Scene::CreateEntity("Item", ptr) };
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
        :   Panel{ iconPath }, m_Visible{ true }, m_Hovered{ false }, m_Focused{ false }
    {
        SetScene(scene);
    }

    auto HierarchyPanel::SetScene(const std::shared_ptr<Scene>& scene) -> void {
        m_Context = scene;
    }
}