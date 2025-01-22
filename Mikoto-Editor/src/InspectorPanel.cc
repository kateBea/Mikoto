/**
 * InspectorPanel.cc
 * Created by kate on 6/25/23.
 * */

// C++ Standard Library
#include <algorithm>
#include <array>
#include <iterator>

// Third-Party Libraries
#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

// Project Headers
#include <GUI/IconsMaterialDesign.h>

#include <Assets/AssetsManager.hh>
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>
#include <Core/FileManager.hh>
#include <GUI/ImGuiManager.hh>
#include <GUI/ImGuiUtils.hh>
#include <Material/Material/StandardMaterial.hh>
#include <Panels/InspectorPanel.hh>
#include <STL/Filesystem/FileUtilities.hh>
#include <Scene/Component.hh>
#include <Scene/Entity.hh>
#include <Scene/SceneManager.hh>

namespace Mikoto {
    template<LightType, typename UIFunc>
    static auto DrawLightTypeOptions(UIFunc &&func, LightComponent &lightComponent) -> void {
        func(lightComponent);
    }

    auto InspectorPanel::OpenMaterialEditor() -> void {
        // You only open this window if you have a material
        // since it is the only way to click on the button to edit one
        if (!m_TargetMaterialForMaterialEditor || !m_OpenMaterialEditor) {
            return;
        }

        ImGui::Begin(fmt::format("{} Material editor", ICON_MD_EDIT_SQUARE).c_str(),
                     std::addressof(m_OpenMaterialEditor));

        if (m_TargetMaterialForMaterialEditor->GetType() == Type::MATERIAL_TYPE_STANDARD) {
            // Standard Material ----------------

            // If mesh has standard material. That one requires a diffuse map and a specular map
            ImGui::TextUnformatted(fmt::format("{}", ICON_MD_TEXTURE).c_str());

            ImGui::SameLine();

            ImGui::TextUnformatted(" Albedo");

            // For now, we are not loading the material textures, we have to retrieve this data from the currently selected mesh in the renderer component
            Texture2D *texture2DAlbedo{std::dynamic_pointer_cast<StandardMaterial>(
                    m_TargetMaterialForMaterialEditor)->GetDiffuseMap().get()};
            ImGuiUtils::PushImageButton(texture2DAlbedo, ImVec2{128, 128});

            if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {

                ImGuiUtils::PushImageButton(texture2DAlbedo, ImVec2{256, 256});

                ImGui::SameLine();

                // Table showings texture properties
                constexpr ImGuiTableFlags tableFlags{ImGuiTableFlags_SizingStretchProp};

                if (ImGui::BeginTable("MaterialEditorDiffusePropertiesTable", 2, tableFlags)) {
                    // First row - first colum
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Dimensions");

                    // First row - second colum
                    ImGui::TableSetColumnIndex(1);
                    UInt32_T width{static_cast<UInt32_T>( texture2DAlbedo->GetWidth())};
                    UInt32_T height{static_cast<UInt32_T>( texture2DAlbedo->GetHeight())};
                    ImGui::TextUnformatted(fmt::format("{} x {}", width, height).c_str());

                    // Second row - first colum
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Type");

                    // Second row - second colum
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(Texture2D::GetFileTypeStr(texture2DAlbedo->GetFileType()).data());

                    // Third row - first colum
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("File size");

                    // Third row - second colum
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(fmt::format("{:.2f} MB", texture2DAlbedo->GetSize()).c_str());

                    ImGui::EndTable();
                }

                ImGui::EndTooltip();
            }

            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

            ImGui::SameLine();
            // Table to control albedo mix color and ambient value
            // Table has two rows and one colum
            constexpr auto columnIndex{0};

            constexpr ImGuiTableFlags tableFlags{ImGuiTableFlags_None};

            if (ImGui::BeginTable("AlbedoEditContentsTable", 1, tableFlags)) {
                // First row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(columnIndex);

                static constexpr ImGuiColorEditFlags colorEditFlags{
                        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview};
                static glm::vec4 color{};
                if (ImGui::ColorEdit4("Color", glm::value_ptr(color), colorEditFlags)) {}
                if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

                // Second row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(columnIndex);
                static float mixing{};
                ImGui::SliderFloat("Mix", std::addressof(mixing), 0.0f, 1.0f);
                if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Specular component ------------------

            ImGui::Spacing();
            ImGui::TextUnformatted(fmt::format("{}", ICON_MD_TEXTURE).c_str());

            ImGui::SameLine();

            ImGui::TextUnformatted(" Specular");
            Texture2D *texture2DSpecular{m_EmptyTexturePlaceHolder.get()};
            ImGuiUtils::PushImageButton(texture2DSpecular, ImVec2{128, 128});

            if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

            ImGui::SameLine();
            // Table to control specular component
            // Table has one row and one colum
            constexpr auto columnCount{1};
            constexpr auto columnIndexSpecular{0};

            constexpr ImGuiTableFlags specularTableFlags{ImGuiTableFlags_None};

            if (ImGui::BeginTable("SpecularEditContentsTable", columnCount, specularTableFlags)) {
                // First row
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(columnIndexSpecular);
                static float strength{};
                ImGui::SliderFloat("Strength", std::addressof(strength), 0.0f, 32.0f);
                if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

                ImGui::EndTable();
            }
        }

        ImGui::End();
    }


    static auto ShowTextureHoverTooltip(Texture2D *texture) -> void {
        if (ImGui::IsItemHovered() &&
            ImGui::BeginTooltip() /** && has albedo map, otherwise it display info about the */ ) {

            ImGuiUtils::PushImageButton(texture, ImVec2{128, 128});

            ImGui::SameLine();

            // Table showings texture properties
            constexpr ImGuiTableFlags tableFlags{ImGuiTableFlags_SizingStretchProp};

            if (ImGui::BeginTable("MaterialEditorDiffusePropertiesTable", 2, tableFlags)) {
                // First row - first colum
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Dimensions");

                // First row - second colum
                ImGui::TableSetColumnIndex(1);
                UInt32_T width{static_cast<UInt32_T>( texture->GetWidth())};
                UInt32_T height{static_cast<UInt32_T>( texture->GetHeight())};
                ImGui::TextUnformatted(fmt::format("{} x {}", width, height).c_str());

                // Second row - first colum
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Type");

                // Second row - second colum
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(Texture2D::GetFileTypeStr(texture->GetFileType()).data());

                // Third row - first colum
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("File size");

                // Third row - second colum
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(fmt::format("{:.2f} MB", texture->GetSize()).c_str());

                ImGui::EndTable();
            }

            ImGui::EndTooltip();
        }
        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }
    }


