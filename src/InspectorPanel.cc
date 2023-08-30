/**
 * InspectorPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <array>
#include <utility>
#include <iterator>
#include <algorithm>

// Third-Party Libraries
#include <entt/entt.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_internal.h>

// Project Headers
#include <Core/Logger.hh>
#include <Scene/Component.hh>
#include <Editor/Panels/InspectorPanel.hh>

namespace Mikoto {
    /**
     * Adds a button to insert components to an entity. This button is only added
     * if the parameter is a valid entity, does nothing otherwise
     * @param entity entity to adds components on
     * */
    static auto AddComponentButton(Entity& entity) {
        if (entity.IsValid()) {
            ImGui::SameLine();
            ImGui::PushItemWidth(-1.0f);

            if (ImGui::Button("Add component"))
                ImGui::OpenPopup("AddComponentButtonPopup");

            if (ImGui::BeginPopup("AddComponentButtonPopup")) {
                const bool menuItemSelected{ false };
                const char* menuItemShortcut{ nullptr }; // no shortcuts for now

                // NOTE: MenuItem will remain disabled if the entity already has a specific component preventing from reapplying it

                if (ImGui::MenuItem("Sprite", menuItemShortcut, menuItemSelected, !entity.HasComponent<SpriteRendererComponent>())) {
                    entity.AddComponent<SpriteRendererComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::MenuItem("Material", menuItemShortcut, menuItemSelected, !entity.HasComponent<MaterialComponent>())) {
                    entity.AddComponent<MaterialComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::MenuItem("Camera", menuItemShortcut, menuItemSelected, !entity.HasComponent<CameraComponent>())) {
                    entity.AddComponent<CameraComponent>(std::make_shared<SceneCamera>());
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::MenuItem("Script", menuItemShortcut, menuItemSelected, !entity.HasComponent<NativeScriptComponent>())) {
                    entity.AddComponent<NativeScriptComponent>();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            ImGui::PopItemWidth();
        }
    }

    InspectorPanel::InspectorPanel(const std::shared_ptr<HierarchyPanel> &hierarchy, const Path_T& iconPath)
        :   Panel{ iconPath }, m_Hierarchy{ hierarchy }
    {
        m_PanelIsVisible = true;
        m_PanelIsHovered = false;
        m_PanelIsFocused = false;
    }

    static auto DrawVec3Transform(std::string_view label, glm::vec3& data, double resetValue = 0.0 , double columWidth = 100.0) {
        ImGuiIO& io = ImGui::GetIO();

        // See ImGuiLayer for font loading order
        auto boldFont{ io.Fonts->Fonts[0] };

        // Group is part of a unique label
        ImGui::PushID(label.data());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columWidth);
        ImGui::Text("%s", label.data());
        ImGui::NextColumn();
        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 7.0f, 5.0f });

        float lineHeight{ GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f };
        ImVec2 buttonSize{ lineHeight + 3.0f, lineHeight };

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.1f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.1f, 1.0f });
        ImGui::PushFont(boldFont);

        if (ImGui::Button("X", buttonSize))
            data.x = (float)resetValue;

        ImGui::PopFont();

        ImGui::SameLine();
        ImGui::DragFloat("##X", &data.x, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.1f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.25f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.1f, 1.0f });
        ImGui::PushFont(boldFont);

        if (ImGui::Button("Y", buttonSize))
            data.y = (float)resetValue;

        ImGui::PopFont();

        ImGui::SameLine();
        ImGui::DragFloat("##Y", &data.y, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.25f, 0.3f, 0.9f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        ImGui::PushFont(boldFont);

        if (ImGui::Button("Z", buttonSize))
            data.z = (float)resetValue;

        ImGui::PopFont();

        ImGui::SameLine();
        ImGui::DragFloat("##Z", &data.z, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopStyleColor(3);
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::Columns(1);

        ImGui::PopID();
    }

    template<typename ComponentType, typename UIFunction>
    static auto DrawComponent(std::string_view componentLabel, Entity& entity, UIFunction uiFunc, bool hasRemoveButton = true) {
        ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                         ImGuiTreeNodeFlags_AllowItemOverlap |
                                         ImGuiTreeNodeFlags_Framed |
                                         ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_FramePadding };

        if (entity.HasComponent<ComponentType>()) {
            bool removeComponent{ false };
            ImVec2 contentRegionAvailable{ ImGui::GetContentRegionAvail() };

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f });

            // See ImGui implementation for button dimensions computation
            float lineHeight{ GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f };
            ImGui::Separator();

            bool componentNodeOpen{ ImGui::TreeNodeEx((void*) typeid(ComponentType).hash_code(), treeNodeFlags, "%s", componentLabel.data()) };

            ImGui::PopStyleVar();
            if (hasRemoveButton) {
                ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
                if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight }))
                    ImGui::OpenPopup("ComponentSettingsButton");

                if (ImGui::BeginPopup("ComponentSettingsButton")) {
                    if (ImGui::MenuItem("Remove Component")) {
                        removeComponent = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }

            if (componentNodeOpen) {
                ComponentType& component{ entity.GetComponent<ComponentType>() };
                uiFunc(component);
                ImGui::TreePop();
            }

            if (removeComponent)
                entity.RemoveComponent<ComponentType>();
        }
    }

    auto InspectorPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin("Inspector");

            // Check to see if the scene is still alive
            if (auto ptr{ m_Hierarchy->m_Context.lock() })
                m_Hierarchy->m_ContextSelection.SetContext(ptr);

            if (m_Hierarchy->m_ContextSelection.HasComponent<TagComponent>()) {
                TagComponent& tag{ m_Hierarchy->m_ContextSelection.GetComponent<TagComponent>() };
                static bool renderContextSelectionToScene{ true }; // tells whether we want the selection context to be rendered to the scene (not implemented yet)

                std::string value{ tag.GetTag() };
                char contextSelectionTagName[1024]{};
                std::copy(tag.GetTag().begin(), tag.GetTag().end(), contextSelectionTagName);

                ImGui::Checkbox("##show", &renderContextSelectionToScene);
                ImGui::SameLine();

                if (ImGui::InputText("##Tag", contextSelectionTagName, std::size(contextSelectionTagName))) {
                    tag.SetTag(contextSelectionTagName);
                }

            }

            AddComponentButton(m_Hierarchy->m_ContextSelection);

            // By default, all scene objects have a transform component which cannot be removed
            constexpr bool TRANSFORM_HAS_PLUS_BUTTON{ false };

            DrawComponent<TransformComponent>("Transform", m_Hierarchy->m_ContextSelection,
                [](auto& component) -> void {
                    auto translation{ component.GetTranslation() };
                    auto rotation{ component.GetRotation() };
                    auto scale{ component.GetScale() };

                    DrawVec3Transform("Translation", translation);
                    DrawVec3Transform("Rotation", rotation);
                    DrawVec3Transform("Scale", scale, 1.0);

                    component.SetTranslation(translation);
                    component.SetRotation(rotation);
                    component.SetScale(scale);
                },
                    TRANSFORM_HAS_PLUS_BUTTON);


            DrawComponent<CameraComponent>("Camera", m_Hierarchy->m_ContextSelection,
                [](auto& component) -> void {
                    const std::array<std::string, 2> cameraProjectionTypes{ "Orthographic", "Perspective" };
                    auto cameraCurrentProjectionType{ component.GetCameraPtr()->GetProjectionType() };
                    auto currentProjectionTypeStr{ cameraProjectionTypes[cameraCurrentProjectionType] };

                    if (ImGui::BeginCombo("Projection", currentProjectionTypeStr.c_str())) {
                        UInt32_T projectionIndex{};
                        for (const auto& projectionType : cameraProjectionTypes) {
                            bool isSelected{ projectionType == cameraProjectionTypes[cameraCurrentProjectionType] };

                            if (ImGui::Selectable(projectionType.c_str(), isSelected)) {
                                currentProjectionTypeStr = projectionType;
                                component.GetCameraPtr()->SetProjectionType((SceneCamera::ProjectionType) projectionIndex);
                            }

                            if (isSelected)
                                ImGui::SetItemDefaultFocus();

                            ++projectionIndex;
                        }

                        ImGui::EndCombo();
                    }

                    if (component.GetCameraPtr()->GetProjectionType() == SceneCamera::ProjectionType::ORTHOGRAPHIC) {
                        float size{ (float)component.GetCameraPtr()->GetOrthographicSize() };
                        if (ImGui::SliderFloat("Orthographic Size", &size, 2.0f, 10.0f))
                            component.GetCameraPtr()->SetOrthographicSize(size);

                        float nearPlane{ (float)component.GetCameraPtr()->GetOrthographicNearPlane() };
                        if (ImGui::SliderFloat("Orthographic Near", &nearPlane, -5.0, -1.0))
                            component.GetCameraPtr()->SetOrthographicNearPlane(nearPlane);

                        float farPlane{ (float)component.GetCameraPtr()->GetOrthographicFarPlane() };
                        if (ImGui::SliderFloat("Orthographic Far", &farPlane, 1.0, 5.0))
                            component.GetCameraPtr()->SetOrthographicFarPlane(farPlane);

                        component.GetCameraPtr()->SetOrthographic(nearPlane, farPlane, size);
                    }

                    if (component.GetCameraPtr()->GetProjectionType() == SceneCamera::ProjectionType::PERSPECTIVE) {
                        float fov{ (float)component.GetCameraPtr()->GetPerspectiveFOV() };
                        if (ImGui::SliderFloat("Perspective FOV", &fov, 45.0f, 90.0f))
                            component.GetCameraPtr()->SetPerspectiveFOV(fov);

                        float nearPlane{ (float)component.GetCameraPtr()->GetPerspectiveNearPlane() };
                        if (ImGui::SliderFloat("Perspective Near", &nearPlane, 0.001f, 1.0))
                            component.GetCameraPtr()->SetPerspectiveNearPlane(nearPlane);

                        float farPlane{ (float)component.GetCameraPtr()->GetPerspectiveFarPlane() };
                        if (ImGui::SliderFloat("Perspective Far", &farPlane, 100.0f, 10000.0f))
                            component.GetCameraPtr()->SetPerspectiveFarPlane(farPlane);

                        component.GetCameraPtr()->SetPerspective(nearPlane, farPlane, fov);
                    }
                }
            );

            DrawComponent<SpriteRendererComponent>("Sprite", m_Hierarchy->m_ContextSelection, [](auto& component) -> void {
                glm::vec4 color{ component.GetColor() };
                ImGui::ColorEdit4("Color", glm::value_ptr(color), ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar);
                component.SetColor(color);
            });

            DrawComponent<MaterialComponent>("Material", m_Hierarchy->m_ContextSelection, [](auto& component) -> void {
                ImGuiTreeNodeFlags flags{  ImGuiTreeNodeFlags_OpenOnArrow |  ImGuiTreeNodeFlags_SpanAvailWidth };

                // TODO: temporary, need proper identifiers, no 83121231231
                if (ImGui::TreeNodeEx((void*)83121231231, flags, "%s", "Diffuse texture")) {

                    ImGui::TreePop();
                }
            });

            DrawComponent<NativeScriptComponent>("Script", m_Hierarchy->m_ContextSelection, [](auto& component) -> void {
                // Currently empty
            });

            ImGui::End();
        }
    }

    auto InspectorPanel::OnEvent(Event& event) -> void {

    }

    auto InspectorPanel::MakeVisible(bool value) -> void {
        m_PanelIsVisible = value;
    }
}