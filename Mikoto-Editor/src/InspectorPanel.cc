/**
 * InspectorPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <array>
#include <iterator>
#include <algorithm>

// Third-Party Libraries
#include "imgui.h"
#include "imgui_internal.h"
#include "glm/gtc/type_ptr.hpp"

// Project Headers
#include "Common/StringUtils.hh"
#include "Common/Types.hh"

#include <Assets/AssetsManager.hh>

#include <Core/Serializer.hh>

#include "Scene/Component.hh"
#include "Scene/SceneManager.hh"

#include "Panels/InspectorPanel.hh"

#include "Renderer/Renderer.hh"

#include "GUI/IconsFontAwesome5.h"
#include "GUI/IconsMaterialDesign.h"
#include "GUI/IconsMaterialDesignIcons.h"
#include "GUI/ImGuiManager.hh"

namespace Mikoto {
    static auto MaterialComponentEditor(MaterialComponent& material) -> void {
        // Albedo material
        ImGui::SeparatorText("Albedo");
        static constexpr Int32_T columnCount{ 2 };
        static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PreciseWidths };

        if (ImGui::BeginTable("MaterialAlbedoTable", columnCount, flags)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Button("Texture\nGoes Here", ImVec2{ 100, 100 });

            ImGui::TableNextColumn();
            static float mixing{};
            static constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };
            glm::vec4 color{ material.GetColor()};
            if (ImGui::ColorEdit4("Color", glm::value_ptr(color), colorEditFlags)) {
                material.SetColor(color);
            }
            ImGui::SliderFloat("Mix", std::addressof(mixing), 0.0f, 1.0f);

            ImGui::EndTable();
        }

        // Specular material
        ImGui::SeparatorText("Specular");
        static float shininess{};
        static glm::vec3 specular{};
        ImGui::ColorEdit3("Channels", glm::value_ptr(specular));
        ImGui::SliderFloat("Shininess", std::addressof(shininess), 0.0f, 1.0f);

        // Roughness material
        ImGui::SeparatorText("Roughness");
        static float smoothness{};
        ImGui::SliderFloat("Smoothness", std::addressof(smoothness), 0.0f, 1.0f);

        // Metallic material
        ImGui::SeparatorText("Metalness");
        static float reflection{};
        ImGui::SliderFloat("Reflection", std::addressof(reflection), 0.0f, 1.0f);
    }

    /**
     * Adds a button to insert components to an entity. This button is only added
     * if the parameter is a valid entity, does nothing otherwise
     * @param entity entity to adds components on
     * */
    static auto AddComponentButtonFor(Entity& entity) {
        if (entity.IsValid()) {
            ImGui::SameLine();
            ImGui::PushItemWidth(-1.0f);

            if (ImGui::Button("Add component"))
                ImGui::OpenPopup("AddComponentButtonPopup");

            if (ImGui::BeginPopup("AddComponentButtonPopup")) {
                const bool menuItemSelected{ false };
                const char* menuItemShortcut{ nullptr }; // no shortcuts for now

                if (ImGui::MenuItem("Material", menuItemShortcut, menuItemSelected, !entity.HasComponent<MaterialComponent>())) {
                    entity.AddComponent<MaterialComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::MenuItem("Script", menuItemShortcut, menuItemSelected, !entity.HasComponent<NativeScriptComponent>())) {
                    entity.AddComponent<NativeScriptComponent>();
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::MenuItem("Mesh", menuItemShortcut, menuItemSelected, !entity.HasComponent<RenderComponent>())) {
                    entity.AddComponent<RenderComponent>();

                    // TODO: Temporary, required to filter renderables, see Scene update
                    entity.AddComponent<MaterialComponent>();
                    auto& materialData{ entity.GetComponent<MaterialComponent>() };

                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::MenuItem("Camera", menuItemShortcut, menuItemSelected, !entity.HasComponent<CameraComponent>())) {
                    entity.AddComponent<CameraComponent>(std::make_shared<SceneCamera>());
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            ImGui::PopItemWidth();
        }
    }

    static constexpr auto GetInspectorPanelName() -> std::string_view {
        return "Inspector";
    }

    InspectorPanel::InspectorPanel()
        :   Panel{}
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_ERROR_OUTLINE, GetInspectorPanelName());

        for (Size_T count{}; count < REQUIRED_IDS; ++count) {
            m_Guids.emplace_back();
        }
    }

    static auto DrawVec3Transform(std::string_view label, glm::vec3& data, double resetValue = 0.0 , double columWidth = 100.0) {

        // See ImGuiManager for font loading order
        auto buttonFont{ ImGuiManager::GetFonts()[1] };

        // Group is part of a unique label
        ImGui::PushID(label.data());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, (float)columWidth);
        ImGui::Text("%s", label.data());
        ImGui::NextColumn();
        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 7.0f, 5.0f });

        const float lineHeight{ GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f };
        const ImVec2 buttonSize{ lineHeight + 3.0f, lineHeight };

        ImGui::PushFont(buttonFont);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.1f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.1f, 1.0f });

        if (ImGui::Button("X", buttonSize)) {
            data.x = (float)resetValue;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }


        ImGui::SameLine();
        ImGui::DragFloat("##X", &data.x, 0.1f, 0.0f, 0.0f, "%.2f");

        ImGui::PopItemWidth();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.1f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.25f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.1f, 1.0f });

        if (ImGui::Button("Y", buttonSize)) {
            data.y = (float)resetValue;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::SameLine();
        ImGui::DragFloat("##Y", &data.y, 0.1f, 0.0f, 0.0f, "%.2f");

        ImGui::PopItemWidth();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.25f, 0.3f, 0.9f, 1.0f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });

        if (ImGui::Button("Z", buttonSize)) {
            data.z = (float)resetValue;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::SameLine();
        ImGui::DragFloat("##Z", &data.z, 0.1f, 0.0f, 0.0f, "%.2f");

        ImGui::PopStyleColor(3);
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::PopFont();

        ImGui::Columns(1);

        ImGui::PopID();
    }

    template<typename ComponentType, typename UIFunction>
    static auto DrawComponent(std::string_view componentLabel, Entity& entity, UIFunction uiFunc, bool hasRemoveButton = true) {
        static constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                         ImGuiTreeNodeFlags_AllowItemOverlap |
                                         ImGuiTreeNodeFlags_Framed |
                                         ImGuiTreeNodeFlags_SpanAvailWidth |
                                         ImGuiTreeNodeFlags_FramePadding };

        if (entity.HasComponent<ComponentType>()) {
            bool removeComponent{ false };
            const ImVec2 contentRegionAvailable{ ImGui::GetContentRegionAvail() };

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f });

            // See ImGui implementation for button dimensions computation
            const float lineHeight{ GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f };

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const bool componentNodeOpen{ ImGui::TreeNodeEx((void*) typeid(ComponentType).hash_code(), treeNodeFlags, "%s", componentLabel.data()) };
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::PopStyleVar();
            if (hasRemoveButton) {
                ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
                if (ImGui::Button(fmt::format("{}", ICON_MD_SETTINGS).c_str(), ImVec2{ lineHeight, lineHeight })) {
                    ImGui::OpenPopup("ComponentSettingsButton");
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

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

            if (removeComponent) {
                entity.RemoveComponent<ComponentType>();
            }
        }
    }

    static auto DrawVisibilityCheckBox(Entity& entity) {
        if (!entity.IsValid()) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity.GetComponent<TagComponent>() };

        bool wantToRenderActiveEntity{ tag.IsVisible() };
        if (ImGui::Checkbox("##show", &wantToRenderActiveEntity)) {
            tag.SetVisibility(!tag.IsVisible());
        }
    }

    static auto DrawNameTextInput(Entity& entity) -> void {
        if (!entity.IsValid()) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity.GetComponent<TagComponent>() };

        // Input text to change target's name
        static constexpr ImGuiTextFlags flags{};
        char contextSelectionTagName[1024]{};
        std::copy(tag.GetTag().begin(), tag.GetTag().end(), contextSelectionTagName);

        if (ImGui::InputText("##Tag", contextSelectionTagName, std::size(contextSelectionTagName), flags)) {
            tag.SetTag(contextSelectionTagName);
        }
    }

    static auto DrawComponents() -> void {
        auto& currentlyActiveEntity{ SceneManager::GetCurrentlySelectedEntity() };

        if (!currentlyActiveEntity.IsValid()) {
            return;
        }

        // By default, all scene objects have a transform component which cannot be removed
        constexpr bool TRANSFORM_HAS_PLUS_BUTTON{ false };
        DrawComponent<TransformComponent>(fmt::format("{} Transform", ICON_MD_DEVICE_HUB), currentlyActiveEntity,
                [](auto& component) -> void {
                    auto translation{ component.GetTranslation() };
                    auto rotation{ component.GetRotation() };
                    auto scale{ component.GetScale() };

                    ImGui::Spacing();
                    DrawVec3Transform("Translation", translation);
                    DrawVec3Transform("Rotation", rotation);
                    DrawVec3Transform("Scale", scale, 1.0);

                    component.SetTranslation(translation);
                    component.SetRotation(rotation);
                    component.SetScale(scale);
                },
                TRANSFORM_HAS_PLUS_BUTTON);

        DrawComponent<RenderComponent>(fmt::format("{} Mesh", ICON_MD_VIEW_IN_AR), currentlyActiveEntity, [](auto& component) -> void {
            auto& objData{ component.GetObjectData() };

            ImGui::Unindent();
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::Button(fmt::format(" {} Source ", ICON_MD_SOURCE).c_str());
            ImGui::PopItemFlag();

            ImGui::SameLine();

            std::string path{ "No model loaded" };

            if (objData.IsPrefab) {
                path = AssetsManager::GetModelPrefabByType(objData.PrefabType).GetDirectory().string();
            }

            if (!objData.ModelPath.empty() && !objData.ModelName.empty()) {
                path = objData.ModelPath;
            }

            ImGui::PushFont(ImGuiManager::GetFonts()[ImGuiManager::IMGUI_MANAGER_FONT_JET_BRAINS_17]);
            ImGui::InputText("##PathToModel", path.data(), path.size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopFont();

            ImGui::SameLine();
            if (objData.IsPrefab) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            if (ImGui::Button(fmt::format(" {} Load ", ICON_MD_SEARCH).c_str())) {
                // prepare filters for the dialog
                std::initializer_list<std::pair<std::string, std::string>> filters{
                    { "OBJ files", "obj" },
                    { "glTF files", "gltf" },
                    { "FBX files", "fbx" }
                };

                path = Serializer::OpenDialog(filters);

                if (!path.empty()) {
                    const Path_T modelPath{ path };

                    // Load the model
                    AssetsManager::LoadModel(modelPath);

                    objData.ModelPath = modelPath;
                    objData.ModelName = modelPath.stem();

                    // Setup renderer component
                    component.GetObjectData().IsPrefab = false;
                    component.GetObjectData().PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;

                    // Add a material for each one of the meshes of this model
                    auto modelPtr{ AssetsManager::GetModel(modelPath) };

                    for (auto& mesh : modelPtr->GetMeshes()) {
                        // Emplace mesh metadata, mesh and material
                        MeshMetaData meshMetaData{};
                        meshMetaData.ModelMesh = std::addressof(mesh);

                        auto it{ std::find_if(mesh.GetTextures().begin(), mesh.GetTextures().end(),
                                              [](const std::shared_ptr<Texture2D> &texture) -> bool { return texture->GetType() == MapType::TEXTURE_2D_DIFFUSE; }) };

                        if (it != mesh.GetTextures().end()) {
                            // If this mesh has got a diffuse map, we can use it with the standard material,
                            // otherwise we have to apply a base material which only allows to change base aspects like the color
                            DefaultMaterialCreateSpec spec{};
                            spec.DiffuseMap = it != mesh.GetTextures().end() ? *it : nullptr;

                            meshMetaData.MeshMaterial = Material::CreateStandardMaterial(spec);
                            component.GetObjectData().MeshMeta.push_back(std::move(meshMetaData));
                        } else {
                            // Apply base material
                            meshMetaData.MeshMaterial = Material::CreateColoredMaterial();
                            component.GetObjectData().MeshMeta.push_back(std::move(meshMetaData));
                        }
                    }
                }
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            if (objData.IsPrefab) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
            }

            ImGui::Indent();
        });

        DrawComponent<MaterialComponent>(fmt::format("{} Material", ICON_MD_INSIGHTS), currentlyActiveEntity, [](auto& component) -> void {
            MaterialComponentEditor(component);
        });

        DrawComponent<NativeScriptComponent>(fmt::format("{} Script", ICON_MD_CODE), currentlyActiveEntity, [](auto& component) -> void {
            (void)component;
        });

        DrawComponent<CameraComponent>(fmt::format("{} Camera", ICON_MD_CAMERA_ALT), currentlyActiveEntity,
           [](auto& component) -> void {
               static const std::array<std::string, 2> CAMERA_PROJECTION_TYPE_NAMES{ "Orthographic", "Perspective" };

               // This is the camera's current projection type
               const auto cameraCurrentProjectionType{ component.GetCameraPtr()->GetProjectionType() };

               // This is the camera's current projection type as a string
               const auto& currentProjectionTypeStr{CAMERA_PROJECTION_TYPE_NAMES[cameraCurrentProjectionType] };

               if (ImGui::BeginCombo("Projection", currentProjectionTypeStr.c_str())) {
                   UInt32_T projectionIndex{};
                   for (const auto& projectionType : CAMERA_PROJECTION_TYPE_NAMES) {
                       // Indicates if that we want to highlight this projection in the ImGui combo.
                       // This will be the case if this projection type is the current one for this camera.
                       bool isSelected{ projectionType == CAMERA_PROJECTION_TYPE_NAMES[cameraCurrentProjectionType] };

                       // Create a selectable combo item for each perspective
                       if (ImGui::Selectable(projectionType.c_str(), isSelected)) {
                           component.GetCameraPtr()->SetProjectionType((Camera::ProjectionType) projectionIndex);
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
    }

    auto InspectorPanel::OnUpdate(MKT_UNUSED_VAR float timeStep) -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible));

            auto& currentlyActiveEntity{ SceneManager::GetCurrentlySelectedEntity() };
            DrawVisibilityCheckBox(currentlyActiveEntity);
            ImGui::SameLine();
            DrawNameTextInput(currentlyActiveEntity);
            AddComponentButtonFor(currentlyActiveEntity);

            DrawComponents();

            ImGui::End();
        }
    }
}