    static auto EditStandardMaterial(StandardMaterial &standardMat) -> void {
        static constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                          ImGuiTreeNodeFlags_AllowItemOverlap |
                                                          ImGuiTreeNodeFlags_Framed |
                                                          ImGuiTreeNodeFlags_SpanAvailWidth |
                                                          ImGuiTreeNodeFlags_FramePadding };
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::TreeNodeEx((void *) "EditStandardMaterialDiffuseTreeNode", treeNodeFlags, "Diffuse")) {
            // We use the standard default font with FONT_ICON_FILE_NAME_MD font icons
            // since the other fonts don't correctly display these icons
            ImGui::TextUnformatted(fmt::format("{}", ICON_MD_TEXTURE).c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(" Albedo");

            Texture2D* diffuseMap{ standardMat.GetDiffuseMap().get() };
            ImGuiUtils::PushImageButton(diffuseMap, ImVec2{ 64, 64 });

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            if (standardMat.HasDiffuseMap()) {
                ShowTextureHoverTooltip(diffuseMap);
            }

            ImGui::SameLine();

            // Table to control albedo mix color and ambient value
            // Table has two rows and one colum
            constexpr auto columnIndex{ 0 };
            constexpr ImGuiTableFlags tableFlags{ImGuiTableFlags_None};

            if (ImGui::BeginTable("DiffuseEditContentsTable", 1, tableFlags)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(columnIndex);

                glm::vec4 color{ standardMat.GetColor() };
                constexpr ImGuiColorEditFlags colorEditFlags{ ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

                if (ImGui::ColorEdit4("Color", glm::value_ptr(color), colorEditFlags)) {
                    standardMat.SetColor(color);
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(columnIndex);

                static float mixing{};
                ImGui::SliderFloat("Mix", std::addressof(mixing), 0.0f, 1.0f);

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::Spacing();

        if (ImGui::TreeNodeEx((void *) "EditStandardMaterialSpecularTreeNode", treeNodeFlags, "Specular")) {
            ImGui::TextUnformatted(fmt::format("{}", ICON_MD_TEXTURE).c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(" Specular");

            Texture2D* specularMap{ standardMat.GetSpecularMap().get() };

            ImGuiUtils::PushImageButton(specularMap, ImVec2{ 64, 64 });

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            if (standardMat.HasSpecularMap()) {
                ShowTextureHoverTooltip(specularMap);
            }

            ImGui::SameLine();
            // Table to control specular component
            // Table has one row and one colum
            constexpr auto columnCount{ 1 };
            constexpr auto columnIndexSpecular{ 0 };
            constexpr ImGuiTableFlags specularTableFlags{ ImGuiTableFlags_None };

            if (ImGui::BeginTable("SpecularEditContentsTable", columnCount, specularTableFlags)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(columnIndexSpecular);

                static float strength{};

                ImGui::SliderFloat("Strength", std::addressof(strength), 0.0f, 32.0f);

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }


    /**
     * @brief Adds a button to insert components to an entity. This button is only added
     * if the parameter is a valid entity, does nothing otherwise.
     *
     * @param entity Entity to adds components on.
     * */
    static auto DrawComponentButton(Entity &entity) -> void {
        if (!entity.IsValid()) {
            return;
        }

        ImGui::SameLine();
        ImGui::PushItemWidth(-1.0f);

        if (ImGui::Button("Add component")) {
            ImGui::OpenPopup("AddComponentButtonPopup");
        }

        if (ImGui::BeginPopup("AddComponentButtonPopup")) {
            const bool menuItemSelected{ false };
            const char* menuItemShortcut{ nullptr };

            if (ImGui::MenuItem("Material", menuItemShortcut, menuItemSelected,
                                !entity.HasComponent<MaterialComponent>())) {
                entity.AddComponent<MaterialComponent>();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Script", menuItemShortcut, menuItemSelected,
                                !entity.HasComponent<NativeScriptComponent>())) {
                entity.AddComponent<NativeScriptComponent>();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Mesh", menuItemShortcut, menuItemSelected, !entity.HasComponent<RenderComponent>())) {
                entity.AddComponent<RenderComponent>();

                if (!entity.HasComponent<MaterialComponent>()) {
                    entity.AddComponent<MaterialComponent>();
                }

                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Camera", menuItemShortcut, menuItemSelected,
                                !entity.HasComponent<CameraComponent>())) {
                entity.AddComponent<CameraComponent>(std::make_shared<SceneCamera>());
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Lighting", menuItemShortcut, menuItemSelected,
                                !entity.HasComponent<LightComponent>())) {
                entity.AddComponent<LightComponent>();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Physics", menuItemShortcut, menuItemSelected,
                                !entity.HasComponent<PhysicsComponent>())) {
                entity.AddComponent<PhysicsComponent>();
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Audio", menuItemShortcut, menuItemSelected, !entity.HasComponent<AudioComponent>())) {
                entity.AddComponent<AudioComponent>();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        ImGui::PopItemWidth();
    }


    static constexpr auto GetInspectorPanelName() -> std::string_view {
        return "Inspector";
    }


    InspectorPanel::InspectorPanel()
            : Panel{} {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_ERROR_OUTLINE, GetInspectorPanelName());

        m_EmptyTexturePlaceHolder = Texture2D::Create(FileManager::Assets::GetRootPath() / "Icons/emptyTexture.png",
                                                      MapType::TEXTURE_2D_DIFFUSE);

        if (!m_EmptyTexturePlaceHolder) {
            MKT_CORE_LOGGER_ERROR("Could not load empty texture placeholder for inspector channel!");
        }
    }


    static auto
    DrawVec3Transform(std::string_view label, glm::vec3 &data, double resetValue = 0.0, double columWidth = 100.0) {
        // This Group is part of a unique label
        ImGui::PushID(label.data());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, (float) columWidth);
        ImGui::Text("%s", label.data());
        ImGui::NextColumn();
        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{7.0f, 5.0f});

        const float lineHeight{GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f};
        const ImVec2 buttonSize{lineHeight + 3.0f, lineHeight};

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.1f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.1f, 1.0f});

        if (ImGui::Button("X", buttonSize)) {
            data.x = (float) resetValue;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }


        ImGui::SameLine();
        ImGui::DragFloat("##X", &data.x, 0.1f, 0.0f, 0.0f, "%.2f");

        ImGui::PopItemWidth();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.1f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.25f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.1f, 1.0f});

        if (ImGui::Button("Y", buttonSize)) {
            data.y = (float) resetValue;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::SameLine();
        ImGui::DragFloat("##Y", &data.y, 0.1f, 0.0f, 0.0f, "%.2f");

        ImGui::PopItemWidth();
        ImGui::PopStyleColor(3);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.25f, 0.3f, 0.9f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});

        if (ImGui::Button("Z", buttonSize)) {
            data.z = (float) resetValue;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::SameLine();
        ImGui::DragFloat("##Z", &data.z, 0.1f, 0.0f, 0.0f, "%.2f");

        ImGui::PopStyleColor(3);
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();

        ImGui::Columns(1);

        ImGui::PopID();
    }


    template<typename ComponentType, typename UIFunction>
    static auto DrawComponent( const std::string_view componentLabel, Entity &entity, UIFunction &&uiFunc, const bool hasRemoveButton = true ) -> void {
        static constexpr ImGuiTreeNodeFlags treeNodeFlags{ ImGuiTreeNodeFlags_DefaultOpen |
                                                           ImGuiTreeNodeFlags_AllowItemOverlap |
                                                           ImGuiTreeNodeFlags_Framed |
                                                           ImGuiTreeNodeFlags_SpanAvailWidth |
                                                           ImGuiTreeNodeFlags_FramePadding };

        if ( entity.HasComponent<ComponentType>() ) {
            bool removeComponent{ false };
            const ImVec2 contentRegionAvailable{ ImGui::GetContentRegionAvail() };

            ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2{ 4.0f, 4.0f } );

            // See ImGui implementation for button dimensions computation
            const float lineHeight{ GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f };

            const bool componentNodeOpen{
                ImGui::TreeNodeEx( reinterpret_cast<void *>( typeid( ComponentType ).hash_code() ), treeNodeFlags, "%s",
                                   componentLabel.data() )
            };

            if ( ImGui::IsItemHovered() ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            }

            ImGui::PopStyleVar();

            if ( hasRemoveButton ) {
                ImGui::SameLine( contentRegionAvailable.x - lineHeight * 0.5f );
                if ( ImGui::Button( fmt::format( "{}", ICON_MD_SETTINGS ).c_str(), ImVec2{ lineHeight, lineHeight } ) ) {
                    ImGui::OpenPopup( "ComponentSettingsButton" );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
                }

                if ( ImGui::BeginPopup( "ComponentSettingsButton" ) ) {
                    if ( ImGui::MenuItem( "Remove Component" ) ) {
                        removeComponent = true;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }

            if ( componentNodeOpen ) {
                ComponentType &component{ entity.GetComponent<ComponentType>() };

                uiFunc( component );

                ImGui::TreePop();
            }

            if ( removeComponent ) {
                entity.RemoveComponent<ComponentType>();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
    }


    static auto DrawVisibilityCheckBox(Entity &entity) {
        if (!entity.IsValid()) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity.GetComponent<TagComponent>() };

        bool wantToRenderActiveEntity{ tag.IsVisible() };
        if (ImGui::Checkbox("##DrawVisibilityCheckBox::Checkbox", std::addressof(wantToRenderActiveEntity))) {
            tag.SetVisibility(!tag.IsVisible());
        }
    }


    static auto DrawNameTextInput(Entity &entity) -> void {
        if (!entity.IsValid()) {
            return;
        }

        // All entities are guaranteed to have a TagComponent
        TagComponent& tag{ entity.GetComponent<TagComponent>() };

        // Input text to change target's name
        constexpr ImGuiTextFlags flags{};
        std::array<char, 1024> selectionName{};
        std::copy(tag.GetTag().begin(), tag.GetTag().end(), selectionName.data());

        if (ImGui::InputText("##DrawNameTextInputTag", selectionName.data(), std::size(selectionName), flags)) {
            tag.SetTag(selectionName.data());
        }
    }


    static auto AddTransformComponentTab(Entity& entity) {
        // By default, all scene objects have a transform component which cannot be removed
        constexpr bool TRANSFORM_HAS_PLUS_BUTTON{ false };

        DrawComponent<TransformComponent>(
                fmt::format("{} Transform", ICON_MD_DEVICE_HUB),
                entity,
                [](auto &component) -> void {
                    auto translation{component.GetTranslation()};
                    auto rotation{component.GetRotation()};
                    auto scale{component.GetScale()};

                    ImGui::Spacing();

                    DrawVec3Transform("Translation", translation);
                    DrawVec3Transform("Rotation", rotation);
                    DrawVec3Transform("Scale", scale, 1.0);

                    component.SetTranslation(translation);
                    component.SetRotation(rotation);
                    component.SetScale(scale);
                },
                TRANSFORM_HAS_PLUS_BUTTON);
    }


    static auto AddNativeScriptingComponentTab(Entity& entity) {
        DrawComponent<NativeScriptComponent>(fmt::format("{} Script", ICON_MD_CODE), entity,
                                             [](auto &component) -> void {
                                                 (void) component;
                                             });
    }


    static auto AddMaterialComponentTab(Entity& entity) {
        DrawComponent<MaterialComponent>(fmt::format("{} Material", ICON_MD_INSIGHTS), entity,
                                         [&](auto &component) -> void {
                                             ImGui::Unindent();

                                             MaterialInfo& matInfo{ component.GetMaterialInfo() };

                                             if (matInfo.MeshMat) {
                                                 EditStandardMaterial( *std::dynamic_pointer_cast<StandardMaterial>(matInfo.MeshMat) );
                                             }

                                             ImGui::Indent();
                                         });
    }


    static auto AddPhysicsComponentTab(Entity& entity) {
        DrawComponent<PhysicsComponent>(fmt::format("{} Physics", ICON_MD_FITNESS_CENTER), entity,
                                        [](auto &component) -> void {
                                            (void) component;
                                        });
    }


    static auto ShowGameObjectMaterialInfo( const GameObject & gameObject) {
        if (gameObject.MeshData.Data != nullptr) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Mesh Info");
            ImGui::SameLine();

            auto& meshTarget{ *gameObject.MeshData.Data };

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::Spacing();
            ImGui::Spacing();


            // Assumes we have one type of each map for a single mesh!
            // A mesh cannot have more than one diffuse map, for example.
            Int32_T diffuseIndex{ -1 };
            Int32_T specularIndex{ -1 };
            Int32_T emissionIndex{ -1 };
            Int32_T normalIndex{ -1 };
            Int32_T roughnessIndex{ -1 };
            Int32_T metallicIndex{ -1 };
            Int32_T aoIndex{ -1 };

            // Get the texture indices
            for (Int32_T index{}; index < static_cast<Int32_T>( meshTarget.GetTextures().size() ); ++index) {
                switch (meshTarget.GetTextures()[index]->GetType()) {
                    case MapType::TEXTURE_2D_DIFFUSE: diffuseIndex = index; break;
                    case MapType::TEXTURE_2D_SPECULAR: specularIndex = index; break;
                    case MapType::TEXTURE_2D_EMISSIVE: emissionIndex = index; break;
                    case MapType::TEXTURE_2D_NORMAL: normalIndex = index; break;
                    case MapType::TEXTURE_2D_ROUGHNESS: roughnessIndex = index; break;
                    case MapType::TEXTURE_2D_METALLIC: metallicIndex = index; break;
                    case MapType::TEXTURE_2D_AMBIENT_OCCLUSION: aoIndex = index; break;

                    case MapType::TEXTURE_2D_INVALID:
                    case MapType::TEXTURE_2D_COUNT:
                        MKT_CORE_LOGGER_ERROR("Error, type of texture not valid!");
                        break;
                }
            }

            // Tooltip displaying texture details
            auto displayMapInformation{
                    [](const Texture2D *map, const std::string_view mapName) -> void {
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::TextUnformatted(fmt::format("{} ", ICON_MD_PANORAMA).c_str());
                        ImGui::SameLine();
                        ImGui::TextUnformatted(mapName.data());

                        ImGui::Spacing();

                        ImGuiUtils::PushImageButton(map, ImVec2{ 64, 64 });

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        }

                        ImGui::SameLine();

                        constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp };

                        if (ImGui::BeginTable("MaterialEditorDiffusePropertiesTable", 2, tableFlags)) {
                            // First row - first colum
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted("Dimensions");

                            // First row - second colum
                            ImGui::TableSetColumnIndex(1);
                            UInt32_T width{static_cast<UInt32_T>( map->GetWidth())};
                            UInt32_T height{static_cast<UInt32_T>( map->GetHeight())};
                            ImGui::TextUnformatted(fmt::format("{} x {}", width, height).c_str());

                            // Second row - first colum
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted("Type");

                            // Second row - second colum
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(Texture2D::GetFileTypeStr(map->GetFileType()).data());

                            // Third row - first colum
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted("File size");

                            // Third row - second colum
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(fmt::format("{:.2f} MB", map->GetSize()).c_str());

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted("Channels");

                            // Third row - second colum
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(fmt::format("{}", map->GetChannels()).c_str());

                            ImGui::EndTable();
                        }
                    }
            };

            // Diffuse
            if (diffuseIndex != -1) {
                displayMapInformation(meshTarget.GetTextures()[diffuseIndex].get(), "Albedo");
            }

            // Specular
            if (specularIndex != -1) {
                displayMapInformation(meshTarget.GetTextures()[specularIndex].get(), "Specular");
            }

            // Normal
            if (normalIndex != -1) {
                displayMapInformation(meshTarget.GetTextures()[normalIndex].get(), "Normal");
            }

            // Emissive
            if (emissionIndex != -1) {
                displayMapInformation(meshTarget.GetTextures()[emissionIndex].get(), "Emmissive");
            }

            // Roughness
            if (roughnessIndex != -1) {
                displayMapInformation(meshTarget.GetTextures()[roughnessIndex].get(), "Roughness");
            }

            // Metallic
            if (metallicIndex != -1) {
                displayMapInformation(meshTarget.GetTextures()[metallicIndex].get(), "Metal");
            }

            // Ao
            if (aoIndex != -1) {
                displayMapInformation(meshTarget.GetTextures()[aoIndex].get(), "Ambient Occlusion");
            }
        }
    }


    static auto AddRenderComponentTab(Entity& entity) {
        DrawComponent<RenderComponent>(
            fmt::format("{} Mesh", ICON_MD_VIEW_IN_AR),
            entity,
            [&entity](auto& component) -> void {
                GameObject& objData{ component.GetObjectData() };

                ImGui::Unindent();
                ImGui::Spacing();

                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::Button(fmt::format(" {} Source ", ICON_MD_ARCHIVE).c_str());
                ImGui::PopItemFlag();

                ImGui::SameLine();

                std::string path{ "Internal" };

                if (!objData.ModelPath.empty()) {
                    path = objData.ModelPath.string();
                }

                ImGui::InputText("##PathToModel", path.data(), path.size(), ImGuiInputTextFlags_ReadOnly);

                ImGui::SameLine();

                if (ImGui::Button(fmt::format(" {} Load ", ICON_MD_SEARCH).c_str())) {
                    const std::initializer_list<std::pair<std::string, std::string>> filters{
                            { "Model files", "obj, gltf, fbx" },
                            { "OBJ files",   "obj" },
                            { "glTF files",  "gltf" },
                            { "FBX files",   "fbx" }
                    };

                    path = FileManager::OpenDialog(filters);

                    if (!path.empty()) {
                        const Path_T modelPath{ path };
                        objData.ModelPath = modelPath;
                        objData.ModelName = StringUtils::GetByteChar( modelPath.stem() );

                        const ModelLoadInfo modelLoadInfo{
                            .ModelPath = modelPath,
                            .InvertedY = Renderer::GetActiveGraphicsAPI() == GraphicsAPI::VULKAN_API,
                            .WantTextures = true,
                        };

                        EntityCreateInfo entityCreateInfo{
                            .Name = objData.ModelName,
                            .PrefabType = PrefabSceneObject::CUSTOM_MODEL_PREFAB_OBJECT,
                        };

                        Model* model{ AssetsManager::LoadModel(modelLoadInfo) };
                        SceneManager::GetActiveScene().CreatePrefabEntity(objData.ModelName, model, std::addressof(entity));
                    }
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                auto& renderData{ component.GetObjectData() };
                ShowGameObjectMaterialInfo(renderData);

                ImGui::Indent();
            });
    }


    static auto AddLightComponentTab(Entity& entity) {
        DrawComponent<LightComponent>(fmt::format("{} Lighting", ICON_MD_LIGHT), entity,
                                      [](auto &component) -> void {
            static constexpr std::array<std::string_view, 3> lightTypes{ "Directional light", "Point light",
                                                                        "Spot light" };

            auto lightType{ component.GetType() };
            constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingFixedFit };

            // Directional light type -----------
            auto directionalLightOptions{ [](LightComponent &lightComponent) -> void {
                if (ImGui::BeginTable("DirectionalLightEditTable", 2, tableFlags)) {
                    constexpr ImGuiColorEditFlags colorEditFlags{
                            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Direction");

                    ImGui::SameLine();

                    HelpMarker(
                            "In the case of the fourth component having a value of 1.0f\n"
                            "we do light calculations using the light's position instead\n"
                            "which is the position of the game object.");

                    ImGui::TableSetColumnIndex(1);

                    glm::vec4 direction{ lightComponent.GetDirLightData().Direction };
                    if (ImGui::DragFloat4("##DirectionalLightDirection", glm::value_ptr(direction), 0.1f, 0.0f, 512.0f, "%.2f")) {
                        lightComponent.GetDirLightData().Direction = direction;
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Ambient");

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableSetColumnIndex(1);

                    glm::vec4 ambient{ lightComponent.GetDirLightData().Ambient };
                    if (ImGui::ColorEdit4("##DirectionalLightAmbient", glm::value_ptr(ambient), colorEditFlags)) {
                        lightComponent.GetDirLightData().Ambient = ambient;
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Diffuse");

                    ImGui::TableSetColumnIndex(1);

                    glm::vec4 diffuse{ lightComponent.GetDirLightData().Diffuse };
                    if (ImGui::ColorEdit4("##DirectionalLightDiffuse", glm::value_ptr(diffuse), colorEditFlags)) {
                        lightComponent.GetDirLightData().Diffuse = diffuse;
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Specular");

                    ImGui::TableSetColumnIndex(1);

                    glm::vec4 specular{ lightComponent.GetDirLightData().Specular };
                    if (ImGui::ColorEdit4("##DirectionalLightSpecular", glm::value_ptr(specular), colorEditFlags)) {
                        lightComponent.GetDirLightData().Specular = specular;
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Cast shadows");

                    ImGui::TableSetColumnIndex(1);
                    static bool castShadows{};
                    ImGui::Checkbox("##DirectionalLightSahdows", std::addressof(castShadows));

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::EndTable();
                }
            }};


            // Point light type -----------------
            auto pointLightOptions{ [](LightComponent& lightComponent) -> void {
                if (ImGui::BeginTable("PointLightMainTable", 2, tableFlags)) {
                    auto &pointLightData{lightComponent.GetPointLightData()};

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Ambient");

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableSetColumnIndex(1);
                    static constexpr ImGuiColorEditFlags colorEditFlags{
                            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview };

                    glm::vec4 ambientComponent{pointLightData.Ambient};
                    if (ImGui::ColorEdit3("##PointAmbientComponent", glm::value_ptr(ambientComponent),
                                          colorEditFlags)) {
                        pointLightData.Ambient = ambientComponent;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Diffuse");

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableSetColumnIndex(1);

                    glm::vec4 diffuseComponent{ pointLightData.Diffuse };
                    if (ImGui::ColorEdit3("##PointDiffuseComponent", glm::value_ptr(diffuseComponent),
                                          colorEditFlags)) {
                        pointLightData.Diffuse = diffuseComponent;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Specular");

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableSetColumnIndex(1);

                    glm::vec4 specularComponent{ pointLightData.Specular };
                    if (ImGui::ColorEdit3("##PointSpecularComponent", glm::value_ptr(specularComponent),
                                          colorEditFlags)) {
                        pointLightData.Specular = specularComponent;
                    }


                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Constant");
                    if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

                    ImGui::TableSetColumnIndex(1);

                    float constant{ pointLightData.Components.x };

                    if (ImGui::SliderFloat("##PointConstantComponent", std::addressof(constant), 0.1f, 1.0f)) {
                        pointLightData.Components.x = constant;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Linear");

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableSetColumnIndex(1);

                    float linear{ pointLightData.Components.y };

                    if (ImGui::SliderFloat("##PointLinearComponent", std::addressof(linear), 0.0f, 2.0f)) {
                        pointLightData.Components.y = linear;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Quadratic");

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableSetColumnIndex(1);

                    float quadratic{ pointLightData.Components.z };

                    if (ImGui::SliderFloat("##PointQuadraticComponent", std::addressof(quadratic), 0.0f, 1.0f)) {
                        pointLightData.Components.z = quadratic;
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::TableSetColumnIndex(1);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Cast shadows");

                    static bool castShadows{};
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("##PointLightSahdows", std::addressof(castShadows));

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::EndTable();
                }
            }};


            // Spotlight type -------------------
            auto spotLightLightOptions{ [](LightComponent& lightComponent) -> void {
                if (ImGui::BeginTable("SpotLightEditTable", 2, tableFlags)) {
                    auto& spotLightData{ lightComponent.GetSpotLightData() };

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Direction");

                    ImGui::SameLine();

                    HelpMarker("The spot position is determined by the objects position.");

                    ImGui::TableSetColumnIndex(1);

                    glm::vec4 direction{ spotLightData.Direction };
                    if (ImGui::DragFloat3("##SpotLightDirection", glm::value_ptr(direction), 0.01f, -1.0f, 1.0f,
                                          "%.2f")) {
                        spotLightData.Direction = direction;
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    // Constants
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Constant");

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        }

                        ImGui::TableSetColumnIndex(1);

                        float constant{ spotLightData.Components.x };

                        if (ImGui::SliderFloat("##SpotConstantComponent", std::addressof(constant), 0.1f, 1.0f,
                                               "%.6f")) {
                            spotLightData.Components.x = constant;
                        }

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Linear");

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        }

                        ImGui::TableSetColumnIndex(1);

                        float linear{ spotLightData.Components.y };
                        if (ImGui::SliderFloat("##SpotLinearComponent", std::addressof(linear), 0.0014f, 1.8f,
                                               "%.6f")) {
                            spotLightData.Components.y = linear;
                        }

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Quadratic");

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        }

                        ImGui::TableSetColumnIndex(1);

                        float quadratic{ spotLightData.Components.z };

                        if (ImGui::SliderFloat("##SpotQuadraticComponent", std::addressof(quadratic), 0.000007f, 1.8f,
                                               "%.6f")) {
                            spotLightData.Components.z = quadratic;
                        }
                    }

                    // Components
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Ambient");

                        ImGui::TableSetColumnIndex(1);
                        static constexpr ImGuiColorEditFlags colorEditFlags{
                                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview};

                        glm::vec4 ambientComponent{ spotLightData.Ambient };
                        if (ImGui::ColorEdit3("##SpotAmbientComponent", glm::value_ptr(ambientComponent),
                                              colorEditFlags)) {
                            spotLightData.Ambient = ambientComponent;
                        }

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Diffuse");

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        }

                        ImGui::TableSetColumnIndex(1);

                        glm::vec4 diffuseComponent{ spotLightData.Diffuse };
                        if (ImGui::ColorEdit3("##SpotDiffuseComponent", glm::value_ptr(diffuseComponent),
                                              colorEditFlags)) {
                            spotLightData.Diffuse = diffuseComponent;
                        }

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Specular");

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        }

                        ImGui::TableSetColumnIndex(1);

                        glm::vec4 specularComponent{ spotLightData.Specular };
                        if (ImGui::ColorEdit3("##SpotSpecularComponent", glm::value_ptr(specularComponent),
                                              colorEditFlags)) {
                            spotLightData.Specular = specularComponent;
                        }
                    }

                    // angles
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Cut-off");

                        ImGui::SameLine();
                        HelpMarker("Angles in degrees");

                        ImGui::TableSetColumnIndex(1);
                        float cutOff{ glm::degrees(spotLightData.CutOffValues.x) };
                        if (ImGui::SliderFloat("##SpotLightCutoff", std::addressof(cutOff), 0.0f, 360.0f, "%.1f")) {
                            spotLightData.CutOffValues.x = glm::radians(cutOff);
                        }

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        }

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted("Outer cut-off");

                        ImGui::SameLine();
                        HelpMarker("Angles in degrees");

                        ImGui::TableSetColumnIndex(1);
                        float outerCutOff{ glm::degrees(spotLightData.CutOffValues.y) };
                        if (ImGui::SliderFloat("##SpotLightOuterCutOff", std::addressof(outerCutOff), 0.0f, 360.0f,
                                               "%.1f")) {
                            spotLightData.CutOffValues.y = glm::radians(outerCutOff);
                        }

                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        }
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Cast shadows");

                    static bool castShadows{};
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Checkbox("##SpotLightSahdows", std::addressof(castShadows));

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    ImGui::EndTable();
                }
            }};


            ImGui::TextUnformatted("Light type ");
            ImGui::SameLine();

            if (ImGui::BeginCombo("##LighType", lightTypes[(Size_T) lightType].data())) {
                Size_T lightTypeIndex{};
                for (const auto& currentType: lightTypes) {
                    // Tells whether we want to highlight this light type in the ImGui combo.
                    // This will be the case if the current type of light is the same as the component
                    bool isSelected{currentType == lightTypes[(Size_T) lightType]};

                    // This cast is valid because lightTypeIndex is always in the range [0, 2]
                    // where each index indicates a type of light, see LightType definition.
                    LightType selectedType{ (LightType) lightTypeIndex };

                    // Create a selectable combo item for each light type
                    if (ImGui::Selectable(currentType.data(), isSelected)) {
                        // Update the type of light for this component
                        component.SetType(selectedType);
                    }

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }

                    ++lightTypeIndex;
                }

                ImGui::EndCombo();
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            switch (component.GetType()) {
                case LightType::DIRECTIONAL_LIGHT_TYPE:
                    DrawLightTypeOptions<LightType::DIRECTIONAL_LIGHT_TYPE>(directionalLightOptions, component);
                    break;

                case LightType::POINT_LIGHT_TYPE:
                    DrawLightTypeOptions<LightType::POINT_LIGHT_TYPE>(pointLightOptions, component);
                    break;

                case LightType::SPOT_LIGHT_TYPE:
                    DrawLightTypeOptions<LightType::SPOT_LIGHT_TYPE>(spotLightLightOptions, component);
                    break;
            }
        });
    }


    static auto AddAudioComponentTab(Entity& entity) {
        DrawComponent<AudioComponent>(fmt::format("{} Audio", ICON_MD_AUDIOTRACK), entity,
                                      [](auto &component) -> void {
            constexpr ImGuiTableFlags tableFlags{ ImGuiTableFlags_SizingStretchProp };

            if (ImGui::BeginTable("MaterialEditorDiffusePropertiesTable", 2, tableFlags)) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Audio clip");

                ImGui::TableSetColumnIndex(1);

                std::string path{ component.GetSourcePath().string() };
                ImGui::InputText("##AudioClipSource", path.data(), path.size(), ImGuiInputTextFlags_ReadOnly);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Muted");

                ImGui::TableSetColumnIndex(1);
                bool isMuted{ component.IsMuted() };
                if (ImGui::Checkbox("##IsMutedAudio", std::addressof(isMuted))) {
                    component.Mute(isMuted);
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Loop");

                ImGui::TableSetColumnIndex(1);
                bool isLooping{component.IsLooping()};
                if (ImGui::Checkbox("##IsLoopingAudio", std::addressof(isLooping))) {
                    component.SetLooping(isLooping);
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                ImGui::EndTable();
            }
        });
    }


    static auto AddCameraComponentTab(Entity& entity) {
        DrawComponent<CameraComponent>(fmt::format("{} Camera", ICON_MD_CAMERA_ALT), entity,
            [](auto &component) -> void {
               static const std::array<std::string, 2> CAMERA_PROJECTION_TYPE_NAMES{
                       "Orthographic", "Perspective" };

               const auto cameraCurrentProjectionType{ component.GetCameraPtr()->GetProjectionType() };

               constexpr ImGuiTableFlags tableFlags{
                       ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchSame };

               if (ImGui::BeginTable("DirectionalLightEditTable", 2, tableFlags)) {
                   ImGui::TableNextRow();
                   ImGui::TableSetColumnIndex(0);
                   ImGui::TextUnformatted("Projection Type");

                   if (ImGui::IsItemHovered()) {
                       ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                   }

                   ImGui::TableSetColumnIndex(1);
                   const auto &currentProjectionTypeStr{ CAMERA_PROJECTION_TYPE_NAMES[cameraCurrentProjectionType] };

                   if (ImGui::BeginCombo("##Projection",
                                         currentProjectionTypeStr.c_str())) {
                       UInt32_T projectionIndex{};
                       for (const auto &projectionType: CAMERA_PROJECTION_TYPE_NAMES) {
                           // Tells whether we want to highlight this projection in the ImGui combo.
                           // This will be the case if this projection type is the current one for this camera.
                           bool isSelected{ projectionType == CAMERA_PROJECTION_TYPE_NAMES[cameraCurrentProjectionType]};

                           // Create a selectable combo item for each perspective
                           if (ImGui::Selectable(projectionType.c_str(), isSelected)) {
                               component.GetCameraPtr()->SetProjectionType((Camera::ProjectionType) projectionIndex);
                           }

                           if (ImGui::IsItemHovered()) {
                               ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                           }

                           if (isSelected) {
                               ImGui::SetItemDefaultFocus();
                           }

                           ++projectionIndex;
                       }

                       ImGui::EndCombo();
                   }

                   if (ImGui::IsItemHovered()) {
                       ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                   }

                   ImGui::EndTable();
               }

               if (component.GetCameraPtr()->GetProjectionType() == SceneCamera::ProjectionType::ORTHOGRAPHIC) {

                   if (ImGui::BeginTable("##OrthographicProjControl", 2, tableFlags)) {

                       ImGui::TableNextRow();
                       ImGui::TableSetColumnIndex(0);
                       ImGui::TextUnformatted("Orthographic Size");

                       ImGui::TableSetColumnIndex(1);
                       float size{ (float) component.GetCameraPtr()->GetOrthographicSize() };
                       if (ImGui::SliderFloat("##Orthographic Size", &size, 2.0f, 10.0f)) {
                           component.GetCameraPtr()->SetOrthographicSize(size);
                       }

                       if (ImGui::IsItemHovered()) {
                           ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                       }


                       ImGui::TableNextRow();
                       ImGui::TableSetColumnIndex(0);
                       ImGui::TextUnformatted("Orthographic Near");

                       ImGui::TableSetColumnIndex(1);
                       float nearPlane{ (float) component.GetCameraPtr()->GetOrthographicNearPlane() };

                       if (ImGui::SliderFloat("##Orthographic Near", &nearPlane, -5.0, -1.0)) {
                           component.GetCameraPtr()->SetOrthographicNearPlane(nearPlane);
                       }

                       if (ImGui::IsItemHovered()) {
                           ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                       }

                       ImGui::TableNextRow();
                       ImGui::TableSetColumnIndex(0);
                       ImGui::TextUnformatted("Orthographic Far");

                       ImGui::TableSetColumnIndex(1);
                       float farPlane{ (float) component.GetCameraPtr()->GetOrthographicFarPlane() };
                       if (ImGui::SliderFloat("##Orthographic Far", &farPlane, 1.0, 5.0)) {
                           component.GetCameraPtr()->SetOrthographicFarPlane(farPlane);
                       }

                       if (ImGui::IsItemHovered()) {
                           ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                       }

                       component.GetCameraPtr()->SetOrthographic(nearPlane, farPlane, size);

                       ImGui::EndTable();
                   }
               }

               if (component.GetCameraPtr()->GetProjectionType() == SceneCamera::ProjectionType::PERSPECTIVE) {

                   if (ImGui::BeginTable("##PerspectiveProjControl", 2, tableFlags)) {

                       ImGui::TableNextRow();
                       ImGui::TableSetColumnIndex(0);
                       ImGui::TextUnformatted("Perspective FOV");

                       ImGui::TableSetColumnIndex(1);
                       float fov{ (float) component.GetCameraPtr()->GetPerspectiveFOV() };
                       if (ImGui::SliderFloat("##Perspective FOV", std::addressof(fov), 45.0f, 90.0f)) {
                           component.GetCameraPtr()->SetPerspectiveFOV(fov);
                       }

                       if (ImGui::IsItemHovered()) {
                           ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                       }

                       ImGui::TableNextRow();
                       ImGui::TableSetColumnIndex(0);
                       ImGui::TextUnformatted("Perspective Near");

                       ImGui::TableSetColumnIndex(1);
                       float nearPlane{
                               (float) component.GetCameraPtr()->GetPerspectiveNearPlane()};
                       if (ImGui::SliderFloat("##Perspective Near", std::addressof(nearPlane), 0.001f, 1.0)) {
                           component.GetCameraPtr()->SetPerspectiveNearPlane(nearPlane);
                       }

                       if (ImGui::IsItemHovered()) {
                           ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                       }

                       ImGui::TableNextRow();
                       ImGui::TableSetColumnIndex(0);
                       ImGui::TextUnformatted("Perspective Far");

                       ImGui::TableSetColumnIndex(1);
                       float farPlane{ (float) component.GetCameraPtr()->GetPerspectiveFarPlane() };
                       if (ImGui::SliderFloat("##Perspective Far", &farPlane, 100.0f, 10000.0f)) {
                           component.GetCameraPtr()->SetPerspectiveFarPlane(farPlane);
                       }

                       if (ImGui::IsItemHovered()) {
                           ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                       }

                       component.GetCameraPtr()->SetPerspective(nearPlane, farPlane, fov);

                       ImGui::EndTable();
                   }
               }
            });
    }


    auto InspectorPanel::DrawComponents(Entity& entity) -> void {
        if (!entity.IsValid()) {
            return;
        }

        AddTransformComponentTab(entity);
        AddMaterialComponentTab(entity);
        AddPhysicsComponentTab(entity);
        AddRenderComponentTab(entity);
        AddLightComponentTab(entity);
        AddAudioComponentTab(entity);
        AddCameraComponentTab(entity);
        AddNativeScriptingComponentTab(entity);
    }


    auto InspectorPanel::OnUpdate(MKT_UNUSED_VAR float timeStep) -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin( m_PanelHeaderName.c_str(), std::addressof( m_PanelIsVisible ), ImGuiWindowFlags_NoCollapse );

            const auto activeEntity{ SceneManager::GetCurrentSelection() };
            if (activeEntity.has_value()) {
                DrawVisibilityCheckBox(activeEntity->get());

                ImGui::SameLine();

                DrawNameTextInput(activeEntity->get());
                DrawComponentButton(activeEntity->get());

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                DrawComponents(activeEntity->get());
            }

            ImGui::End();
        }
    }
}