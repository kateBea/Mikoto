//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cmath>
#include <mutex>
#include <cfloat>
#include <cstring>
#include <stdexcept>

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>

#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Threading/TaskService.hh>

#include <Filesystem/FileSystem.hh>

#include <Assets/MeshFactory.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/Utility.hh>
#include <Material/TextMaterial.hh>
#include <Material/VolumeMaterial.hh>
#include <Material/SkyboxMaterial.hh>
#include <Material/ParticleMaterial.hh>
#include <Material/PostProcessMaterial.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Application/ThumbnailCache.hh>
#include <Application/MaterialProperties.hh>

#include <Panels/MaterialEditorPanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace asset;
    using namespace filesystem;
    using namespace material;
    using namespace renderer;
    using namespace renderer::rhi;

    struct MaterialEditorPanel::ModelLoad {
        std::mutex mMutex{};
        asset::ModelHandle mModel{};
        eastl::string mError{};
        bool mComplete{};
    };

    MaterialEditorPanel::MaterialEditorPanel( const MaterialEditorPanelDescription& description )
        : Panel{ "Material Editor", "Panel", description.mGroup, description.mIsToggleable },
          mEditorState{ description.mState },
          mDevice{ description.mDevice } {
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_PALETTE, mPanelName );
        mPanelIsVisible = false;
        mPreview.mInfiniteGrid = true;

        // Keep this viewport's output independent of browser thumbnail jobs.
        mRenderer = renderer::ThumbnailRenderer::Create( renderer::ThumbnailRendererCreateInfo{}
            .SetName( "Material Editor Preview" )
            .SetDevice( mDevice ) );
        mRenderer->Initialize();
    }

    auto MaterialEditorPanel::SetMaterial( core::Ref<material::SurfaceMaterial> material ) -> void {
        SetAsset( material.As<material::IMaterial>() );
    }

    auto MaterialEditorPanel::SetAsset( material::MaterialHandle material ) -> void {
        if ( !material ) {
            mEditorState->mEditMaterial = {};
        }
        else if ( !mEditorState->mEditMaterial || mEditorState->mEditMaterial->GetResource().GetPtr() != material.GetPtr() ) {
            mEditorState->mEditMaterial = core::Ref<asset::Material>::New( material );
        }

        mMaterialResource = eastl::move( material );
        mDomainMaterials.fill( material::MaterialHandle{} );

        mGraph.Reset();
        mError.clear();

        mMaterial = dynamic_cast<material::SurfaceMaterial*>( mMaterialResource.GetPtr() ) ? mMaterialResource.As<material::SurfaceMaterial>() : core::Ref<material::SurfaceMaterial>{};

        mName.fill( '\0' );

        if ( mMaterialResource ) {
            const auto& name{ mMaterialResource->GetName() };
            std::memcpy( mName.data(), name.data(), eastl::min( name.size(), mName.size() - 1 ) );
        }

        mPreviewDirty = true;
        mPreviewPending = false;
        mPreviewTexture.Reset();
        mUnsaved = mMaterialResource && material::GetMaterialSourcePath( *mMaterialResource ).IsEmpty();
    }

    auto MaterialEditorPanel::RequestPreviewModel() -> void {
        filesystem::Path path{ "Resources/Prefabs/material_sphere/scene.gltf" };

        if ( !path.Exists() ) {
            path = filesystem::Path{ "Mikoto/Resources/Resources/Prefabs/material_sphere/scene.gltf" };
        }

        if ( !path.Exists() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot find Resources/Prefabs/material_sphere/scene.gltf." );
        }

        auto state{ eastl::make_shared<ModelLoad>() };

        // The task owns this mailbox, not the panel. Closing the window while
        // loading never leaves a callback holding a destroyed panel pointer.
        threading::TaskService::Get()->ExecuteWorkerTask( [state, path]() {
            asset::ModelHandle model{};
            eastl::string error{};

            try {
                model = MeshFactory::Get()->ImportModel( ModelLoadDescription{
                    .mFile = filesystem::FileHandle::New( path ),
                    .mApi = renderer::rhi::GraphicsAPI::eVulkan,
                    .mExtractTextures = true
                } );

                if ( !model || !model->GetMeshNodeCount() ) {
                    MKT_THROW_RUNTIME_ERROR( "Material sphere has no mesh." );
                }
            } catch ( const std::exception& exception ) {
                error = exception.what();
            }

            std::lock_guard lock{ state->mMutex };
            state->mModel = eastl::move( model );
            state->mError = eastl::move( error );
            state->mComplete = true;
        } );

        mModelLoad = eastl::move( state );
    }

    auto MaterialEditorPanel::OnRender( float timeStep ) -> void {
        MKT_BEGIN_PROFILER();
        const bool openRequested{ mEditorState->mRequestMaterialEditor };
        mEditorState->mRequestMaterialEditor = false;

        if ( openRequested ) {
            mPanelIsVisible = true;
        }

        // The state owns the edited asset. Selection changes do not replace it;
        // only an explicit open request switches this workspace to another material.
        const auto resource{ mEditorState->mEditMaterial ? mEditorState->mEditMaterial->GetResource() : material::MaterialHandle{} };

        if ( resource.GetPtr() != mMaterialResource.GetPtr() ) {
            SetAsset( resource );
        }

        mRenderer->Poll();

        // Publish only a completed preview for the current material.
        if ( mPreviewPending && !mRenderer->IsBusy() ) {
            mPreviewPending = false;

            const auto error{ mRenderer->GetPreviewError() };
            if ( error.empty() ) {
                mPreviewTexture = mRenderer->GetPreviewTexture();
            }
            else {
                mError.assign( error.data(), error.size() );
            }
        }

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( ImVec2{ 1100, 760 }, ImGuiCond_FirstUseEver );

        if ( openRequested ) {
            ImGui::SetNextWindowFocus();
        }

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        mPanelIsHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows );
        mPanelIsFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_ChildWindows );
        const bool textPreview{ dynamic_cast<const material::TextMaterial*>( mMaterialResource.GetPtr() ) != nullptr };

        try {
            if ( !textPreview ) {
                if ( !mEnvironment.IsRequested() ) {
                    mEnvironment.Request( PreviewEnvironment::StreetPath() );
                }

                mPreviewDirty |= mEnvironment.Poll( mPreview.mEnvironment );
            }

            if ( !textPreview && !mModelLoad ) {
                RequestPreviewModel();
            }

            if ( !textPreview && mModelLoad && !mModel ) {
                std::lock_guard lock{ mModelLoad->mMutex };

                if ( mModelLoad->mComplete ) {
                    mModel = mModelLoad->mModel;
                    mError = mModelLoad->mError;

                    if ( mModel && !mMaterialResource ) {
                        SetMaterial( core::Ref<material::SurfaceMaterial>::New( mModel->GetMeshNode( 0 ).GetProperties() ) );
                    }
                }
            }
        } catch ( const std::exception& error ) {
            mError = error.what();
        }

        if ( !textPreview && !mModel ) {
            ImGui::TextWrapped( "%s", mError.empty() ? MKT_LOC( "material_editor_loading_material_sphere" ).c_str() : mError.c_str() );

            if ( !mError.empty() && ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_retry", "Retry" ).c_str() ) ) {
                mModelLoad.reset();
                mError.clear();
            }

            ImGui::End();
            return;
        }

        DrawToolbar();

        if ( !mError.empty() ) {
            ImGui::TextWrapped( "%s", mError.c_str() );

            if ( ImGui::SmallButton( imgui::widget::LocalizedLabel( "material_editor_dismiss_retry_preview", "Dismiss / retry preview" ).c_str() ) ) {
                mError.clear();
                mPreviewDirty = true;
            }
        }

        ImGui::Separator();

        if ( ImGui::BeginTable( "Material workspace", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp ) ) {
            ImGui::TableSetupColumn( MKT_LOC( "material_editor_preview_and_details" ).c_str(), ImGuiTableColumnFlags_WidthStretch, 0.30f );
            ImGui::TableSetupColumn( MKT_LOC( "material_editor_material_graph" ).c_str(), ImGuiTableColumnFlags_WidthStretch, 0.70f );
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex( 0 );

            if ( ImGui::BeginChild( "Preview and details", {}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar ) ) {
                // Keep details scrollable independently of the preview. Clamp the
                // splitter when the dock shrinks so neither region becomes unreachable.
                const core::f32 height{ ImGui::GetContentRegionAvail().y };
                const core::f32 maximum{ eastl::max( 80.0f, height - 120.0f ) };
                const core::f32 minimum{ eastl::min( 180.0f, maximum ) };
                mPreviewHeight = glm::clamp( mPreviewHeight, minimum, maximum );
                ImGui::SetNextWindowSizeConstraints( { 0, minimum }, { FLT_MAX, maximum } );

                if ( ImGui::BeginChild( "Preview canvas", { 0, mPreviewHeight }, ImGuiChildFlags_ResizeY | ImGuiChildFlags_Borders ) ) {
                    mPreviewHeight = ImGui::GetWindowHeight();
                    DrawPreview( timeStep );
                }

                ImGui::EndChild();

                if ( ImGui::BeginChild( "Material properties" ) ) {
                    ImGui::SeparatorText( MKT_LOC( "material_editor_details" ).c_str() );
                    ImGui::PushItemWidth( -140.0f );
                    DrawMaterial();
                    DrawLighting();
                    ImGui::PopItemWidth();
                }

                ImGui::EndChild();
            }

            ImGui::EndChild();
            ImGui::TableSetColumnIndex( 1 );

            DrawGraph();

            ImGui::EndTable();
        }

        ImGui::End();
    }

    auto MaterialEditorPanel::DrawGraph() -> void {
        material::MaterialGraph* graph{};

        if ( mMaterial ) {
            graph = mMaterial->GetGraph();

            if ( graph && graph->GetNodes().empty() ) {
                // Start a new graph with the edited material's surface factors.
                graph->InitializeDefault();

                for ( auto& node: graph->GetNodes() ) {
                    if ( node.mType != material::MaterialNodeType::eStandardSurface ) {
                        continue;
                    }

                    const auto& color{ mMaterial->GetBaseColorFactor() };

                    for ( auto& pin: node.mInputs ) {
                        if ( pin.mName == "base_color" ) {
                            pin.mValue = color;
                        }
                        else if ( pin.mName == "metalness" ) {
                            pin.mValue = core::float4{ mMaterial->GetMetallicFactor() };
                        }
                        else if ( pin.mName == "specular_roughness" ) {
                            pin.mValue = core::float4{ mMaterial->GetRoughnessFactor() };
                        }
                        else if ( pin.mName == "emission" ) {
                            pin.mValue = core::float4{ mMaterial->GetEmissiveStrength() };
                        }
                        else if ( pin.mName == "emission_color" ) {
                            pin.mValue = core::float4{ mMaterial->GetEmissiveFactor(), 1.0f };
                        }
                        else if ( pin.mName == "opacity" ) {
                            pin.mValue = core::float4{ color.a };
                        }
                    }
                }

                mUnsaved = true;
            }
        }
        else if ( auto* particle{ dynamic_cast<material::ParticleMaterial*>( mMaterialResource.GetPtr() ) } ) {
            graph = particle->GetGraph();
        }
        else if ( auto* postProcess{ dynamic_cast<material::PostProcessMaterial*>( mMaterialResource.GetPtr() ) } ) {
            graph = postProcess->GetGraph();
        }

        if ( graph ) {
            core::cstr surfaceLabel{};

            if ( mMaterial ) {
                switch ( mMaterial->GetShadingModel() ) {
                    case material::ShadingModel::eDefaultPbr:
                        surfaceLabel = MKT_LOC( "material_editor_standard_surface" ).c_str();
                        break;
                    case material::ShadingModel::eClearCoat:
                        surfaceLabel = MKT_LOC( "material_editor_clear_coat" ).c_str();
                        break;
                    case material::ShadingModel::eToonShading:
                        surfaceLabel = MKT_LOC( "material_editor_toon_shading" ).c_str();
                        break;
                    case material::ShadingModel::eFlatShading:
                        surfaceLabel = MKT_LOC( "material_editor_flat_shading" ).c_str();
                        break;
                    case material::ShadingModel::eCellShading:
                        surfaceLabel = MKT_LOC( "material_editor_cell_shading" ).c_str();
                        break;
                    case material::ShadingModel::eSubsurfaceScattering:
                        surfaceLabel = MKT_LOC( "material_editor_subsurface_scattering" ).c_str();
                        break;
                    case material::ShadingModel::eDielectric:
                        surfaceLabel = MKT_LOC( "material_dielectric" ).c_str();
                        break;
                    case material::ShadingModel::eConductor:
                        surfaceLabel = MKT_LOC( "material_conductor" ).c_str();
                        break;
                    case material::ShadingModel::eUnlit:
                        surfaceLabel = MKT_LOC( "material_unlit" ).c_str();
                        break;
                    default:
                        break;
                }
            }

            mUnsaved |= mGraph.Render( *graph, surfaceLabel );
        }
        else {
            ImGui::TextDisabled( MKT_LOC( "material_editor_this_material_type_has_no_authoring_graph" ).c_str() );
        }
    }

    auto MaterialEditorPanel::DrawToolbar() -> void {
        // Applying to the selected entity is disabled in Play. Materials opened
        // from the Inspector are already shared with their owning component.
        auto* selected{ mEditorState ? mEditorState->mSelectedEntity : nullptr };
        material::MaterialHandle selectedMaterial{};

        if ( selected ) {
            if ( selected->HasComponent<scene::CloudComponent>() ) {
                selectedMaterial = selected->GetComponent<scene::CloudComponent>().GetMaterial();
            }
            else if ( selected->HasComponent<scene::FogComponent>() ) {
                selectedMaterial = selected->GetComponent<scene::FogComponent>().GetMaterial();
            }
            else if ( selected->HasComponent<scene::PostProcessMaterialComponent>() ) {
                selectedMaterial = selected->GetComponent<scene::PostProcessMaterialComponent>().GetMaterial();
            }
            else if ( selected->HasComponent<scene::MaterialComponent>() ) {
                selectedMaterial = selected->GetComponent<scene::MaterialComponent>().GetMaterial();
            }
            else if ( selected->HasComponent<scene::TextComponent>() ) {
                selectedMaterial = selected->GetComponent<scene::TextComponent>().GetMaterial().As<material::IMaterial>();
            }
            else if ( selected->HasComponent<scene::SkyboxMaterialComponent>() ) {
                selectedMaterial = selected->GetComponent<scene::SkyboxMaterialComponent>().GetMaterial();
            }
        }

        const bool canApply{ selected && !mEditorState->mRuntimeActive &&
                             ( ( mMaterial && selected->HasComponent<scene::MaterialComponent>() ) ||
                                 ( dynamic_cast<material::VolumeMaterial*>( mMaterialResource.GetPtr() ) &&
                                     ( selected->HasComponent<scene::CloudComponent>() || selected->HasComponent<scene::FogComponent>() ) ) ||
                                 ( dynamic_cast<material::PostProcessMaterial*>( mMaterialResource.GetPtr() ) && selected->HasComponent<scene::PostProcessMaterialComponent>() ) ||
                                 ( dynamic_cast<material::TextMaterial*>( mMaterialResource.GetPtr() ) && selected->HasComponent<scene::TextComponent>() ) ||
                                 ( dynamic_cast<material::SkyboxMaterial*>( mMaterialResource.GetPtr() ) && selected->HasComponent<scene::SkyboxMaterialComponent>() ) ) };

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_new", "New" ).c_str() ) ) {
            ImGui::OpenPopup( imgui::widget::LocalizedLabel( "material_editor_new_material_type", "New material type" ).c_str() );
        }

        if ( ImGui::BeginPopup( imgui::widget::LocalizedLabel( "material_editor_new_material_type", "New material type" ).c_str() ) ) {
            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "material_editor_surface_material", "Surface material" ).c_str() ) ) {
                SetMaterial( core::Ref<material::SurfaceMaterial>::New( "New Material" ) );
                mMaterial->SetMetallicFactor( 0.0f );
                mMaterial->SetRoughnessFactor( 0.5f );
                mUnsaved = true;
            }

            const auto create{ [&]( core::cstr label, auto factory ) {
                if ( ImGui::MenuItem( label ) ) {
                    SetAsset( factory() );
                    mUnsaved = true;
                }
            } };

            create( MKT_LOC( "material_editor_particle_material" ).c_str(), [] {
                return core::Ref<ParticleMaterial>::New().As<material::IMaterial>();
            } );
            create( MKT_LOC( "volume_material" ).c_str(), [] {
                return core::Ref<material::VolumeMaterial>::New().As<material::IMaterial>();
            } );

            create( MKT_LOC( "material_editor_text_material" ).c_str(), [] {
                return core::Ref<material::TextMaterial>::New().As<material::IMaterial>();
            } );

            create( MKT_LOC( "material_editor_skybox_material" ).c_str(), [] {
                return core::Ref<material::SkyboxMaterial>::New().As<material::IMaterial>();
            } );

            create( MKT_LOC( "material_editor_color_grading_material" ).c_str(), [] {
                return core::Ref<material::PostProcessMaterial>::New().As<material::IMaterial>();
            } );

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_open", "Open..." ).c_str() ) ) {
            LoadMaterial();
        }

        ImGui::SameLine();
        ImGui::BeginDisabled( mEditorState->mSandboxMode );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_save", "Save" ).c_str() ) ) {
            SaveMaterial( false );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_save_as", "Save as..." ).c_str() ) ) {
            SaveMaterial( true );
        }

        ImGui::EndDisabled();

        ImGui::BeginDisabled( !selectedMaterial );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_open_in_graph_editor", "Open in graph editor" ).c_str() ) && selectedMaterial ) {
            mEditorState->mEditMaterial = core::Ref<asset::Material>::New( selectedMaterial );

            if ( selectedMaterial.GetPtr() != mMaterialResource.GetPtr() ) {
                SetAsset( selectedMaterial );
            }

            mEditorState->mRequestMaterialEditor = true;
        }

        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled( !canApply );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_apply_to_selected", "Apply to selected" ).c_str() ) && canApply ) {
            try {
                if ( mMaterial ) {
                    selected->GetComponent<scene::MaterialComponent>().SetMaterial( material::SurfaceMaterialHandle::New( *mMaterial ) );
                }
                else {
                    auto clone{ CloneMaterial( *mMaterialResource ) };

                    if ( dynamic_cast<material::VolumeMaterial*>( clone.GetPtr() ) ) {
                        if ( selected->HasComponent<scene::CloudComponent>() ) {
                            selected->GetComponent<scene::CloudComponent>().SetMaterial( clone.As<material::VolumeMaterial>() );
                        }
                        else {
                            selected->GetComponent<scene::FogComponent>().SetMaterial( clone.As<material::VolumeMaterial>() );
                        }
                        SetAsset( clone );
                        mUnsaved = true;
                    }
                    else if ( dynamic_cast<material::TextMaterial*>( clone.GetPtr() ) ) {
                        selected->GetComponent<scene::TextComponent>().SetMaterial( clone.As<material::TextMaterial>() );
                    }
                    else if ( dynamic_cast<material::SkyboxMaterial*>( clone.GetPtr() ) ) {
                        selected->GetComponent<scene::SkyboxMaterialComponent>().SetMaterial( clone );
                    }
                    else if ( dynamic_cast<material::PostProcessMaterial*>( clone.GetPtr() ) ) {
                        selected->GetComponent<scene::PostProcessMaterialComponent>().SetMaterial( clone );
                        // Continue editing the instance just assigned to this entity.
                        SetAsset( clone );
                        mUnsaved = true;
                    }
                }
            } catch ( const std::exception& error ) {
                mError = error.what();
            }
        }

        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled( "%s", mUnsaved ? MKT_LOC( "material_editor_unsaved_changes" ).c_str() : "" );
    }

    auto MaterialEditorPanel::DrawPreview( core::f32 timeStep ) -> void {
        if ( dynamic_cast<material::VolumeMaterial*>( mMaterialResource.GetPtr() ) ) {
            ImGui::TextWrapped( "%s", MKT_LOC( "volume_preview_help" ).c_str() );
            return;
        }
        if ( dynamic_cast<ParticleMaterial*>( mMaterialResource.GetPtr() ) ) {
            ImGui::TextWrapped( MKT_LOC( "material_editor_particle_material_graph_authoring_shader_integration_is_pending_use_particle" ).c_str() );
            return;
        }

        const bool textPreview{ dynamic_cast<const material::TextMaterial*>( mMaterialResource.GetPtr() ) != nullptr };

        if ( textPreview ) {
            ImGui::TextWrapped( MKT_LOC( "material_editor_text_material_sample_font_with_color_and_opacity_the_font" ).c_str() );
        }
        else {
            if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_reset_view", "Reset view" ).c_str() ) ) {
                mPreview.mYaw = 35.0f;
                mPreview.mPitch = 18.0f;
                mPreview.mDistance = 4.5f;
                mPreviewDirty = true;
            }

            ImGui::SameLine();
            imgui::CheckBox( imgui::widget::LocalizedLabel( "material_editor_orbit", "Orbit" ).c_str(), mAutoRotate );
            ImGui::TextDisabled( MKT_LOC( "material_editor_drag_to_orbit_scroll_to_zoom" ).c_str() );

            if ( mAutoRotate ) {
                mPreview.mYaw = glm::fmod( mPreview.mYaw + timeStep * 18.0f, 360.0f );
                mPreviewDirty = true;
            }
        }

        const auto available{ ImGui::GetContentRegionAvail() };

        if ( available.x < 1 || available.y < 1 ) {
            return;
        }

        // Render with the viewport's aspect instead of fitting a square image
        // inside it. Cap resolution, but retain the full-width camera framing.
        const core::f32 scale{ eastl::min( 1.0f, 1536.0f / eastl::max( available.x, available.y ) ) };
        const core::u32 width{ eastl::max( 1u, core::as<core::u32>( available.x * scale ) ) };
        const core::u32 height{ eastl::max( 1u, core::as<core::u32>( available.y * scale ) ) };
        const bool resized{ width != mPreviewWidth || height != mPreviewPixelHeight };

        try {
            MKT_ASSERT( mRenderer, "Thumbnail renderer for Material Preview is null" );

            if ( ( mPreviewDirty || ( resized && !ImGui::IsMouseDown( ImGuiMouseButton_Left ) ) ) && !mRenderer->IsBusy() && mError.empty() ) {
                // Coalesce mesh edits while the graph is in flight. Flat text
                // samples also rebuild only after an edit, never for camera motion.
                // Text previews are editor-generated images; surface materials use the model graph.
                asset::ImageHandle previewImage{};

                if ( const auto* text{ dynamic_cast<const material::TextMaterial*>( mMaterialResource.GetPtr() ) } ) {
                    previewImage = RenderTextMaterialPreview( *text, width, height );
                }

                if ( mRenderer->BeginModelPreview( mModel, mPreview, mMaterialResource, {}, width, height, previewImage ) ) {
                    mPreviewWidth = width;
                    mPreviewPixelHeight = height;
                    mPreviewDirty = false;
                    mPreviewPending = true;
                }
            }
        } catch ( const std::exception& error ) {
            mError = error.what();
        }

        if ( mPreviewTexture ) {
            const auto& texture{ mPreviewTexture };

            // While a resize is pending, crop the old frame instead of stretching
            // the sphere. The completed render replaces it with the correct aspect.
            const core::f32 sourceAspect{ core::as<core::f32>( texture->GetWidth() ) / texture->GetHeight() };
            const core::f32 targetAspect{ available.x / available.y };

            ImVec2 uv0{ 0, 0 };
            ImVec2 uv1{ 1, 1 };

            if ( sourceAspect > targetAspect ) {
                uv0.x = ( 1.0f - targetAspect / sourceAspect ) * 0.5f;
                uv1.x = 1.0f - uv0.x;
            }
            else {
                uv0.y = ( 1.0f - sourceAspect / targetAspect ) * 0.5f;
                uv1.y = 1.0f - uv0.y;
            }

            const auto origin{ ImGui::GetCursorScreenPos() };
            ImGui::InvisibleButton( "##MaterialPreview", available, ImGuiButtonFlags_MouseButtonLeft );
            ImGui::GetWindowDrawList()->AddImage( imgui::ImGuiService::Get()->GetTextureID( texture ),
                origin, { origin.x + available.x, origin.y + available.y }, uv0, uv1 );
            const bool hovered{ ImGui::IsItemHovered() };

            if ( ImGui::IsItemActivated() ) {
                ImGui::SetWindowFocus();
            }

            if ( !textPreview && ImGui::IsItemActive() && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) ) {
                mPreview.mYaw -= ImGui::GetIO().MouseDelta.x * 0.35f;
                mPreview.mPitch = glm::clamp( mPreview.mPitch + ImGui::GetIO().MouseDelta.y * 0.35f, -80.0f, 80.0f );
                mPreviewDirty = true;
            }

            if ( !textPreview && hovered && ImGui::GetIO().MouseWheel != 0.0f ) {
                mPreview.mDistance = glm::clamp( mPreview.mDistance - ImGui::GetIO().MouseWheel * 0.3f, 2.0f, 20.0f );
                mPreviewDirty = true;
            }
        }
    }

    auto MaterialEditorPanel::DrawTexture( material::MapType type, core::cstr label ) -> void {
        ImGui::PushID( core::as<core::i32>( type ) );
        ImGui::SeparatorText( label );

        auto texture{ mMaterial->GetTexture( type ) };

        bool use{ mMaterial->IsTextureUsed( type ) };

        if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "inspector_use_texture", "UseTexture" ).c_str(), use ) ) {
            mMaterial->UseTexture( type, use );
            mPreviewDirty = mUnsaved = true;
        }

        if ( texture ) {
            ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( texture ), ImVec2{ 54, 54 } );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( "%s", mMaterial->GetTexturePath( type ).GetC_Str() );
            }

            ImGui::SameLine();
        }

        if ( ImGui::Button( texture ? MKT_LOC( "material_editor_replace" ).c_str() : MKT_LOC( "material_editor_choose_texture" ).c_str() ) ) {
            try {
                const std::initializer_list<filesystem::FileDialogPair> filters{
                    { .mDescription = MKT_LOC( "material_editor_texture" ).c_str(), .mPattern = "png,jpg,jpeg,tga,bmp,hdr,ktx2" }
                };
                const auto path{ OpenFileDialog( filters ) };

                if ( !path.IsEmpty() ) {
                    auto loaded{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, renderer::rhi::TextureDimension::eTexture2D ) };

                    if ( !loaded.IsReady() ) {
                        MKT_THROW_RUNTIME_ERROR( MKT_LOC( "material_editor_texture_could_not_be_loaded" ).c_str() );
                    }

                    mMaterial->SetTexture( type, loaded );
                    mPreviewDirty = mUnsaved = true;
                }
            } catch ( const std::exception& error ) {
                mError = error.what();
            }
        }

        if ( texture ) {
            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_remove", "Remove" ).c_str() ) ) {
                mMaterial->RemoveTexture( type );
                mPreviewDirty = mUnsaved = true;
            }
        }

        ImGui::PopID();
    }

    auto MaterialEditorPanel::DrawMaterialDomain() -> void {
        core::i32 domain{ -1 };

        if ( mMaterial ) {
            domain = 0;
        }
        else if ( dynamic_cast<material::PostProcessMaterial*>( mMaterialResource.GetPtr() ) ) {
            domain = 1;
        }
        else if ( dynamic_cast<material::TextMaterial*>( mMaterialResource.GetPtr() ) ) {
            domain = 2;
        }
        else if ( dynamic_cast<ParticleMaterial*>( mMaterialResource.GetPtr() ) ) {
            domain = 3;
        }
        else if ( dynamic_cast<material::SkyboxMaterial*>( mMaterialResource.GetPtr() ) ) {
            domain = 4;
        }
        else if ( dynamic_cast<material::VolumeMaterial*>( mMaterialResource.GetPtr() ) ) {
            domain = 5;
        }

        const core::i32 previous{ domain };

        if ( ImGui::Combo( imgui::widget::LocalizedLabel( "material_editor_material_domain", "Material domain" ).c_str(), &domain, MKT_LOC( "material_editor_surface_post_process_text_particle_skybox" ).c_str() ) && domain != previous ) {
            auto variants{ eastl::move( mDomainMaterials ) };

            if ( previous >= 0 ) {
                variants[previous] = mMaterialResource;
            }

            auto& selected{ variants[domain] };

            if ( !selected ) {
                switch ( domain ) {
                    case 0:
                        selected = core::Ref<material::SurfaceMaterial>::New().As<material::IMaterial>();
                        break;
                    case 1:
                        selected = core::Ref<material::PostProcessMaterial>::New().As<material::IMaterial>();
                        break;
                    case 2:
                        selected = core::Ref<material::TextMaterial>::New().As<material::IMaterial>();
                        break;
                    case 3:
                        selected = core::Ref<ParticleMaterial>::New().As<material::IMaterial>();
                        break;
                    case 4:
                        selected = core::Ref<material::SkyboxMaterial>::New().As<material::IMaterial>();
                        break;
                    case 5:
                        selected = core::Ref<material::VolumeMaterial>::New().As<material::IMaterial>();
                        break;
                    default:;
                }
            }

            selected->SetName( mName.data() );

            SetAsset( selected );

            mDomainMaterials = eastl::move( variants );
            mUnsaved = true;
        }

        if ( ImGui::IsItemHovered() ) {
            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "material_editor_each_domain_has_its_own_settings_and_graph_only_the" ).c_str() );
        }

        if ( mMaterial ) {
            core::i32 model{ static_cast<core::i32>( mMaterial->GetShadingModel() ) };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "material_editor_shading_model", "Shading model" ).c_str(), &model, MKT_LOC( "material_editor_default_pbr_clear_coat_toon_shading_flat_shading_cell_shading" ).c_str() ) ) {
                mMaterial->SetShadingModel( static_cast<material::ShadingModel>( model ) );
                mUnsaved = true;
                mPreviewDirty = true;
            }
        }
    }

    auto MaterialEditorPanel::DrawMaterial() -> void {
        if ( !mMaterialResource ) {
            return;
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "material_editor_material", "Material" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            if ( ImGui::InputText( imgui::widget::LocalizedLabel( "material_editor_name", "Name" ).c_str(), mName.data(), mName.size() ) ) {
                mMaterialResource->SetName( mName.data() );
                mUnsaved = true;
            }

            DrawMaterialDomain();

            if ( !mMaterial ) {
                DrawOtherMaterial();
            }
        }

        if ( !mMaterial ) {
            return;
        }

        bool changed{};
        auto scalar{ [this, &changed]( core::cstr label, auto getter, auto setter, core::f32 minimum, core::f32 maximum ) {
            core::f32 value{ ( mMaterial.GetPtr()->*getter )() };

            if ( imgui::widget::CircleSlider( label, &value, minimum, maximum, "%.2f" ) ) {
                ( mMaterial.GetPtr()->*setter )( value );
                changed = true;
            }
        } };

        {
            auto settings{ mMaterial->GetSurfaceShading() };
            const auto model{ mMaterial->GetShadingModel() };
            bool edited{};

            if ( model == material::ShadingModel::eClearCoat ) {
                edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_coat_weight", "Coat weight" ).c_str(), &settings.mCoatWeight, 0.0f, 1.0f, "%.2f" );
                edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_coat_roughness", "Coat roughness" ).c_str(), &settings.mCoatRoughness, 0.045f, 1., "%.2f" );
            }
            else if ( model == material::ShadingModel::eToonShading ) {
                edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_lighting_threshold", "Lighting threshold" ).c_str(), &settings.mToonThreshold, 0.0f, 1.0f, "%.2f" );
                edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_threshold_softness", "Threshold softness" ).c_str(), &settings.mToonSoftness, 0.001f, 1.0f, "%.2f" );
            }
            else if ( model == material::ShadingModel::eCellShading ) {
                edited |= imgui::widget::CircleInt( imgui::widget::LocalizedLabel( "material_editor_lighting_bands", "Lighting bands" ).c_str(), &settings.mCellBands, 2, 16 );
            }
            else if ( model == material::ShadingModel::eFlatShading ) {
                ImGui::TextWrapped( "%s", MKT_LOC( "material_editor_uses_triangle_normals_normal_maps_are_ignored" ).c_str() );
            }

            if ( model == material::ShadingModel::eToonShading || model == material::ShadingModel::eCellShading ) {
                edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_contour_width_px", "Contour width (px)" ).c_str(), &settings.mContourWidth, 0.0f, 12.0f );

                if ( ImGui::IsItemHovered() ) {
                    mikoto::imgui::widget::ItemTooltip( MKT_LOC( "material_editor_opaque_scene_silhouettes_zero_disables_contours_not_used_by_material" ).c_str() );
                }

                edited |= ImGui::ColorEdit3( imgui::widget::LocalizedLabel( "material_editor_contour_color", "Contour color" ).c_str(), &settings.mContourColor.x );
            }

            if ( edited ) {
                mMaterial->SetSurfaceShading( settings );
                changed = true;
            }
        }

        changed |= DrawMaterialOptics( *mMaterial );

        if ( mMaterial->GetShadingModel() == material::ShadingModel::eSubsurfaceScattering &&
             ImGui::CollapsingHeader( MKT_LOC( "material_raster_diffusion" ).c_str() ) ) {
            auto settings{ mMaterial->GetSubsurface() };
            bool edited{ imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_scatter_scale", "Scatter scale" ).c_str(), &settings.mScatterScale, 0.0f, 10.0f ) };

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "material_editor_screen_space_sigma_rgb_profile_width_x_scatter_scale_x" ).c_str() );
            }

            edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_subsurface_weight", "Subsurface weight" ).c_str(), &settings.mStrength, 0.0f, 1.0f );
            edited |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "material_editor_near_sigma_rgb", "Near sigma RGB" ).c_str(), &settings.mNear.x, 0.005f, 0.001f, 10.0f );
            edited |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "material_editor_far_sigma_rgb", "Far sigma RGB" ).c_str(), &settings.mFar.x, 0.005f, 0.001f, 10.0f );
            edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_near_far_balance", "Near / far balance" ).c_str(), &settings.mNearWeight, 0.0f, 1.0f );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "material_editor_0_wide_far_profile_1_tight_near_profile" ).c_str() );
            }

            int radius{ ( settings.mSamples - 1 ) / 2 };

            if ( ImGui::SliderInt( imgui::widget::LocalizedLabel( "material_editor_sample_radius_px", "Sample radius (px)" ).c_str(), &radius, 3, 32 ) ) {
                settings.mSamples = radius * 2 + 1;
                edited = true;
            }

            ImGui::TextDisabled( MKT_LOC( "material_editor_taps_per_axis" ).c_str(), settings.mSamples );
            edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_depth_edge_threshold", "Depth edge threshold" ).c_str(), &settings.mDepthRejection, 0.0f, 0.2f );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "material_editor_relative_view_depth_change_before_rejecting_blur_samples_zero_disables" ).c_str() );
            }

            edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_transmission_weight", "Transmission weight" ).c_str(), &settings.mTranslucency, 0.0f, 1.0f );
            edited |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "material_editor_transmission_thickness", "Transmission thickness" ).c_str(), &settings.mThickness, 0.01f, 0.0f, 100.0f );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "material_editor_artist_authored_thickness_in_profile_units_larger_values_absorb_more" ).c_str() );
            }

            edited |= ImGui::ColorEdit3( imgui::widget::LocalizedLabel( "material_editor_scattering_weight_rgb", "Scattering weight RGB" ).c_str(), &settings.mColor.x, ImGuiColorEditFlags_Float );
            edited |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_scattering_suppression", "Scattering suppression" ).c_str(), &settings.mOpacity, 0.0f, 1.0f );

            if ( ImGui::IsItemHovered() ) {
                mikoto::imgui::widget::ItemTooltip( MKT_LOC( "material_editor_0_full_scattering_influence_1_ordinary_surface_lighting_does_not" ).c_str() );
            }

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_reset_scattering", "Reset scattering" ).c_str() ) ) {
                settings = material::SubsurfaceSettings{};
                edited = true;
            }

            ImGui::TextWrapped( "%s", MKT_LOC( "material_editor_diffuse_only_horizontal_and_vertical_blur_specular_ambient_and_emission" ).c_str() );

            if ( edited ) {
                mMaterial->SetSubsurface( settings );
                changed = true;
            }

            ImGui::TextWrapped( "%s", MKT_LOC( "material_editor_scene_raster_rendering_including_msaa_this_preview_still_uses_ordinary" ).c_str() );
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "material_editor_surface", "Surface" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            auto color{ mMaterial->GetBaseColorFactor() };

            if ( ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "material_editor_base_color", "Base color" ).c_str(), &color.x, ImGuiColorEditFlags_Float ) ) {
                mMaterial->SetBaseColorFactor( color );
                changed = true;
            }

            const auto model{ mMaterial->GetShadingModel() };
            if ( model != material::ShadingModel::eDielectric && model != material::ShadingModel::eConductor && model != material::ShadingModel::eUnlit ) {
                scalar( MKT_LOC( "material_editor_metallic" ).c_str(), &material::SurfaceMaterial::GetMetallicFactor, &material::SurfaceMaterial::SetMetallicFactor, 0, 1 );
            }

            if ( model != material::ShadingModel::eDielectric && model != material::ShadingModel::eUnlit ) {
                scalar( MKT_LOC( "material_editor_roughness" ).c_str(), &material::SurfaceMaterial::GetRoughnessFactor, &material::SurfaceMaterial::SetRoughnessFactor, 0, 1 );
            }
            if ( model != material::ShadingModel::eUnlit ) {
                scalar( MKT_LOC( "material_editor_normal_scale" ).c_str(), &material::SurfaceMaterial::GetNormalScale, &material::SurfaceMaterial::SetNormalScale, 0, 3 );
                scalar( MKT_LOC( "material_editor_occlusion" ).c_str(), &material::SurfaceMaterial::GetOcclusionStrength, &material::SurfaceMaterial::SetOcclusionStrength, 0, 1 );
            }

            if ( mMaterial->GetWorkflow() != material::Workflow::eMetallicRoughness ) {
                ImGui::TextWrapped( "%s", MKT_LOC( "material_editor_preview_uses_metallic_roughness_pbr_other_saved_shading_settings_are" ).c_str() );
            }
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "material_editor_emission", "Emission" ).c_str() ) ) {
            auto emission{ mMaterial->GetEmissiveFactor() };

            if ( ImGui::ColorEdit3( imgui::widget::LocalizedLabel( "material_editor_emission", "Emission" ).c_str(), &emission.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR ) ) {
                mMaterial->SetEmissiveFactor( emission );
                changed = true;
            }

            scalar( MKT_LOC( "material_editor_emission_strength" ).c_str(), &material::SurfaceMaterial::GetEmissiveStrength, &material::SurfaceMaterial::SetEmissiveStrength, 0, 20 );
            bool bloomy{ mMaterial->IsBloomy() };

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "material_editor_bloom_emission", "Bloom emission" ).c_str(), bloomy ) ) {
                mMaterial->EnableBloom( bloomy );
                changed = true;
            }
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "material_editor_transparency_and_culling", "Transparency and culling" ).c_str() ) ) {
            core::i32 alpha{ static_cast<core::i32>( mMaterial->GetAlphaMask() ) };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "material_editor_alpha_mode", "Alpha mode" ).c_str(), &alpha, MKT_LOC( "material_editor_opaque_masked_blend" ).c_str() ) ) {
                mMaterial->SetAlphaMask( static_cast<material::AlphaMode>( alpha ) );
                changed = true;
            }

            if ( mMaterial->GetAlphaMask() == material::AlphaMode::eMask ) {
                scalar( MKT_LOC( "material_editor_alpha_cutoff" ).c_str(), &material::SurfaceMaterial::GetAlphaMaskCutoff, &material::SurfaceMaterial::SetAlphaMaskCutoff, 0, 1 );
            }

            bool doubleSided{ mMaterial->IsDoubleSided() };

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "material_editor_double_sided", "Double sided" ).c_str(), doubleSided ) ) {
                mMaterial->SetIsDoubleSided( doubleSided );
                changed = true;
            }
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "material_editor_textures_and_uv_sets", "Textures and UV sets" ).c_str() ) ) {
            DrawTexture( material::MapType::eBaseColor, MKT_LOC( "material_editor_base_color_srgb" ).c_str() );
            DrawTexture( material::MapType::eNormal, MKT_LOC( "material_editor_normal_linear" ).c_str() );
            DrawTexture( material::MapType::eMetallicRoughness, MKT_LOC( "material_editor_metallic_roughness_b_g" ).c_str() );

            if ( mMaterial->GetShadingModel() != material::ShadingModel::eDielectric ) {
                DrawTexture( material::MapType::eMetallic, MKT_LOC( "material_editor_metallic_r" ).c_str() );
            }

            DrawTexture( material::MapType::eRoughness, MKT_LOC( "material_editor_roughness_r" ).c_str() );
            DrawTexture( material::MapType::eAmbientOcclusion, MKT_LOC( "material_editor_occlusion_r" ).c_str() );
            DrawTexture( material::MapType::eEmissive, MKT_LOC( "material_editor_emission_srgb" ).c_str() );

            auto uv{ [this, &changed]( core::cstr label, auto getter, auto setter ) {
                core::i32 value{ ( mMaterial.GetPtr()->*getter )() };

                if ( ImGui::Combo( label, &value, "UV 0\0UV 1\0" ) ) {
                    ( mMaterial.GetPtr()->*setter )( value );
                    changed = true;
                }
            } };

            uv( MKT_LOC( "material_editor_base_color_uv" ).c_str(), &material::SurfaceMaterial::GetBaseColorTextureSet, &material::SurfaceMaterial::SetBaseColorTextureSet );
            uv( MKT_LOC( "material_editor_normal_uv" ).c_str(), &material::SurfaceMaterial::GetNormalTextureSet, &material::SurfaceMaterial::SetNormalTextureSet );
            uv( MKT_LOC( "material_editor_metal_rough_uv" ).c_str(), &material::SurfaceMaterial::GetMetallicRoughnessTextureSet, &material::SurfaceMaterial::SetMetallicRoughnessTextureSet );
            uv( MKT_LOC( "material_editor_occlusion_uv" ).c_str(), &material::SurfaceMaterial::GetOcclusionTextureSet, &material::SurfaceMaterial::SetOcclusionTextureSet );
            uv( MKT_LOC( "material_editor_emission_uv" ).c_str(), &material::SurfaceMaterial::GetEmissiveTextureSet, &material::SurfaceMaterial::SetEmissiveTextureSet );
        }

        mPreviewDirty |= changed;
        mUnsaved |= changed;
    }

    auto MaterialEditorPanel::DrawLighting() -> void {
        if ( dynamic_cast<const material::TextMaterial*>( mMaterialResource.GetPtr() ) ) {
            return;
        }

        if ( !ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "material_editor_preview_lighting", "Preview lighting" ).c_str() ) ) {
            return;
        }

        ImGui::TextWrapped( "%s", MKT_LOC( "material_editor_preview_lighting_is_independent_of_the_scene_background_blur_does" ).c_str() );

        mPreviewDirty |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_exposure", "Exposure" ).c_str(), &mPreview.mExposure, 0.1f, 4.0f, "%.2f" );
        mPreviewDirty |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_environment", "Environment" ).c_str(), &mPreview.mEnvironmentIntensity, 0.0f, 4.0f, "%.2f" );
        mPreviewDirty |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_background_blur", "Background blur" ).c_str(), &mPreview.mBackgroundBlur, 0.0f, 6.0f, "%.2f" );
        mPreviewDirty |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_light_intensity", "Light intensity" ).c_str(), &mPreview.mLightIntensity, 0.0f, 10.0f, "%.2f" );
        mPreviewDirty |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_light_yaw", "Light yaw" ).c_str(), &mPreview.mLightYaw, -180.0f, 180.0f, "%.2f" );
        mPreviewDirty |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_light_elevation", "Light elevation" ).c_str(), &mPreview.mLightElevation, 5.0f, 85.0f, "%.2f" );
        mPreviewDirty |= imgui::CheckBox( imgui::widget::LocalizedLabel( "material_editor_ground", "Ground" ).c_str(), mPreview.mGround );
        mPreviewDirty |= imgui::CheckBox( imgui::widget::LocalizedLabel( "material_editor_infinite_grid", "Infinite grid" ).c_str(), mPreview.mInfiniteGrid );
        mPreviewDirty |= imgui::CheckBox( imgui::widget::LocalizedLabel( "material_editor_shadows", "Shadows" ).c_str(), mPreview.mShadows );
        mPreviewDirty |= imgui::CheckBox( imgui::widget::LocalizedLabel( "material_editor_bloom", "Bloom" ).c_str(), mPreview.mBloom.mEnabled );
        mPreviewDirty |= imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_bloom_intensity", "Bloom intensity" ).c_str(), &mPreview.mBloom.mIntensity, 0.0f, 2.0f, "%.2f" );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_choose_hdr_environment", "Choose HDR environment..." ).c_str() ) ) {
            try {
                const auto path{ OpenFileDialog( { { MKT_LOC( "material_editor_hdr_panorama" ).c_str(), "hdr" } } ) };

                if ( !path.IsEmpty() ) {
                    mEnvironment.Request( path );
                }
            } catch ( const std::exception& error ) {
                mError = error.what();
            }
        }

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_street_environment", "Street environment" ).c_str() ) ) {
            mEnvironment.Request( PreviewEnvironment::StreetPath() );
        }

        if ( !mEnvironment.GetError().empty() ) {
            ImGui::TextWrapped( "%s", mEnvironment.GetError().c_str() );
        }
    }

    auto MaterialEditorPanel::LoadMaterial() -> void {
        try {
            const auto path{ OpenFileDialog( { { MKT_LOC( "material_editor_mikoto_material" ).c_str(), "mktmtl" } } ) };

            if ( path.IsEmpty() ) {
                return;
            }

            if ( auto material{ asset::AssetRegistryService::Get()->LoadAsset<asset::Material>( path ) } ) {
                mEditorState->mEditMaterial = eastl::move( material );
                const auto resource{ mEditorState->mEditMaterial->GetResource() };

                if ( resource.GetPtr() != mMaterialResource.GetPtr() ) {
                    SetAsset( resource );
                }

                mEditorState->mRequestMaterialEditor = true;
                mError.clear();
            }
        } catch ( const std::exception& error ) {
            mError = error.what();
        }
    }

    auto MaterialEditorPanel::SaveMaterial( bool choosePath ) -> void {
        const auto asset{ mEditorState->mEditMaterial };

        if ( !asset || mEditorState->mSandboxMode ) {
            return;
        }

        try {
            auto path{ material::GetMaterialSourcePath( *asset->GetResource() ) };

            if ( choosePath || path.IsEmpty() ) {
                path = SaveFileDialog( "Material.mktmtl", { { MKT_LOC( "material_editor_mikoto_material" ).c_str(), "mktmtl" } } );
            }

            if ( path.IsEmpty() ) {
                return;
            }

            asset->GetResource()->Serialize( path );

            mUnsaved = false;
            mError.clear();
        } catch ( const std::exception& error ) {
            mError = error.what();
        }
    }

    auto MaterialEditorPanel::DrawOtherMaterial() -> void {
        bool changed{};

        if ( auto* volume{ dynamic_cast<material::VolumeMaterial*>( mMaterialResource.GetPtr() ) } ) {
            changed = DrawVolumeMaterial( *volume );
        }
        else if ( auto* text{ dynamic_cast<material::TextMaterial*>( mMaterialResource.GetPtr() ) } ) {
            auto color{ text->GetColor() };
            auto opacity{ text->GetOpacity() };

            if ( ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "material_editor_color", "Color" ).c_str(), &color.x ) ) {
                text->SetColor( color );
                changed = true;
            }

            if ( imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_opacity", "Opacity" ).c_str(), &opacity, 0.0f, 1.0f, "%.2f" ) ) {
                text->SetOpacity( opacity );
                changed = true;
            }

            ImGui::TextWrapped( "%s", MKT_LOC( "material_editor_used_by_msdf_and_slug_text_the_component_tint_multiplies" ).c_str() );
        }
        else if ( auto* particle{ dynamic_cast<ParticleMaterial*>( mMaterialResource.GetPtr() ) } ) {
            auto color{ particle->GetColor() };

            if ( ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "material_editor_particle_tint", "Particle tint" ).c_str(), &color.x ) ) {
                particle->SetColor( color );
                changed = true;
            }
        }
        else if ( auto* post{ dynamic_cast<material::PostProcessMaterial*>( mMaterialResource.GetPtr() ) } ) {
            core::i32 category{ core::as<core::i32>( post->GetCategory() ) };
            const core::cstr categories[]{ MKT_LOC( "post_process_category_built_in" ).c_str(), MKT_LOC( "post_process_category_custom" ).c_str() };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "post_process_category", "Category" ).c_str(), &category, categories, 2 ) ) {
                post->SetCategory( core::as<material::PostProcessMaterialCategory>( category ) );
                changed = true;
            }

            core::i32 stage{ core::as<core::i32>( post->GetStage() ) };
            const core::cstr stages[]{ MKT_LOC( "post_process_before_tone_mapping" ).c_str(), MKT_LOC( "post_process_after_tone_mapping" ).c_str() };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "post_process_stage", "Stage" ).c_str(), &stage, stages, 2 ) ) {
                post->SetStage( core::as<material::PostProcessStage>( stage ) );
                changed = true;
            }

            bool grainEnabled{ post->IsFilmGrainEnabled() };

            if ( imgui::CheckBox( imgui::widget::LocalizedLabel( "post_process_film_grain", "Film grain" ).c_str(), grainEnabled ) ) {
                post->SetFilmGrainEnabled( grainEnabled );
                changed = true;
            }

            auto contrast{ post->GetContrast() };
            auto saturation{ post->GetSaturation() };
            auto tint{ post->GetTintColor() };

            if ( imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_contrast", "Contrast" ).c_str(), &contrast, 0.0f, 3.0f, "%.2f" ) ) {
                post->SetContrast( contrast );
                changed = true;
            }

            if ( imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_saturation", "Saturation" ).c_str(), &saturation, 0.0f, 3.0f, "%.2f" ) ) {
                post->SetSaturation( saturation );
                changed = true;
            }

            if ( ImGui::ColorEdit4( imgui::widget::LocalizedLabel( "material_editor_tint_blend", "Tint / blend" ).c_str(), &tint.x ) ) {
                post->SetTintColor( tint );
                changed = true;
            }

            ImGui::TextWrapped( "%s", MKT_LOC( "material_editor_preview_applies_grading_to_the_lit_sphere_scene_post_processing" ).c_str() );
        }
        else if ( auto* sky{ dynamic_cast<material::SkyboxMaterial*>( mMaterialResource.GetPtr() ) } ) {
            core::i32 type{ static_cast<core::i32>( sky->GetType() ) };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "material_editor_source", "Source" ).c_str(), &type, MKT_LOC( "material_editor_six_faces_equirectangular" ).c_str() ) ) {
                sky->SetType( static_cast<material::SkyboxType>( type ) );
                changed = true;
            }

            auto ambient{ sky->GetAmbientScale() };

            if ( imgui::widget::CircleSlider( imgui::widget::LocalizedLabel( "material_editor_ambient_scale", "Ambient scale" ).c_str(), &ambient, 0.0f, 10.0f, "%.2f" ) ) {
                sky->SetAmbientScale( ambient );
                changed = true;
            }

            const auto choose{ [&]( core::cstr label, core::i32 face ) {
                if ( ImGui::Button( label ) ) {
                    try {
                        const auto path{ OpenFileDialog( { { MKT_LOC( "material_editor_image" ).c_str(), "hdr,png,jpg,jpeg,tga,exr,ktx2" } } ) };

                        if ( !path.IsEmpty() ) {
                            auto texture{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, renderer::rhi::TextureDimension::eTexture2D ) };

                            if ( !texture.IsReady() ) {
                                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "material_editor_could_not_load_skybox_image" ).c_str() );
                            }

                            if ( face < 0 ) {
                                sky->SetEquirectangular( texture );
                            }
                            else {
                                sky->SetFace( static_cast<material::SkyboxFace>( face ), texture );
                            }

                            changed = true;
                        }
                    } catch ( const std::exception& error ) {
                        mError = error.what();
                    }
                }
            } };

            if ( sky->GetType() == material::SkyboxType::eEquirectangular ) {
                choose( MKT_LOC( "material_editor_choose_panorama" ).c_str(), -1 );
            }
            else {
                const char* labels[]{ MKT_LOC( "material_editor_front" ).c_str(), MKT_LOC( "material_editor_back" ).c_str(), MKT_LOC( "material_editor_left" ).c_str(), MKT_LOC( "material_editor_right" ).c_str(), MKT_LOC( "material_editor_top" ).c_str(), MKT_LOC( "material_editor_bottom" ).c_str() };

                for ( core::i32 face{}; face < 6; ++face ) {
                    choose( labels[face], face );
                }
            }

            if ( !sky->HasRequiredTextures() ) {
                ImGui::TextWrapped( MKT_LOC( "material_editor_assign_all_required_images_to_use_this_skybox_in_a" ).c_str() );
            }
        }

        mPreviewDirty |= changed;
        mUnsaved |= changed;
    }
}// namespace mikoto::editor
