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

#include <memory>

#include <EASTL/sort.h>

// ImGui needs to go before ImGuizmo
#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Animation/AnimationSystem.hh>
#include <Animation/SkinningPalette.hh>
#include <Animation/Utility.hh>

#include <Assets/AssetRegistryService.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/Utility.hh>

#include <Core/Core.hh>
#include <Core/LocalizationService.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>
#include <Core/Types.hh>

#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/Utility.hh>

#include <Layers/EditorLayer.hh>

#include <Logging/Logger.hh>

#include <Material/SkyboxMaterial.hh>
#include <Material/SurfaceMaterial.hh>

#include <Math/Math.hh>

#include <Memory/Allocator.hh>

#include <Panels/ComponentMenu.hh>
#include <Panels/SceneControls.hh>
#include <Panels/ViewportPanel.hh>

#include <Physics/PhysicsWorld.hh>
#include <Physics/RayCast.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Scene/Component.hh>
#include <Scene/Entity.hh>

#include <Text/TextLayout.hh>
#include <Text/Utility.hh>

#include <Threading/TaskService.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;

    MKT_NODISCARD static auto IsViewportWidgetActive() -> bool {
        // An Inspector input releases focus later in the UI frame. It must not
        // consume the first click in the Scene viewport in the meantime.
        return ImGui::IsAnyItemActive() && ImGui::GetCurrentContext()->ActiveIdWindow == ImGui::GetCurrentWindow();
    }
    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    static auto BeginDropPropertyRow( core::cstr label ) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( label );
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth( -1.0f );
    }

    MKT_NODISCARD static auto GetGizmoType( imgui::GizmoType type ) -> ImGuizmo::OPERATION {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case imgui::GizmoType::eTranslation:
                return ImGuizmo::OPERATION::TRANSLATE;
            case imgui::GizmoType::eRotation:
                return ImGuizmo::OPERATION::ROTATE;
            case imgui::GizmoType::eScale:
                return ImGuizmo::OPERATION::SCALE;

            default:;
        }

        return ImGuizmo::OPERATION::TRANSLATE;
    }

    MKT_NODISCARD static auto GetGizmoMode( imgui::GizmoMode mode ) -> ImGuizmo::MODE {
        MKT_BEGIN_PROFILER();

        switch ( mode ) {
            case imgui::GizmoMode::eLocal:
                return ImGuizmo::MODE::LOCAL;
            case imgui::GizmoMode::eWorld:
                return ImGuizmo::MODE::WORLD;

            default:;
        }

        return ImGuizmo::MODE::LOCAL;
    }

    struct OrientationMarker {
        core::float3 mAxis{};
        ImVec2 mPosition{};
        ImU32 mColor{};
        core::cstr mLabel{};
        core::cstr mViewName{};
        core::f32 mDepth{};
        core::f32 mRadius{};
        core::usize mOrder{};
        bool mPositive{};
    };


    ViewportPanel::ViewportPanel( const ViewportPanelCreateInfo &createInfo )
        : Panel{ "Scene", "Panel", createInfo.mGroup, createInfo.mIsToggleable },
          mEditorState{ createInfo.mState },
          mInstanceId{ createInfo.mInstanceId },
          mGpuDevice{ createInfo.mGpuDevice } {
        const eastl::string icon{ imgui::GetStringFromUnicode( 58359 ) };
        mPanelHeaderName = imgui::widget::MakeIconTitle( icon.c_str(), mPanelName );
        InitOverlayIcons();
    }

    auto ViewportPanel::OnRender( float ts ) -> void {
        MKT_BEGIN_PROFILER();

        mContextReady = false;

        // Invalidate cached mesh bounds used to center the gizmo.
        if ( !mEditorState->mCenterPivot || mGizmoBoundsScene != mEditorState->mActiveScene || mGizmoBoundsRevision != mEditorState->GetRevision() ) {
            mGizmoMeshBounds.clear();
            mGizmoBoundsScene = mEditorState->mActiveScene;
            mGizmoBoundsRevision = mEditorState->GetRevision();
        }

        const bool maximized{ mEditorState->mScreenPresentTarget == ScreenPresentTarget::eScene };

        if ( !mPanelIsVisible && !maximized ) {
            SuspendInput();
            return;
        }

        ImGuiWindowFlags windowFlags{ ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse };

        if ( maximized ) {
            const auto &bounds{ mEditorState->mMaximizedViewport };
            ImGui::SetNextWindowPos( { bounds.mX, bounds.mY } );
            ImGui::SetNextWindowSize( { glm::max( 1.0f, bounds.mWidth ), glm::max( 1.0f, bounds.mHeight ) } );
            ImGui::SetNextWindowViewport( ImGui::GetMainViewport()->ID );
            ImGui::SetNextWindowDockID( 0 );
            windowFlags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
        }

        // Expand scene view to window bounds (no padding)
        imgui::ImGuiScopedStyleVar winPad( ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f } );

        if ( !imgui::BeginWindow( maximized ? "##MaximizedScene" : GetHeaderName().data(),
                 maximized ? nullptr : MKT_ADDRESSOF( mPanelIsVisible ), windowFlags ) ) {
            SuspendInput();
            ImGui::End();
            return;
        }

        if ( maximized && ImGui::IsWindowAppearing() ) {
            ImGui::SetWindowFocus();
        }

        // Using the window itself does not work for drag and drop, it does not
        // seem to be flagged as hovered when drag and drop is up

        // Save the original cursor position where ImGui was about to draw your UI layout.
        mStartCursorPos = ImGui::GetCursorPos();
        mStartWindowPos = ImGui::GetWindowPos();
        mPanelViewport = ImGui::GetWindowViewport();
        mStartWindowSize = ImGui::GetWindowSize();

        // Stretch an invisible element across the entire remaining content region of this window.
        // This acts as a "safety net" to catch mouse hover events over transparent areas.
        ImGui::Dummy( ImGui::GetContentRegionAvail() );

        if ( mEditorState->HasWorkspace() && !mEditorState->mRuntimeActive && ImGui::BeginDragDropTarget() ) {
            if ( const ImGuiPayload *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_MODEL", ImGuiDragDropFlags_AcceptNoDrawDefaultRect ) } ) {
                // ImGui owns a copy of the UTF-8 payload, never a shallow-copied Path.
                filesystem::Path path{};

                if ( payload->Data && payload->DataSize > 1 && core::as<core::cstr>( payload->Data )[payload->DataSize - 1] == '\0' ) {
                    path = filesystem::Path{ eastl::string_view{ core::as<core::cstr>( payload->Data ), core::as<core::usize>( payload->DataSize - 1 ) } };
                }

                if ( !path.IsEmpty() ) {
                    // Scene ownership begins immediately. The streaming request belongs
                    // to the root entity, so deleting it cancels its demand naturally.
                    const auto reference{ asset::AssetRef<asset::Model>{ path } };

                    scene::EntityCreateInfo info{};

                    info.mName = path.GetPathTyped<std::filesystem::path>().filename().string().c_str();
                    info.mModelAsset = reference;

                    mEditorState->mActiveScene->PushEntity( info );
                }

                core::RuntimeConsole::Get()->Debug( "You dropped a model from the content browser" );
            }

            // Scene/project payloads carry owned UTF-8 paths. Route opening
            // through EditorLayer so Save/Discard/Cancel retains the destination.
            const ImGuiPayload *scenePayload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_SCENE", ImGuiDragDropFlags_AcceptNoDrawDefaultRect ) };
            const ImGuiPayload *projectPayload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_PROJECT", ImGuiDragDropFlags_AcceptNoDrawDefaultRect ) };
            const ImGuiPayload *workspacePayload{ scenePayload ? scenePayload : projectPayload };

            if ( workspacePayload && workspacePayload->Data && workspacePayload->DataSize > 1 &&
                 core::as<core::cstr>( workspacePayload->Data )[workspacePayload->DataSize - 1] == '\0' ) {
                const filesystem::Path path{ eastl::string_view{ core::as<core::cstr>( workspacePayload->Data ), core::as<core::usize>( workspacePayload->DataSize - 1 ) } };

                if ( scenePayload && mEditorState->mOpenScene ) {
                    mEditorState->mOpenScene( path );
                }
                else if ( projectPayload && mEditorState->mOpenProject ) {
                    mEditorState->mOpenProject( path );
                }
            }

            ImGui::EndDragDropTarget();
        }

        // Restore the cursor position back to the top-left starting point.
        // This ensures any overlay buttons, gizmos, or transformation tools drawn below
        // will render directly on top of the drop zone without being pushed down.
        ImGui::SetCursorPos( mStartCursorPos );

        mPanelIsFocused = ImGui::IsWindowFocused();
        mPanelIsHovered = ImGui::IsWindowHovered();

        ImVec2 viewportMin{ ImGui::GetItemRectMin() };
        ImVec2 viewportSize{ ImGui::GetContentRegionAvail() };

        mViewport = ViewportInfo{
            .mX = viewportMin.x,
            .mY = viewportMin.y,
            .mWidth = viewportSize.x,
            .mHeight = viewportSize.y
        };

        mContentViewport = mViewport;

        if ( mEditorState->mTerrainTool.mEnabled && mCursorTool != CursorTool::eLandscape ) {
            SetCursorTool( CursorTool::eLandscape );
        }
        if ( !mEditorState->mTerrainTool.mEnabled && mCursorTool == CursorTool::eLandscape ) {
            SetCursorTool( CursorTool::eSelect );
        }

        DrawFinalImage();

        mPickingBlockedByOverlay = false;

        if ( mEditorState->mViewSettings.mShowViewportControls ) {
            DrawSceneButtonsOverlay();
        }

        DrawViewSettings();

        // Hovering the toolbar or an overlay must not start viewport input.
        mPanelIsHovered = mSceneImageHovered && !mPickingBlockedByOverlay;

        if ( !mEditorState->mEditorRenderer || !mEditorState->mEditorRenderer->IsRenderAvailable() ) {
            SuspendInput();
            ImGui::SetCursorPos( { mStartCursorPos.x + 16.0f, mStartCursorPos.y + 16.0f } );
            ImGui::TextWrapped( MKT_LOC( "viewport_scene_rendering_is_unavailable_with_this_graphics_backend_you_can" ).c_str() );
            ImGui::End();
            return;
        }

        // ImGui disabled widgets do not block raw mouse or ImGuizmo input.
        if ( !mEditorState->HasWorkspace() ) {
            SuspendInput();
            ImGui::End();
            return;
        }

        ImGui::SetCursorPos( mStartCursorPos );

        mEditorState->mEditorRenderer->SetInfoOverlayEnabled( mEnablePerformanceOverlay );

        if ( mEnablePerformanceOverlay && !mEditorState->mEditorRenderer->HasFrameGraph() ) {
            DrawPerformanceOverlay( false );
        }

        if ( mEnableUtilityOverlay ) {
            DrawUtilitiesOverlay();
        }

        if ( !IsDisplayTextureValid() ) {
            SuspendInput();
            ImGui::End();
            return;
        }

        ApplyPickingResult();
        DrawSceneAnnotations();

        if ( mEditorState->mViewSettings.mShowGizmos &&
             ( !mEditorState->mRuntimeActive || mEditorState->mViewSettings.mShowGizmosInPlayMode ) ) {
            DrawOrientationAxis();

            if ( mCursorTool == CursorTool::eSelect ) {
                UpdateManipulation();
                DrawManipulationGizmos();
            }
        }

        UpdateSceneContextMenu();
        OnEntityRightClickMenu();
        BlankSpacePopupMenu();

        UpdateCursorTool();

        if ( mCursorTool != CursorTool::eGrabSoftBody ) {
            DrawSoftBodyVertices( entt::null, 0 );
        }

        UpdateTextureDrop();
        UpdateMaterialDrop();

        // Utility/orientation overlays may have blocked input after the image
        // was drawn. Publish their final state for the next platform event.
        mPanelIsHovered = mSceneImageHovered && !mPickingBlockedByOverlay;

        ImGui::End();
    }

    auto ViewportPanel::BlankSpacePopupMenu() -> void {
        MKT_BEGIN_PROFILER();

        ImGui::SetNextWindowPos( mContextPopupPosition, ImGuiCond_Appearing );
        imgui::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        imgui::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::BeginPopup( "##ViewportPanelBackgroundMenu" ) ) {
            if ( !mEditorState->mActiveScene || mContextScene != mEditorState->mActiveScene ) {
                ImGui::CloseCurrentPopup();
            }
            else if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "viewport_create_empty_object", "Create empty object" ).c_str() ) ) {
                mEditorState->mActiveScene->PushEntity( scene::EntityCreateInfo{
                    .mName = MKT_LOC( "viewport_empty_object" ).c_str(),
                    .mEntityType = scene::EntityType::eEmpty } );
            }
            else if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "viewport_3d_object", "3D Object" ).c_str() ) ) {
                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "viewport_cube", "Cube" ).c_str() ) ) {
                    AddEntityWithModel( mEditorState->GetPrefabReference( PrefabModelType::eCube ), nullptr );
                }

                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "viewport_cone", "Cone" ).c_str() ) ) {
                    AddEntityWithModel( mEditorState->GetPrefabReference( PrefabModelType::eCone ), nullptr );
                }

                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "viewport_cylinder", "Cylinder" ).c_str() ) ) {
                    AddEntityWithModel( mEditorState->GetPrefabReference( PrefabModelType::eCylinder ), nullptr );
                }

                if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "viewport_sphere", "Sphere" ).c_str() ) ) {
                    AddEntityWithModel( mEditorState->GetPrefabReference( PrefabModelType::eSphere ), nullptr );
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    }

    auto ViewportPanel::AddEntityWithModel( const asset::AssetRef<asset::Model> &model, scene::Entity *root ) -> void {
        MKT_BEGIN_PROFILER();

        mEditorState->mActiveScene->PushEntity( {
            .mRoot = root,
            .mName = eastl::string{ model.GetPath().GetStem() },
            .mModelAsset = model,
            .mEntityType = scene::EntityType::eMesh,
        } );
    }

    auto ViewportPanel::OnEntityRightClickMenu() -> void {
        MKT_BEGIN_PROFILER();

        ImGui::SetNextWindowPos( mContextPopupPosition, ImGuiCond_Appearing );

        imgui::ImGuiScopedStyleVar popupBorder{ ImGuiStyleVar_PopupBorderSize, 1.0f };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 11.0f, 11.0f } };
        imgui::ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 12.0f, 12.0f } };

        if ( ImGui::BeginPopup( "##ViewportPanelEntityMenu" ) ) {
            auto *entity{ mEditorState->mActiveScene && mContextScene == mEditorState->mActiveScene
                              ? mEditorState->mActiveScene->FindByID( mContextEntityID )
                              : nullptr };

            if ( !entity ) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

            if ( ImGui::BeginMenu( imgui::widget::LocalizedLabel( "viewport_add_component", "Add component" ).c_str() ) ) {
                DrawAddComponentMenu( entity );
                ImGui::EndMenu();
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "viewport_remove_object", "Remove object" ).c_str() ) ) {
                mEditorState->mActiveScene->RemoveEntity( entity->GetComponent<scene::TagComponent>().GetGuid() );
                mEditorState->SelectEntity( nullptr );
            }

            else if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "viewport_create_empty_child", "Create empty child" ).c_str() ) ) {
                scene::EntityCreateInfo createInfo{
                    .mRoot = entity,
                    .mName = MKT_LOC( "viewport_empty_object" ).c_str(),
                    .mEntityType = scene::EntityType::eEmpty,
                };

                mEditorState->mActiveScene->PushEntity( createInfo );
            }

            // DrawPrefabMenu( entity );
            // DrawCanvasMenu( entity );
            // DrawInterfaceElements( entity );
            // DrawModelLoadMenu( entity );
            // DrawLightMenuItems( entity );
            // DrawTextMenu( entity );

            ImGui::EndPopup();
        }
    }

    ViewportPanel::~ViewportPanel() {
        ReleaseSoftBodyGrab();
    }

    auto ViewportPanel::SuspendInput() -> void {
        ReleaseTerrainStroke();
        ReleaseSoftBodyGrab();

        mPanelIsHovered = false;
        mPanelIsFocused = false;
        mSceneImageHovered = false;
        mRightMouseGesture = RightMouseGesture::eIdle;
    }

    auto ViewportPanel::IsCameraNavigating() const -> bool {
        return mRightMouseGesture == RightMouseGesture::eNavigate;
    }

    auto ViewportPanel::SetGizmoType( imgui::GizmoType type ) -> void {
        MKT_BEGIN_PROFILER();
        SetCursorTool( CursorTool::eSelect );
        mGizmoType = type;
    }

    auto ViewportPanel::SetGizmoMode( imgui::GizmoMode mode ) -> void {
        MKT_BEGIN_PROFILER();
        mGizmoMode = mode;
    }

    auto ViewportPanel::SetTexture( renderer::rhi::TextureHandle texture ) -> void {
        MKT_BEGIN_PROFILER();
        mColorImage = texture;
    }

    auto ViewportPanel::SetEnableUtilityOverlay( bool enable ) -> void {
        MKT_BEGIN_PROFILER();
        mEnableUtilityOverlay = enable;
    }

    auto ViewportPanel::SetEnablePerformanceOverlay( bool enable ) -> void {
        MKT_BEGIN_PROFILER();
        mEnablePerformanceOverlay = enable;
    }

    auto ViewportPanel::GetWidth() const -> float {
        MKT_BEGIN_PROFILER();
        return mViewportWidth;
    }

    auto ViewportPanel::GetHeight() const -> float {
        MKT_BEGIN_PROFILER();
        return mViewportHeight;
    }

    auto ViewportPanel::DrawPerformanceOverlay( bool showOnlyFps ) -> void {
        MKT_BEGIN_PROFILER();

        ImGuiWindowFlags overlayFlags{
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove
        };

        ImVec2 windowPos{};
        ImVec2 windowPosPivot{};
        windowPos.x = mViewport.mX + mViewport.mWidth - 12.0f;
        windowPos.y = mViewport.mY + mViewport.mHeight - 12.0f;
        windowPosPivot.x = 1.0f;
        windowPosPivot.y = 1.0f;

        ImGui::SetNextWindowBgAlpha( 0.35f );

        // Pivot defines which part of the window (main viewport) aligns with 'windowPos'.
        // ImVec2(X, Y) ranges from 0.0f (left/top) to 1.0f (right/bottom):
        //   - ImVec2(0.0f, 0.0f) : Top-left corner (Default behavior)
        //   - ImVec2(0.5f, 0.5f) : Exact center of the window (Great for centered popups)
        //   - ImVec2(1.0f, 1.0f) : Bottom-right corner (Great for bottom-right corner overlays)
        ImGui::SetNextWindowPos( windowPos, ImGuiCond_Always, windowPosPivot );
        ImGui::SetNextWindowSize( ImVec2{ 0.f, 0.f }, ImGuiCond_Always );

        ImGui::SetNextWindowViewport( mPanelViewport->ID );

        imgui::ImGuiScopedStyleVar innerPadding( ImGuiStyleVar_WindowPadding, ImVec2( 10.0f, 10.0f ) );
        imgui::ImGuiScopedStyleVar windowRounding( ImGuiStyleVar_WindowRounding, 2.0f );
        imgui::ImGuiScopedStyleVar windowBorderSize( ImGuiStyleVar_WindowBorderSize, 0.0f );

        const eastl::string nodeID{ string::Format( "##ViewportPanelPerformanceOverlay" ) };

        if ( imgui::BeginWindow( nodeID.c_str(), nullptr, overlayFlags ) ) {
            ImGui::Text( "%.1f FPS (%.1f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate );

            if ( !showOnlyFps && mEditorState->mEditorRenderer ) {
                const auto statistics{ mEditorState->mEditorRenderer->GetStatistics() };
                const auto &commands{ statistics.mCommands };
                ImGui::Text( "GPU: %.*s", core::as<int>( statistics.mGraphicsDeviceName.size() ), statistics.mGraphicsDeviceName.data() );

                if ( const auto *system{ core::SystemStats::GetPtr() } ) {
                    const auto cpu{ system->GetCpuName() };
                    ImGui::Text( "CPU: %.*s", core::as<int>( cpu.size() ), cpu.data() );
                }

                if ( const auto *time{ core::TimeService::GetPtr() } ) {
                    ImGui::Text( "Budget: %.2f ms", time->GetFrameBudget() );
                }

                ImGui::Text( "Draw calls: %llu", commands.mDrawCallCount );
                ImGui::Text( "Dispatches: %llu", commands.mDispatchCallCount );
                ImGui::Text( "Pipeline binds: %llu", commands.mPipelineBindCount );
                ImGui::Text( "Image barriers: %llu", commands.mBarriers.mImageCount );
                ImGui::Text( "Buffer barriers: %llu", commands.mBarriers.mBufferCount );
                ImGui::Text( "Memory barriers: %llu", commands.mBarriers.mMemoryCount );
                ImGui::Text( "Barrier submissions (batches): %llu", commands.mBarriers.mCallCount );
            }
        }

        ImGui::End();
    }

    auto ViewportPanel::IsDisplayTextureValid() const -> bool {
        MKT_BEGIN_PROFILER();
        return !mColorImage.IsEmpty();
    }

    auto ViewportPanel::DrawFinalImage() -> void {
        MKT_BEGIN_PROFILER();

        mSceneImageHovered = false;
        const ImVec2 available{ ImGui::GetContentRegionAvail() };
        ImVec2 dim{ available };

        if ( mAspectRatio > 0.0f && available.x > 0.0f && available.y > 0.0f ) {
            dim.y = glm::min( available.y, available.x / mAspectRatio );
            dim.x = dim.y * mAspectRatio;
            const ImVec2 position{ ImGui::GetCursorScreenPos() };
            ImGui::GetWindowDrawList()->AddRectFilled( position,
                { position.x + available.x, position.y + available.y }, IM_COL32( 0, 0, 0, 255 ) );
        }

        const ImVec2 origin{ ImGui::GetCursorScreenPos() };
        const ImVec2 imageOrigin{ origin.x + ( available.x - dim.x ) * 0.5f, origin.y + ( available.y - dim.y ) * 0.5f };

        ImGui::SetCursorScreenPos( imageOrigin );

        mViewport = ViewportInfo{ .mX = imageOrigin.x, .mY = imageOrigin.y, .mWidth = dim.x, .mHeight = dim.y };
        const bool hasImageArea{ dim.x > 0.0f && dim.y > 0.0f };

        if ( hasImageArea ) {
            // Request the first frame even if its output texture is not ready yet.
            mEditorState->mScenePreviewRequested = true;
            mViewportWidth = dim.x;
            mViewportHeight = dim.y;

            // Before the first panel layout these dimensions are only defaults.
            // Use the actual image area for both gizmos and the scene rendered
            // later this frame. Hidden or collapsed panels retain the last size.
            if ( mEditorState->mActiveCamera ) {
                mEditorState->mActiveCamera->SetViewportSize( dim.x, dim.y );
            }
        }

        if ( hasImageArea && !IsDisplayTextureValid() ) {
            const auto message{ MKT_LOC( "viewport_preparing_scene" ) };
            const ImVec2 textSize{ ImGui::CalcTextSize( message.c_str() ) };
            auto *draw{ ImGui::GetWindowDrawList() };
            draw->AddRectFilled( imageOrigin, { imageOrigin.x + dim.x, imageOrigin.y + dim.y }, IM_COL32( 28, 28, 30, 255 ) );
            draw->AddText( { imageOrigin.x + glm::max( 0.0f, ( dim.x - textSize.x ) * .5f ),
                               imageOrigin.y + glm::max( 0.0f, ( dim.y - textSize.y ) * .5f ) },
                ImGui::GetColorU32( ImGuiCol_Text ), message.c_str() );
            // Retain the measured image rectangle and render request, but no
            // hit target or scene interaction until a matching frame arrives.
            ImGui::Dummy( dim );
            return;
        }

        // No flipping, the final image is already in the correct viewport coordinates
        // In the case of vulkan this is also taken into account when setting up the viewport
        if ( hasImageArea && IsDisplayTextureValid() ) {
            // ImGui bindings are tracked per frame, even when scene rendering
            // pauses for a save/load operation. Retain the texture, not its ID.
            const ImTextureID imageID{ imgui::ImGuiService::Get()->GetTextureID( mColorImage ) };

            if ( imageID == 0 ) {
                ImGui::Dummy( dim );
                return;
            }

            ImGui::Image( imageID, ImVec2{ dim.x, dim.y } );
            // The brush owns an invisible item over this image while dragging.
            // Its active ID must not hide the image's hover state on later frames.
            mSceneImageHovered = ImGui::IsItemHovered( mCursorTool == CursorTool::eLandscape ? ImGuiHoveredFlags_AllowWhenBlockedByActiveItem : ImGuiHoveredFlags_None );
            const auto min = ImGui::GetItemRectMin();
            const auto max = ImGui::GetItemRectMax();
            mViewport = ViewportInfo{ .mX = min.x, .mY = min.y, .mWidth = max.x - min.x, .mHeight = max.y - min.y };
            DrawCameraFrustum();
        }
    }

    auto ViewportPanel::DrawCameraFrustum() -> void {
        if ( !mEditorState->mGameCamera || !mEditorState->mActiveCamera ||
             !mEditorState->mEditorRenderer->GetSettings().mVisualizeFrustum ) {
            return;
        }
        // Project the main camera's eight clip-volume corners into the Scene camera.
        // Clip each edge before dividing by W, including when the observer is inside it.
        const auto transform{ mEditorState->mActiveCamera->GetViewProjection() *
                              glm::inverse( mEditorState->mGameCamera->GetViewProjection() ) };
        core::float4 corners[8]{};

        for ( core::u32 index{}; index < 8; ++index ) {
            corners[index] = transform * core::float4{ ( index & 1 ) ? 1.0f : -1.0f,
                ( index & 2 ) ? 1.0f : -1.0f, ( index & 4 ) ? 1.0f : 0.0f, 1.0f };
        }

        constexpr core::u32 edges[12][2]{ { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 }, { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
        constexpr core::float4 planes[6]{ { 1, 0, 0, 1 }, { -1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, -1, 0, 1 }, { 0, 0, 1, 0 }, { 0, 0, -1, 1 } };

        auto *draw{ ImGui::GetWindowDrawList() };
        draw->PushClipRect( { mViewport.mX, mViewport.mY },
            { mViewport.mX + mViewport.mWidth, mViewport.mY + mViewport.mHeight }, true );

        for ( const auto &edge: edges ) {
            core::float4 a{ corners[edge[0]] };
            core::float4 b{ corners[edge[1]] };
            bool visible{ true };

            for ( const auto &plane: planes ) {
                const float da{ glm::dot( a, plane ) };
                const float db{ glm::dot( b, plane ) };

                if ( ( da < 0.0f && db < 0.0f ) ) {
                    visible = false;
                    break;
                }

                if ( ( da < 0.0f ) != ( db < 0.0f ) ) {
                    const core::float4 intersection{ glm::mix( a, b, da / ( da - db ) ) };

                    if ( da < 0.0f ) {
                        a = intersection;
                    }
                    else {
                        b = intersection;
                    }
                }
            }

            if ( !visible || a.w <= 1e-7f || b.w <= 1e-7f ) {
                continue;
            }

            const auto screen = [this]( const core::float4 &clip ) -> ImVec2 {
                return { mViewport.mX + ( clip.x / clip.w * 0.5f + 0.5f ) * mViewport.mWidth,
                    mViewport.mY + ( 0.5f - clip.y / clip.w * 0.5f ) * mViewport.mHeight };
            };
            draw->AddLine( screen( a ), screen( b ), IM_COL32( 80, 215, 255, 255 ), 2.0f );
        }

        draw->PopClipRect();
    }

    auto ViewportPanel::CancelPicking() -> void {
        mBoxActive = false;
        mBoxScene = nullptr;
        mBoxDragged = mPickRectangle = mPickAppend = false;

        mTexturePickPending = mPickForTexture = false;
        mMaterialPickPending = mPickForMaterial = mMaterialDelivered = false;

        if ( mEditorState->mEditorRenderer ) {
            mEditorState->mEditorRenderer->SetTextureDropHover( 0 );
        }

        mPickScene = nullptr;
        mPickForContext = false;
        mContextReady = false;
        mRightMouseGesture = RightMouseGesture::eIdle;

        if ( mEditorState->mEditorRenderer ) {
            mEditorState->mEditorRenderer->CancelPick();
        }
    }

    auto ViewportPanel::ApplyPickingResult() -> void {
        // Apply a GPU picking result only if it is still relevant.
        if ( mEditorState->mEditorRenderer ) {
            if ( auto result{ mEditorState->mEditorRenderer->GetPickResult() };
                result &&
                mEditorState->mActiveScene &&
                mPickScene == mEditorState->mActiveScene &&
                mPickRevision == mEditorState->GetRevision() ) {
                auto *selected{ *result ? mEditorState->mActiveScene->FindByID( *result ) : nullptr };
                if ( mPickForMaterial ) {
                    mMaterialPickPending = mPickForMaterial = false;
                    mMaterialTargetID = selected && selected->HasComponent<scene::MeshComponent>() ? *result : 0;
                    const auto *payload{ ImGui::GetDragDropPayload() };
                    const bool dragging{ payload && payload->IsDataType( "CONTENT_BROWSER_TEXT_MATERIAL" ) };
                    mEditorState->mEditorRenderer->SetTextureDropHover( !mMaterialDelivered && dragging ? mMaterialTargetID : 0 );
                    if ( mMaterialDelivered ) {
                        mMaterialDelivered = false;
                        mOpenMaterialPopup = true;
                    }
                    return;
                }

                if ( mPickForTexture ) {
                    mTexturePickPending = false;
                    mPickForTexture = false;
                    mTextureTargetID = selected && selected->HasComponent<scene::MeshComponent>() ? *result : 0;
                    mEditorState->mEditorRenderer->SetTextureDropHover( mTextureDelivered ? 0 : mTextureTargetID );
                    if ( mTextureDelivered ) {
                        mOpenTexturePopup = true;
                    }
                    return;
                }

                if ( mPickRectangle ) {
                    mPickRectangle = false;

                    // Only visible ID pixels contribute; projected origins bypass depth testing.
                    const auto ids{ mEditorState->mEditorRenderer->ReadSelectionRectangle( mPendingRectangleFirst, mPendingRectangleLast ) };
                    eastl::vector<core::u64> selection{};
                    for ( const auto id: ids ) {
                        auto *entity{ mEditorState->mActiveScene->FindByID( id ) };
                        if ( mEditorState->mViewSettings.mSelectRoot ) {
                            while ( entity && entity->GetComponent<scene::RelationComponent>().GetParent() ) {
                                entity = mEditorState->mActiveScene->FindByID( *entity->GetComponent<scene::RelationComponent>().GetParent() );
                            }
                        }

                        if ( entity ) {
                            selection.push_back( entity->GetComponent<scene::TagComponent>().GetGuid() );
                        }
                    }

                    mEditorState->SelectEntities( selection, mPickAppend );
                    return;
                }


                if ( mPickParent && selected ) {
                    // Double-click promotes the hit mesh by one hierarchy level.
                    // Keep roots selected when there is no parent to resolve.
                    const auto parentID{ selected->GetComponent<scene::RelationComponent>().GetParent() };

                    if ( parentID ) {
                        if ( auto *parent{ mEditorState->mActiveScene->FindByID( *parentID ) } ) {
                            selected = parent;
                        }
                    }
                }
                else if ( mEditorState->mViewSettings.mSelectRoot ) {
                    while ( selected && selected->GetComponent<scene::RelationComponent>().GetParent() ) {
                        auto *parent{ mEditorState->mActiveScene->FindByID( *selected->GetComponent<scene::RelationComponent>().GetParent() ) };

                        if ( !parent || parent == selected ) {
                            break;
                        }

                        selected = parent;
                    }
                }

                if ( mPickAppend && !mPickForContext ) {
                    mEditorState->SelectEntities( selected ? eastl::vector<core::u64>{ selected->GetComponent<scene::TagComponent>().GetGuid() } : eastl::vector<core::u64>{}, true );
                }
                else {
                    mEditorState->SelectEntity( selected );
                }

                if ( mPickForContext ) {
                    mContextEntityID = selected ? selected->GetComponent<scene::TagComponent>().GetGuid() : 0;
                    mContextScene = mEditorState->mActiveScene;
                    mContextReady = true;
                }
            }
        }
    }

    auto ViewportPanel::SetCursorTool( CursorTool tool ) -> void {
        if ( mCursorTool == tool ) {
            return;
        }

        ReleaseSoftBodyGrab();
        CancelPicking();

        mBoxOpacity = 0.0f;
        if ( mCursorTool == CursorTool::eLandscape ) {
            ReleaseTerrainStroke();
        }
        if ( tool == CursorTool::eLandscape && mEditorState->mTerrainTool.mBrush.mMode == TerrainEditMode::eGenerate ) {
            mEditorState->mTerrainTool.mBrush.mMode = TerrainEditMode::eSculpt;
        }
        mEditorState->mTerrainTool.mEnabled = tool == CursorTool::eLandscape;
        mCursorTool = tool;
    }

    auto ViewportPanel::UpdateCursorTool() -> void {
        switch ( mCursorTool ) {
            case CursorTool::eSelect:
                UpdatePicking();
                break;
            case CursorTool::eGrabSoftBody:
                UpdateSoftBodyTool();
                break;
            case CursorTool::eLandscape:
                UpdateTerrainTool();
                break;
        }
    }

    auto ViewportPanel::ReleaseTerrainStroke() -> void {
        auto &tool{ mEditorState->mTerrainTool };
        if ( const auto *entity{ tool.mScene ? tool.mScene->FindByID( tool.mEntity ) : nullptr };
            entity && entity->HasComponent<scene::LandscapeComponent>() ) {
            tool.EndStroke( entity->GetComponent<scene::LandscapeComponent>() );
        }
        if ( mTerrainBrushInputID && ImGui::GetActiveID() == mTerrainBrushInputID ) {
            ImGui::ClearActiveID();
        }
    }

    auto ViewportPanel::UpdateTerrainTool() -> void {
        auto &tool{ mEditorState->mTerrainTool };
        auto *scene{ mEditorState->mActiveScene };
        auto *entity{ mEditorState->mSelectedEntity };
        const auto *camera{ mEditorState->mActiveCamera };
        if ( !scene || !entity || !entity->HasComponent<scene::LandscapeComponent>() || !camera || scene->IsPlaying() || scene->IsSimulating() ) {
            ReleaseTerrainStroke();
            tool.SetTarget( nullptr, 0 );
            return;
        }
        auto &landscape{ entity->GetComponent<scene::LandscapeComponent>() };
        tool.SetTarget( scene, entity->GetComponent<scene::TagComponent>().GetGuid() );
        const bool released{ !ImGui::IsMouseDown( ImGuiMouseButton_Left ) || ImGui::GetIO().AppFocusLost };
        const bool navigating{ ImGui::IsMouseDown( ImGuiMouseButton_Right ) || ImGui::IsMouseDown( ImGuiMouseButton_Middle ) || ImGui::GetIO().KeyAlt };
        if ( released || navigating ) {
            ReleaseTerrainStroke();
        }
        if ( ImGui::IsKeyPressed( ImGuiKey_Escape ) ) {
            SetCursorTool( CursorTool::eSelect );
            return;
        }
        if ( ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput ) {
            if ( ImGui::IsKeyPressed( ImGuiKey_Z ) ) {
                ReleaseTerrainStroke();
                tool.Undo( landscape );
                return;
            }
            if ( ImGui::IsKeyPressed( ImGuiKey_Y ) ) {
                ReleaseTerrainStroke();
                tool.Redo( landscape );
                return;
            }
        }
        mTerrainBrushInputID = ImGui::GetID( "##TerrainBrushInput" );
        const bool ownsInput{ ImGui::GetActiveID() == mTerrainBrushInputID };
        const bool available{ mSceneImageHovered && !mPickingBlockedByOverlay &&
                              ImGui::IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem ) &&
                              ( !IsViewportWidgetActive() || ownsInput ) && !ImGui::GetDragDropPayload() &&
                              !navigating && !ImGui::GetIO().AppFocusLost };
        if ( ( !available && !ownsInput ) || mViewport.mWidth <= 0 || mViewport.mHeight <= 0 ) {
            return;
        }

        // Claim the gesture only after toolbar/overlay hit tests. A bare Image
        // leaves LMB to ImGui's window-move ID, which blocked every held frame.
        const auto cursorPosition{ ImGui::GetCursorPos() };
        ImGui::SetCursorScreenPos( { mViewport.mX, mViewport.mY } );
        ImGui::InvisibleButton( "##TerrainBrushInput", { mViewport.mWidth, mViewport.mHeight }, ImGuiButtonFlags_MouseButtonLeft );
        const bool brushActive{ ImGui::IsItemActive() };
        ImGui::SetCursorPos( cursorPosition );
        ImGui::Dummy( { 0, 0 } );
        if ( !available ) {
            return;
        }
        const auto transform{ entity->GetComponent<scene::TransformComponent>().GetWorldTransform() };
        if ( glm::abs( glm::determinant( transform ) ) < 1e-10f ) {
            return;
        }
        const auto inverse{ glm::inverse( transform ) };
        const auto mouse{ ImGui::GetMousePos() };
        const float2 cursor{ mouse.x, mViewport.mY + mViewport.mHeight - ( mouse.y - mViewport.mY ) };
        const float4 viewport{ mViewport.mX, mViewport.mY, mViewport.mWidth, mViewport.mHeight };
        auto a{ glm::unProjectZO( float3{ cursor, 0 }, camera->GetViewMatrix(), camera->GetProjection(), viewport ) };
        auto b{ glm::unProjectZO( float3{ cursor, 1 }, camera->GetViewMatrix(), camera->GetProjection(), viewport ) };
        if ( glm::length( a - camera->GetPosition() ) > glm::length( b - camera->GetPosition() ) ) {
            eastl::swap( a, b );
        }
        const float3 origin{ inverse * float4{ a, 1 } };
        const float3 direction{ inverse * float4{ b - a, 0 } };
        float3 local{};
        const auto world{ landscape.GetWorldDescription() };
        if ( !renderer::IntersectTerrain( world, landscape.GetHeightmap(), origin, direction, local ) ) {
            return;
        }
        const float3 center{ transform * float4{ local, 1 } };
        const float2 uv{ ( float2{ local.x, local.z } - world.mOrigin ) / world.GetWorldSize() };
        const float normalizedHeight{ landscape.GetHeightmap() ? landscape.GetHeightmap()->Sample( uv ) : 0 };
        ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeAll );
        if ( brushActive && !tool.mStrokeActive ) {
            tool.BeginStroke( landscape, normalizedHeight );
        }
        if ( tool.mStrokeActive && brushActive && !released ) {
            ApplyTerrainBrush( landscape, transform, tool.mBrush, center, ImGui::GetIO().DeltaTime,
                ImGui::GetIO().KeyShift, tool.mFlattenHeight );
        }
        const auto viewProjection{ camera->GetProjection() * camera->GetViewMatrix() };
        const auto point{ [&]( float angle, float radius, ImVec2 &screen ) {
            const float3 worldPoint{ center + float3{ std::cos( angle ) * radius, 0, std::sin( angle ) * radius } };
            auto terrainPoint{ float3{ inverse * float4{ worldPoint, 1 } } };
            const float2 coordinates{ ( float2{ terrainPoint.x, terrainPoint.z } - world.mOrigin ) / world.GetWorldSize() };
            if ( coordinates.x < 0 || coordinates.y < 0 || coordinates.x > 1 || coordinates.y > 1 ) {
                return false;
            }
            terrainPoint.y = world.mBaseElevation + ( landscape.GetHeightmap() ? landscape.GetHeightmap()->Sample( coordinates ) : 0 ) * world.mHeightScale;
            const float4 clip{ viewProjection * transform * float4{ terrainPoint, 1 } };
            if ( clip.w <= .0001f ) {
                return false;
            }
            const auto ndc{ float3{ clip } / clip.w };
            screen = { mViewport.mX + ( ndc.x * .5f + .5f ) * mViewport.mWidth,
                mViewport.mY + ( .5f - ndc.y * .5f ) * mViewport.mHeight };
            return true;
        } };
        auto *draw{ ImGui::GetWindowDrawList() };
        draw->PushClipRect( { mViewport.mX, mViewport.mY }, { mViewport.mX + mViewport.mWidth, mViewport.mY + mViewport.mHeight }, true );
        for ( int ring{}; ring < 2; ++ring ) {
            const float radius{ tool.mBrush.mRadius * ( ring == 0 ? 1.0f : 1.0f - tool.mBrush.mFalloff ) };
            for ( int segment{}; segment < 96 && radius > .001f; ++segment ) {
                ImVec2 start{};
                ImVec2 end{};
                if ( point( segment * 6.2831853f / 96, radius, start ) && point( ( segment + 1 ) * 6.2831853f / 96, radius, end ) ) {
                    draw->AddLine( start, end, IM_COL32( 0, 0, 0, 180 ), 3.0f );
                    draw->AddLine( start, end, ring == 0 ? IM_COL32( 255, 206, 91, 255 ) : IM_COL32( 255, 206, 91, 140 ), 1.4f );
                }
            }
        }
        draw->PopClipRect();
    }

    auto ViewportPanel::ReleaseSoftBodyGrab() -> void {
        if ( !mSoftBodyDrag.mScene ) {
            return;
        }

        if ( auto *world{ mSoftBodyDrag.mScene->GetPhysicsWorld() } ) {
            world->EndSoftBodyGrab();
        }

        mSoftBodyDrag = {};
    }

    auto ViewportPanel::UpdateSoftBodyTool() -> void {
        auto *scene{ mEditorState->mActiveScene };
        const auto *camera{ mEditorState->mActiveCamera };

        if ( !scene || !scene->GetPhysicsWorld() || !camera || mViewport.mWidth <= 0.0f || mViewport.mHeight <= 0.0f ) {
            ReleaseSoftBodyGrab();
            return;
        }

        auto &world{ *scene->GetPhysicsWorld() };
        const bool simulating{ scene->IsPlaying() || scene->IsSimulating() };
        const bool cancel{ ImGui::GetIO().AppFocusLost || ImGui::IsKeyPressed( ImGuiKey_Escape ) ||
                           ImGui::IsMouseDown( ImGuiMouseButton_Right ) || ImGui::IsMouseDown( ImGuiMouseButton_Middle ) };

        if ( mSoftBodyDrag.mScene &&
             ( mSoftBodyDrag.mScene != scene || !simulating || cancel || !world.IsSoftBodyGrabActive() ||
                 !ImGui::IsMouseDown( ImGuiMouseButton_Left ) || !ImGui::IsMousePosValid() ) ) {
            ReleaseSoftBodyGrab();
        }

        const auto mouse{ ImGui::GetMousePos() };
        const auto viewProjection{ camera->GetViewProjection() };
        const core::float4 image{ mViewport.mX, mViewport.mY, mViewport.mWidth, mViewport.mHeight };
        const core::float2 cursor{ mouse.x, mViewport.mY + mViewport.mHeight - ( mouse.y - mViewport.mY ) };
        const auto unproject = [&]( core::f32 depth ) {
            return glm::unProjectZO( core::float3{ cursor, depth }, camera->GetViewMatrix(), camera->GetProjection(), image );
        };

        physics::SoftBodyHit hit{};
        const bool available{ mSceneImageHovered && !mPickingBlockedByOverlay && ImGui::IsWindowHovered() &&
                              !IsViewportWidgetActive() && !ImGui::GetIO().KeyAlt && !ImGui::GetDragDropPayload() && !cancel };

        if ( !mSoftBodyDrag.mScene && available ) {
            const auto nearPoint{ unproject( 0.0f ) };
            const auto farPoint{ unproject( 1.0f ) };
            hit = world.PickSoftBody( physics::RayCast{ nearPoint, farPoint - nearPoint } );

            if ( hit.mEntity != entt::null && simulating ) {
                ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );

                if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) && world.BeginSoftBodyGrab( hit ) ) {
                    const auto clip{ viewProjection * core::float4{ hit.mPosition, 1.0f } };
                    const auto depth{ clip.z / clip.w };

                    // Keep the particle offset from the cursor to avoid a jump on the first frame.
                    mSoftBodyDrag = {
                        .mScene = scene::SceneHandle{ scene },
                        .mEntity = hit.mEntity,
                        .mParticle = hit.mParticle,
                        .mDepth = depth,
                        .mOffset = hit.mPosition - unproject( depth )
                    };
                }
            }
        }

        if ( mSoftBodyDrag.mScene ) {
            world.SetSoftBodyGrabTarget( unproject( mSoftBodyDrag.mDepth ) + mSoftBodyDrag.mOffset );
            ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeAll );
        }

        DrawSoftBodyVertices( hit.mEntity, hit.mParticle );
    }

    auto ViewportPanel::DrawSoftBodyVertices( entt::entity hovered, core::u32 hoveredParticle ) -> void {
        using Visibility = EditorState::ViewSettings::SoftBodyVertexVisibility;
        const auto visibility{ mEditorState->mViewSettings.mSoftBodyVertexVisibility };
        auto *scene{ mEditorState->mActiveScene };
        const auto *camera{ mEditorState->mActiveCamera };

        if ( visibility == Visibility::eHidden || !scene || !scene->GetPhysicsWorld() || !camera ||
             mViewport.mWidth <= 0.0f || mViewport.mHeight <= 0.0f ) {
            return;
        }

        auto &world{ *scene->GetPhysicsWorld() };
        const auto viewProjection{ camera->GetViewProjection() };
        const core::float4 image{ mViewport.mX, mViewport.mY, mViewport.mWidth, mViewport.mHeight };
        constexpr ImU32 particleColor{ IM_COL32( 220, 232, 240, 220 ) };
        constexpr ImU32 pinnedColor{ IM_COL32( 89, 177, 255, 255 ) };
        constexpr ImU32 activeColor{ IM_COL32( 246, 194, 96, 255 ) };
        constexpr ImU32 markerOutline{ IM_COL32( 18, 22, 27, 230 ) };
        eastl::vector<physics::SoftBodyParticle> particles{};
        auto *draw{ ImGui::GetWindowDrawList() };
        draw->PushClipRect( { image.x, image.y }, { image.x + image.z, image.y + image.w }, true );

        const auto drawEntity = [&]( entt::entity entity ) -> void {
            world.ReadSoftBodyParticles( entity, particles );

            for ( core::usize index{}; index < particles.size(); ++index ) {
                const auto &particle{ particles[index] };
                const auto clip{ viewProjection * core::float4{ particle.mPosition, 1.0f } };

                if ( clip.w <= 0.0f || clip.z < 0.0f || clip.z > clip.w || glm::abs( clip.x ) > clip.w || glm::abs( clip.y ) > clip.w ) {
                    continue;
                }

                const auto ndc{ core::float2{ clip } / clip.w };
                const ImVec2 position{ image.x + ( ndc.x * 0.5f + 0.5f ) * image.z, image.y + ( 0.5f - ndc.y * 0.5f ) * image.w };
                const bool active{ ( mSoftBodyDrag.mScene && entity == mSoftBodyDrag.mEntity && index == mSoftBodyDrag.mParticle ) ||
                                   ( entity == hovered && index == hoveredParticle ) };
                const auto radius{ active ? 5.0f : particle.mPinned ? 3.5f
                                                                    : 2.5f };
                const auto color{ active ? activeColor : particle.mPinned ? pinnedColor
                                                                          : particleColor };
                draw->AddCircleFilled( position, radius + 1.0f, markerOutline );
                draw->AddCircleFilled( position, radius, color );
            }
        };

        if ( visibility == Visibility::eAll ) {
            for ( const auto entity: scene->GetRegistry().view<scene::ClothComponent>() ) {
                drawEntity( entity );
            }

            for ( const auto entity: scene->GetRegistry().view<scene::SoftBodyComponent>() ) {
                if ( !scene->GetRegistry().all_of<scene::ClothComponent>( entity ) ) {
                    drawEntity( entity );
                }
            }
        }
        else if ( mSoftBodyDrag.mScene ) {
            drawEntity( mSoftBodyDrag.mEntity );
        }
        else if ( const auto *selected{ scene->FindByID( mEditorState->mSelectedEntityID ) } ) {
            drawEntity( selected->Get() );
        }

        draw->PopClipRect();
    }

    auto ViewportPanel::UpdatePicking() -> void {
        const auto mouse{ ImGui::GetMousePos() };
        const bool available{ mSceneImageHovered && !mPickingBlockedByOverlay &&
                              ImGui::IsWindowHovered() && !IsViewportWidgetActive() &&
                              !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !ImGui::GetIO().KeyAlt &&
                              !ImGui::IsMouseDown( ImGuiMouseButton_Right ) && !ImGui::IsMouseDown( ImGuiMouseButton_Middle ) &&
                              !ImGui::GetDragDropPayload() && mEditorState->mEditorRenderer && mEditorState->mActiveScene };
        if ( available && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) ) {
            // A new gesture supersedes any readback still pending for the previous one.
            CancelPicking();
            mBoxStart = mBoxEnd = mouse;
            mBoxActive = true;
            mBoxDragged = false;
            mBoxScene = mEditorState->mActiveScene;
            mBoxAppend = ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl;
            mPickParent = ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left );
        }
        if ( mBoxActive ) {
            mBoxEnd = { glm::clamp( mouse.x, mViewport.mX, mViewport.mX + mViewport.mWidth - 0.01f ),
                glm::clamp( mouse.y, mViewport.mY, mViewport.mY + mViewport.mHeight - 0.01f ) };
            mBoxDragged |= ImGui::IsMouseDragging( ImGuiMouseButton_Left );
            if ( ImGui::GetIO().AppFocusLost || mBoxScene != mEditorState->mActiveScene ||
                 ImGui::IsKeyPressed( ImGuiKey_Escape ) || ImGui::IsMouseDown( ImGuiMouseButton_Right ) ) {
                CancelPicking();
            }
            else if ( ImGui::IsMouseReleased( ImGuiMouseButton_Left ) ) {
                mBoxActive = false;
                const renderer::ReadPixelViewportInfo info{ mBoxEnd.x, mBoxEnd.y,
                    mViewport.mX, mViewport.mY, mViewport.mWidth, mViewport.mHeight };

                if ( mEditorState->mEditorRenderer->RequestPick( info ) ) {
                    mPickRectangle = mBoxDragged;
                    mPickAppend = mBoxAppend;
                    mPendingRectangleFirst = { mBoxStart.x, mBoxStart.y,
                        mViewport.mX, mViewport.mY, mViewport.mWidth, mViewport.mHeight };
                    mPendingRectangleLast = info;
                    mPickForTexture = mPickForContext = false;
                    mPickScene = mEditorState->mActiveScene;
                    mPickRevision = mEditorState->GetRevision();
                }
            }
        }

        // Exponential fade is independent of refresh rate; clip to the scene image.
        const float goal{ mBoxActive && mBoxDragged ? 1.0f : 0.0f };
        mBoxOpacity += ( goal - mBoxOpacity ) * ( 1.0f - std::exp( -18.0f * ImGui::GetIO().DeltaTime ) );
        if ( mBoxOpacity > 0.005f ) {
            auto *draw{ ImGui::GetWindowDrawList() };
            const ImVec2 minimum{ glm::min( mBoxStart.x, mBoxEnd.x ), glm::min( mBoxStart.y, mBoxEnd.y ) };
            const ImVec2 maximum{ glm::max( mBoxStart.x, mBoxEnd.x ), glm::max( mBoxStart.y, mBoxEnd.y ) };
            draw->PushClipRect( { mViewport.mX, mViewport.mY }, { mViewport.mX + mViewport.mWidth, mViewport.mY + mViewport.mHeight }, true );
            draw->AddRectFilled( minimum, maximum, ImGui::GetColorU32( ImVec4{ 232.0f / 255.0f, 191.0f / 255.0f, 124.0f / 255.0f, 0.18f * mBoxOpacity } ), 4.0f );
            draw->AddRect( minimum, maximum, ImGui::GetColorU32( ImVec4{ 0.0f, 0.0f, 0.0f, mBoxOpacity } ), 4.0f, 1.5f );
            draw->PopClipRect();
        }
    }

    auto ViewportPanel::UpdateTextureDrop() -> void {
        if ( !mEditorState->HasWorkspace() || mEditorState->mRuntimeActive || !mEditorState->mEditorRenderer ) {
            return;
        }
        // Use ImGui's preview/delivery phases: preview queries the ID buffer,
        // delivery freezes a fresh query at the release position before opening UI.
        // https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp
        const ImRect image{ { mViewport.mX, mViewport.mY }, { mViewport.mX + mViewport.mWidth, mViewport.mY + mViewport.mHeight } };
        if ( ImGui::BeginDragDropTargetCustom( image, ImGui::GetID( "ViewportTextureTarget" ) ) ) {
            if ( const auto *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_IMAGE",
                     ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect ) } ) {
                if ( payload->Data && payload->DataSize > 1 && static_cast<const char *>( payload->Data )[payload->DataSize - 1] == 0 ) {
                    mTextureDropPath = filesystem::Path{ static_cast<const char *>( payload->Data ) };
                    mTextureDropScene = mEditorState->mActiveScene;

                    if ( payload->IsDelivery() || !mTexturePickPending ) {
                        const auto mouse{ ImGui::GetMousePos() };
                        const renderer::ReadPixelViewportInfo info{ mouse.x, mouse.y, mViewport.mX, mViewport.mY, mViewport.mWidth, mViewport.mHeight };
                        if ( payload->IsDelivery() ) {
                            mEditorState->mEditorRenderer->CancelPick();
                        }

                        if ( mEditorState->mEditorRenderer->RequestPick( info ) ) {
                            mTexturePickPending = mPickForTexture = true;
                            mTextureDelivered = payload->IsDelivery();
                            mPickRectangle = mPickForContext = mPickForMaterial = false;
                            mPickScene = mEditorState->mActiveScene;
                            mPickRevision = mEditorState->GetRevision();
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if ( !ImGui::GetDragDropPayload() ) {
            mEditorState->mEditorRenderer->SetTextureDropHover( 0 );
        }

        if ( mOpenTexturePopup ) {
            mOpenTexturePopup = false;
            const auto metadata{ asset::AssetRegistryService::Get()->FindMetadata( asset::AssetRef<asset::Texture>{ mTextureDropPath }.GetID() ) };
            mTextureImported = metadata && metadata->IsPersistent();
            mTextureUsage = mTextureTargetID ? 0 : 1;
            mTextureMap = 0;
            mTextureUnique = true;
            mTextureUv = { 1, 1, 0, 0 };
            mTextureError.clear();
            mTextureRequest = {};
            ImGui::OpenPopup( "Apply texture to scene" );
        }

        // The viewport has no window padding; give this modal its own inset and
        // a bounded width so long paths scroll inside their field.
        const float scale{ ImGui::GetFontSize() / 17.0f };
        const float popupWidth{ ImGui::GetFontSize() * 34.0f };
        const imgui::ImGuiScopedStyleVar popupPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 14.0f * scale, 14.0f * scale } };
        const imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 10.0f * scale, 8.0f * scale } };
        const imgui::ImGuiScopedStyleVar cellPadding{ ImGuiStyleVar_CellPadding, ImVec2{ 6.0f * scale, 4.0f * scale } };
        ImGui::SetNextWindowSizeConstraints( { popupWidth, 0.0f }, { popupWidth, FLT_MAX } );

        if ( ImGui::BeginPopupModal( "Apply texture to scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
            ImGui::PushTextWrapPos( 0.0f );
            ImGui::TextDisabled( mTextureImported ? "Registered texture: choose application properties." : "New texture: import and apply using the project's texture policy." );
            ImGui::PopTextWrapPos();
            ImGui::Spacing();

            // Keep every property label on the left and stretch its control to
            // the same right edge, matching the inspector's material rows.
            ImGui::BeginDisabled( mTextureRequest.GetState() != asset::AssetState::eUnloaded );
            if ( ImGui::BeginTable( "##TextureDropProperties", 2,
                     ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX ) ) {
                ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize( "Material map" ).x );
                ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );

                BeginDropPropertyRow( "Path" );
                eastl::string texturePath{ mTextureDropPath.GetC_Str() };
                ImGui::InputText( "##TextureDropPath", texturePath.data(), texturePath.size() + 1,
                    ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AutoSelectAll );

                BeginDropPropertyRow( "Apply as" );
                ImGui::Combo( "##TextureDropUsage", &mTextureUsage, "Mesh texture\0Equirectangular environment cubemap\0" );

                if ( mTextureUsage == 0 ) {
                    static constexpr const char *maps{ "Base color\0Normal\0Metallic / roughness\0Occlusion\0Emission\0Height\0" };
                    BeginDropPropertyRow( "Material map" );
                    ImGui::Combo( "##TextureDropMap", &mTextureMap, maps );

                    BeginDropPropertyRow( "UV tiling" );
                    ImGui::DragFloat2( "##TextureDropUvTiling", &mTextureUv.x, 0.01f );

                    BeginDropPropertyRow( "UV offset" );
                    ImGui::DragFloat2( "##TextureDropUvOffset", &mTextureUv.z, 0.01f );
                }

                ImGui::EndTable();
            }

            if ( mTextureUsage == 0 ) {
                imgui::CheckBox( "Make material unique for this mesh", mTextureUnique );
                if ( !mTextureTargetID ) {
                    ImGui::TextDisabled( "Drop onto a mesh to assign a material texture." );
                }
            }
            else {
                ImGui::TextWrapped( "Use a flat 2:1 equirectangular panorama. Mikoto projects it into its environment cubemap; existing cube/array textures are not accepted here." );
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            const ImVec2 buttonSize{ ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x ) * 0.5f,
                ImGui::GetFrameHeight() + 6.0f * scale };
            const bool applicable{ mTextureDropScene == mEditorState->mActiveScene && ( mTextureUsage == 1 || mTextureTargetID != 0 ) };
            ImGui::BeginDisabled( !applicable );

            if ( ImGui::Button( mTextureImported ? "Apply" : "Import and apply", buttonSize ) ) {
                mTextureError.clear();
                mTextureRequest = asset::AssetRegistryService::Get()->Request<asset::Texture>( asset::AssetRef<asset::Texture>{ mTextureDropPath },
                    asset::TextureLoadDescription{ .mPath = mTextureDropPath, .mDimension = renderer::rhi::TextureDimension::eTexture2D } );
            }

            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::SameLine();

            if ( ImGui::Button( "Cancel", buttonSize ) ) {
                mTextureRequest = {};
                ImGui::CloseCurrentPopup();
            }

            if ( mTextureRequest.GetState() == asset::AssetState::eFailed ) {
                mTextureError = mTextureRequest.GetError();
                mTextureRequest = {};
            }

            if ( const auto texture{ mTextureRequest.GetIfReady() } ) {
                auto *scene{ mEditorState->mActiveScene };
                auto *target{ scene ? scene->FindByID( mTextureTargetID ) : nullptr };
                if ( mTextureDropScene != scene || !scene ) {
                    mTextureError = "The target scene changed.";
                }
                else if ( texture->GetResource()->GetDimension() != renderer::rhi::TextureDimension::eTexture2D ) {
                    mTextureError = "Only flat 2D textures and equirectangular panoramas are supported.";
                }
                else if ( mTextureUsage == 1 && texture->GetWidth() != texture->GetHeight() * 2 ) {
                    mTextureError = "An equirectangular environment must have a 2:1 width/height ratio.";
                }
                else if ( mTextureUsage == 1 ) {
                    auto material{ core::Ref<material::SkyboxMaterial>::New( "Dropped environment" ) };
                    material->SetEquirectangular( mTextureRequest );
                    scene::Entity *environment{};
                    for ( const auto &entry: scene->GetEntities() ) {
                        const auto &entity{ entry.second };

                        if ( entity->HasComponent<scene::SkyboxMaterialComponent>() ) {
                            environment = entity.get();
                            break;
                        }
                    }

                    if ( !environment ) {
                        environment = scene->CreateEntity( scene::EntityCreateInfo{ .mName = "Environment" } );
                    }

                    if ( !environment->HasComponent<scene::SkyboxMaterialComponent>() ) {
                        environment->AddComponent<scene::SkyboxMaterialComponent>();
                    }

                    environment->GetComponent<scene::SkyboxMaterialComponent>().SetMaterial( material );
                    scene->SetEnvironmentLightingSource( scene::EnvironmentLightingSource::eEnvironmentMap );
                    ImGui::CloseCurrentPopup();
                }
                else if ( target && target->HasComponent<scene::MaterialComponent>() ) {
                    auto &component{ target->GetComponent<scene::MaterialComponent>() };
                    auto *surface{ dynamic_cast<material::SurfaceMaterial *>( component.GetMaterial().GetPtr() ) };

                    if ( surface ) {
                        auto material{ mTextureUnique ? material::SurfaceMaterialHandle::New( *surface ) : core::Ref<material::SurfaceMaterial>{ surface } };
                        constexpr material::MapType types[]{ material::MapType::eBaseColor, material::MapType::eNormal,
                            material::MapType::eMetallicRoughness, material::MapType::eAmbientOcclusion, material::MapType::eEmissive, material::MapType::eHeight };
                        material->SetTexture( types[mTextureMap], mTextureRequest );
                        material->SetUvTransform( mTextureUv );
                        component.SetMaterial( material );
                        if ( mTextureUnique ) {
                            component.SetIsInternal( false );
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    else {
                        mTextureError = "The mesh needs a surface material.";
                    }
                }
                else {
                    mTextureError = "The target mesh no longer exists.";
                }

                mTextureRequest = {};
            }

            if ( !mTextureError.empty() ) {
                ImGui::TextWrapped( "%s", mTextureError.c_str() );
            }

            ImGui::EndPopup();
        }
    }

    auto ViewportPanel::UpdateMaterialDrop() -> void {
        if ( !mEditorState->HasWorkspace() || mEditorState->mRuntimeActive || !mEditorState->mEditorRenderer ) {
            return;
        }

        if ( mMaterialDropScene && mMaterialDropScene != mEditorState->mActiveScene ) {
            CancelPicking();
            mMaterialDropScene = nullptr;
            mMaterialRequest = {};
            mMaterialTargetID = 0;
        }

        // Resolve the asset during preview so only surface materials query a mesh.
        // Follow the texture drop's ImGui preview/delivery flow and asynchronous ID picking.
        // https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp
        bool hovering{};
        const ImRect image{ { mViewport.mX, mViewport.mY }, { mViewport.mX + mViewport.mWidth, mViewport.mY + mViewport.mHeight } };
        if ( ImGui::BeginDragDropTargetCustom( image, ImGui::GetID( "ViewportMaterialTarget" ) ) ) {
            if ( const auto *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_MATERIAL",
                     ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect ) } ) {
                if ( payload->Data && payload->DataSize > 1 && static_cast<const char *>( payload->Data )[payload->DataSize - 1] == 0 ) {
                    const filesystem::Path path{ static_cast<const char *>( payload->Data ) };
                    hovering = true;

                    if ( path != mMaterialDropPath || mMaterialDropScene != mEditorState->mActiveScene || !mMaterialRequest ) {
                        CancelPicking();
                        mMaterialDropPath = path;
                        mMaterialDropScene = mEditorState->mActiveScene;
                        mMaterialTargetID = 0;
                        mMaterialUnique = false;
                        mMaterialRequest = asset::AssetRegistryService::Get()->Request<asset::Material>( asset::AssetRef<asset::Material>{ path },
                            asset::MaterialLoadDescription{ .mPath = path } );
                    }

                    const auto mouse{ ImGui::GetMousePos() };
                    if ( !mMaterialDelivered ) {
                        mMaterialDropViewport = { mouse.x, mouse.y, mViewport.mX, mViewport.mY, mViewport.mWidth, mViewport.mHeight };
                    }

                    if ( payload->IsDelivery() ) {
                        mEditorState->mEditorRenderer->CancelPick();
                        mMaterialPickPending = mPickForMaterial = false;
                        mMaterialDelivered = true;
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        const auto loaded{ mMaterialRequest.GetIfReady() };
        auto resource{ loaded ? loaded->GetResource() : material::MaterialHandle{} };
        auto *surface{ dynamic_cast<material::SurfaceMaterial *>( resource.GetPtr() ) };
        auto *skybox{ dynamic_cast<material::SkyboxMaterial *>( resource.GetPtr() ) };

        if ( hovering || mMaterialDelivered ) {
            if ( surface && !mMaterialPickPending ) {
                if ( mEditorState->mEditorRenderer->RequestPick( mMaterialDropViewport ) ) {
                    mMaterialPickPending = mPickForMaterial = true;
                    mPickForTexture = mPickRectangle = mPickForContext = false;
                    mPickScene = mMaterialDropScene;
                    mPickRevision = mEditorState->GetRevision();
                }
            }
            else if ( !surface ) {
                mEditorState->mEditorRenderer->SetTextureDropHover( 0 );
                if ( mMaterialDelivered && ( resource || mMaterialRequest.GetState() == asset::AssetState::eFailed ) ) {
                    mMaterialDelivered = false;
                    mOpenMaterialPopup = true;
                }
            }
        }

        if ( mOpenMaterialPopup ) {
            mOpenMaterialPopup = false;
            ImGui::OpenPopup( "Apply material to scene" );
        }

        const float scale{ ImGui::GetFontSize() / 17.0f };
        const float width{ ImGui::GetFontSize() * 34.0f };
        const imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_WindowPadding, ImVec2{ 14.0f * scale, 14.0f * scale } };
        const imgui::ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 10.0f * scale, 8.0f * scale } };
        ImGui::SetNextWindowSizeConstraints( { width, 0.0f }, { width, FLT_MAX } );

        if ( ImGui::BeginPopupModal( "Apply material to scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
            auto *scene{ mEditorState->mActiveScene };
            auto *target{ scene && scene == mMaterialDropScene ? scene->FindByID( mMaterialTargetID ) : nullptr };
            const bool mesh{ target && target->HasComponent<scene::MeshComponent>() && target->GetComponent<scene::MeshComponent>().HasMesh() };

            if ( ImGui::BeginTable( "MaterialDropProperties", 2, ImGuiTableFlags_SizingStretchProp ) ) {
                ImGui::TableSetupColumn( "Label", ImGuiTableColumnFlags_WidthFixed, 112.0f * scale );
                ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
                BeginDropPropertyRow( "Path" );
                eastl::string path{ mMaterialDropPath.GetC_Str() };
                ImGui::InputText( "##MaterialDropPath", path.data(), path.size() + 1, ImGuiInputTextFlags_ReadOnly );
                BeginDropPropertyRow( "Material" );
                ImGui::TextUnformatted( resource ? resource->GetName().c_str() : "Unavailable" );
                BeginDropPropertyRow( "Apply to" );
                ImGui::TextUnformatted( skybox ? "Scene environment" : mesh ? target->GetComponent<scene::TagComponent>().GetTag().c_str()
                                                                            : "No mesh under cursor" );
                ImGui::EndTable();
            }

            if ( surface ) {
                imgui::CheckBox( "Create a unique copy for this mesh", mMaterialUnique );
                ImGui::TextWrapped( "Replace this mesh's entire surface material. A unique copy can be edited independently of the source asset." );
            }
            else if ( skybox ) {
                ImGui::TextWrapped( "Replace the scene's skybox and environment lighting with this material." );
            }
            else {
                ImGui::TextWrapped( "%s", mMaterialRequest.GetState() == asset::AssetState::eFailed
                                              ? mMaterialRequest.GetError().c_str()
                                              : "Drop a surface or skybox material here." );
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const ImVec2 size{ ( ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x ) * 0.5f, ImGui::GetFrameHeight() + 6.0f * scale };
            ImGui::BeginDisabled( !scene || scene != mMaterialDropScene || ( !skybox && !( surface && mesh ) ) );
            if ( ImGui::Button( "Apply", size ) ) {
                if ( surface ) {
                    if ( !target->HasComponent<scene::MaterialComponent>() ) {
                        target->AddComponent<scene::MaterialComponent>();
                    }
                    auto &component{ target->GetComponent<scene::MaterialComponent>() };
                    component.SetMaterial( mMaterialUnique ? material::SurfaceMaterialHandle::New( *surface ) : core::Ref<material::SurfaceMaterial>{ surface } );
                    component.SetIsInternal( false );
                }
                else {
                    scene::Entity *environment{};
                    for ( const auto &entry: scene->GetEntities() ) {
                        const auto &entity{ entry.second };

                        if ( entity->HasComponent<scene::SkyboxMaterialComponent>() ) {
                            environment = entity.get();
                            break;
                        }
                    }

                    if ( !environment ) {
                        environment = scene->CreateEntity( "Environment" );
                        environment->AddComponent<scene::SkyboxMaterialComponent>();
                    }

                    environment->GetComponent<scene::SkyboxMaterialComponent>().SetMaterial( resource );
                    scene->SetEnvironmentLightingSource( scene::EnvironmentLightingSource::eEnvironmentMap );
                }

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndDisabled();
            ImGui::SameLine();

            if ( ImGui::Button( "Cancel", size ) ) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    auto ViewportPanel::UpdateSceneContextMenu() -> void {
        const bool available{ mSceneImageHovered && !mPickingBlockedByOverlay && ImGui::IsWindowHovered() &&
                              !IsViewportWidgetActive() &&
                              ( mCursorTool != CursorTool::eSelect || ( !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() ) ) &&
                              !ImGui::GetDragDropPayload() && mEditorState->mActiveScene && mEditorState->mEditorRenderer };

        if ( mContextReady && !ImGui::IsMouseClicked( ImGuiMouseButton_Left ) &&
             !ImGui::IsMouseDown( ImGuiMouseButton_Right ) && !ImGui::IsKeyPressed( ImGuiKey_Escape ) &&
             ImGui::IsWindowFocused() ) {
            // Open under the same window/ID scope as BeginPopup, at the click
            // position rather than wherever the cursor moved during readback.
            ImGui::OpenPopup( mContextEntityID ? "##ViewportPanelEntityMenu" : "##ViewportPanelBackgroundMenu" );
        }

        if ( ImGui::IsMouseClicked( ImGuiMouseButton_Right ) ) {
            mRightMouseGesture = available ? RightMouseGesture::ePendingClick : RightMouseGesture::eIdle;
            mRightClickScene = mEditorState->mActiveScene;
            // Invalidate any older result while a new camera/context gesture begins.
            mPickScene = nullptr;

            if ( available ) {
                ImGui::SetWindowFocus();
            }
        }

        if ( mRightMouseGesture == RightMouseGesture::eIdle ) {
            return;
        }

        const auto &io{ ImGui::GetIO() };
        if ( mRightClickScene != mEditorState->mActiveScene || !ImGui::IsWindowFocused() ||
             io.AppFocusLost || ImGui::IsKeyPressed( ImGuiKey_Escape ) ) {
            mRightMouseGesture = RightMouseGesture::eIdle;
            return;
        }

        constexpr auto button{ ImGuiMouseButton_Right };
        const bool navigationInput{ io.MouseWheel != 0 || io.KeyAlt ||
                                    ImGui::IsKeyDown( ImGuiKey_W ) || ImGui::IsKeyDown( ImGuiKey_A ) ||
                                    ImGui::IsKeyDown( ImGuiKey_S ) || ImGui::IsKeyDown( ImGuiKey_D ) ||
                                    ImGui::IsKeyDown( ImGuiKey_Q ) || ImGui::IsKeyDown( ImGuiKey_E ) };

        if ( ImGui::IsMouseDown( button ) ) {
            // Once navigation starts, capture may warp the cursor back. Keep ownership
            // of the gesture until release even if the pointer returns to its origin.
            if ( ImGui::IsMouseDragging( button ) || navigationInput ) {
                mRightMouseGesture = RightMouseGesture::eNavigate;
            }
            return;
        }

        const bool click{ mRightMouseGesture == RightMouseGesture::ePendingClick && available &&
                          ImGui::IsMouseReleased( button ) && !navigationInput };
        mRightMouseGesture = RightMouseGesture::eIdle;
        if ( !click ) {
            return;
        }

        // ImGui accumulates drag distance while held. Check the release position too
        // when the last motion and release arrive in the same frame.
        const auto mouse{ ImGui::GetMousePos() };
        const auto origin{ io.MouseClickedPos[button] };
        const core::float2 delta{ mouse.x - origin.x, mouse.y - origin.y };
        if ( glm::dot( delta, delta ) >= io.MouseDragThreshold * io.MouseDragThreshold ) {
            return;
        }

        if ( mEditorState->mEditorRenderer->RequestPick( renderer::ReadPixelViewportInfo{
                 .mX = mouse.x,
                 .mY = mouse.y,
                 .mViewportX = mViewport.mX,
                 .mViewportY = mViewport.mY,
                 .mViewportWidth = mViewport.mWidth,
                 .mViewportHeight = mViewport.mHeight } ) ) {
            mPickScene = mEditorState->mActiveScene;
            mPickRevision = mEditorState->GetRevision();
            mPickParent = false;
            mPickForContext = true;
            mContextPopupPosition = mouse;
        }
    }

    auto ViewportPanel::UpdateManipulation() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mEditorState->mSelectedEntity ) {
            return;
        }

        scene::Entity *currentSelection{ mEditorState->mSelectedEntity };

        if ( !currentSelection->IsValid() || !currentSelection->GetComponent<scene::TagComponent>().IsActive() ) {
            return;
        }

        ImGuizmo::SetOrthographic( mEditorState->mActiveCamera->IsOrthographic() );

        // ImGuizmo resolves its input window through the draw-list owner name.
        // ImGui can reallocate Window::Name when a localized title changes while
        // keeping the same ### ID, leaving _OwnerName pointing at the old buffer.
        // Refresh that pointer before ImGuizmo resolves the hovered window.
        auto *drawList{ ImGui::GetWindowDrawList() };
        drawList->_OwnerName = ImGui::GetCurrentWindow()->Name;

        ImGuizmo::SetDrawlist( drawList );
        ImGuizmo::SetRect( mViewport.mX, mViewport.mY, mViewport.mWidth, mViewport.mHeight );
    }

    auto ViewportPanel::DrawSceneButtonsOverlay() -> void {
        imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_FramePadding, ImVec2{ 10.0f, 5.0f } };
        imgui::ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 3.0f, 4.0f } };
        imgui::ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 3.0f };

        constexpr SceneControlVisibility controls{};
        const float width{ GetSceneControlsWidth( controls ) };

        if ( width <= 0.0f ) {
            return;
        }

        ImGui::SetCursorPos( { mStartCursorPos.x + glm::max( 0.0f, ( mContentViewport.mWidth - width ) * 0.5f ), mStartCursorPos.y + 8.0f } );
        ImGui::BeginGroup();

        DrawSceneControls( *mEditorState, controls );

        ImGui::EndGroup();
        mPickingBlockedByOverlay |= ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled );
    }

    auto ViewportPanel::DrawToolbarBackground( const ImVec2 &defaultPosition, const ImVec2 &size, ImVec2 &offset, bool classic ) -> ImVec2 {
        const float scale{ ImGui::GetFontSize() / 17.0f };
        const ImVec2 minimum{ mContentViewport.mX + 8.0f * scale, mContentViewport.mY + 8.0f * scale };
        const ImVec2 maximum{
            glm::max( minimum.x, mContentViewport.mX + mContentViewport.mWidth - size.x - 8.0f * scale ),
            glm::max( minimum.y, mContentViewport.mY + mContentViewport.mHeight - size.y - 8.0f * scale )
        };

        // Offsets follow UI scale; clamping keeps a moved toolbar reachable after resizing.
        const ImVec2 position{
            glm::clamp( defaultPosition.x + offset.x * scale, minimum.x, maximum.x ),
            glm::clamp( defaultPosition.y + offset.y * scale, minimum.y, maximum.y )
        };

        offset = { ( position.x - defaultPosition.x ) / scale, ( position.y - defaultPosition.y ) / scale };
        const ImVec2 end{ position.x + size.x, position.y + size.y };
        if ( classic ) {
            auto color{ ImGui::GetStyleColorVec4( ImGuiCol_Tab ) };
            color.w = 0.5f;
            ImGui::RenderFrame( position, end, ImGui::GetColorU32( color ), false, ImGui::GetStyle().FrameRounding );
        }
        else {
            ImGui::GetWindowDrawList()->AddRectFilled( { position.x + scale, position.y + 2.0f * scale },
                { end.x + scale, end.y + 2.0f * scale }, IM_COL32( 0, 0, 0, 70 ), 5.0f * scale );
            ImGui::RenderFrame( position, end, IM_COL32( 37, 39, 42, 230 ), true, 5.0f * scale );
        }

        mPickingBlockedByOverlay |= ImRect{ position, end }.Contains( ImGui::GetMousePos() );
        return position;
    }

    auto ViewportPanel::DragToolbar( core::cstr id, const ImVec2 &minimum, const ImVec2 &size, const ImRect &content, ImVec2 &offset ) -> void {
        const auto dragID{ ImGui::GetID( id ) };

        // Disabled controls and FPS text may have no hovered ID. Exclude their
        // content bounds, but keep an existing background drag captured over them.
        if ( content.Contains( ImGui::GetMousePos() ) && ImGui::GetActiveID() != dragID ) {
            return;
        }

        const auto cursor{ ImGui::GetCursorScreenPos() };
        ImGui::SetCursorScreenPos( minimum );

        ImGui::InvisibleButton( id, size );

        if ( ImGui::IsItemActive() && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) ) {
            const float scale{ ImGui::GetFontSize() / 17.0f };
            const auto delta{ ImGui::GetIO().MouseDelta };
            offset.x += delta.x / scale;
            offset.y += delta.y / scale;
        }

        if ( ImGui::GetHoveredID() == dragID || ImGui::IsItemActive() ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeAll );
            mPickingBlockedByOverlay = true;
        }

        ImGui::SetCursorScreenPos( cursor );
    }

    auto ViewportPanel::DrawUtilitiesOverlay() -> void {
        MKT_BEGIN_PROFILER();

        constexpr core::f32 kClassicButtonSide{ 25.0f };
        constexpr core::f32 kClassicInset{ 3.5f };
        constexpr core::f32 kClassicRowSpacing{ 2.0f };
        constexpr core::f32 kClassicMinimumHeight{ 288.0f };
        constexpr core::f32 kClassicButtonCount{ 8.0f };
        constexpr core::f32 kToolbarIconButtonCount{ 5.0f };
        constexpr core::f32 kToolbarItemSpacing{ 2.0f };

        const core::f32 fontSize{ ImGui::GetFontSize() };
        const core::f32 scale{ fontSize / 17.0f };
        const bool classic{ mEditorState->mViewSettings.mToolsLayout == EditorState::ViewSettings::ToolsLayout::eClassic };
        const ImVec2 buttonSize{ ( classic ? kClassicButtonSide : kToolbarButtonWidth ) * scale,
            ( classic ? kClassicButtonSide : kToolbarButtonHeight ) * scale };
        const core::f32 inset{ ( classic ? kClassicInset : kToolbarInset ) * scale };
        const ImVec2 itemSpacing{ kToolbarItemSpacing * scale, ( classic ? kClassicRowSpacing : kToolbarItemSpacing ) * scale };

        const imgui::ImGuiScopedStyleVar toolPadding{ ImGuiStyleVar_FramePadding, ImVec2{ 6.0f * scale, ( buttonSize.y - fontSize ) * 0.5f } };
        const imgui::ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemSpacing, itemSpacing };
        const imgui::ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, ( classic ? 2.0f : 4.0f ) * scale };

        const bool grabActive{ mCursorTool == CursorTool::eGrabSoftBody };
        const auto *scene{ mEditorState->mActiveScene };
        const bool showClassicGrab{ classic && ( grabActive || ( scene &&
                                                                   ( !scene->GetRegistry().view<scene::SoftBodyComponent>().empty() || !scene->GetRegistry().view<scene::ClothComponent>().empty() ) ) ) };

        const bool compact{ classic || mContentViewport.mWidth < 600.0f * scale };
        const auto modeLabel{ MKT_LOC( mGizmoMode == imgui::GizmoMode::eWorld ? "viewport_toolbar_world" : "viewport_toolbar_local" ) };
        const auto pivotLabel{ MKT_LOC( mEditorState->mCenterPivot ? "viewport_toolbar_center" : "viewport_toolbar_pivot" ) };
        const float modeWidth{ compact ? buttonSize.x : ImGui::CalcTextSize( modeLabel.c_str() ).x + 12.0f * scale };
        const float pivotWidth{ compact ? buttonSize.x : ImGui::CalcTextSize( pivotLabel.c_str() ).x + 12.0f * scale };
        const float width{ ( kToolbarIconButtonCount + 1 ) * buttonSize.x + modeWidth + pivotWidth + 40.0f * scale };
        const float top{ 8.0f * scale + ( mEditorState->mViewSettings.mShowViewportControls ? ( kToolbarButtonHeight + 2.0f * kToolbarInset ) * scale : 0.0f ) };
        const float classicRows{ kClassicButtonCount + 1 + ( showClassicGrab ? 1.0f : 0.0f ) };
        const float classicContentHeight{ fontSize + classicRows * ( buttonSize.y + itemSpacing.y ) + 2.0f * inset };
        const ImVec2 size{ classic ? buttonSize.x + 2.0f * inset : width,
            classic ? glm::max( kClassicMinimumHeight * scale, classicContentHeight ) : buttonSize.y + 2.0f * inset };

        auto &offset{ classic ? mClassicToolbarOffset : mTransformToolbarOffset };
        const auto minimum{ DrawToolbarBackground( { mContentViewport.mX + 8.0f * scale, mContentViewport.mY + top }, size, offset, classic ) };

        ImGui::SetCursorScreenPos( { minimum.x + inset, minimum.y + inset } );
        ImGui::BeginGroup();
        {
            const imgui::ImGuiScopedStyleVar border{ ImGuiStyleVar_FrameBorderSize, 0.0f };
            auto buttonColor{ ImGui::GetStyleColorVec4( ImGuiCol_Button ) };
            auto hoverColor{ ImGui::GetStyleColorVec4( ImGuiCol_ButtonHovered ) };
            auto activeColor{ ImGui::GetStyleColorVec4( ImGuiCol_ButtonActive ) };

            buttonColor.w = classic ? 0.42f : 0.0f;
            if ( classic ) {
                hoverColor.w = 0.55f;
                activeColor.w = 0.65f;
            }

            const imgui::ImGuiScopedColor buttonStyle{ ImGuiCol_Button, buttonColor };
            const imgui::ImGuiScopedColor hoverStyle{ ImGuiCol_ButtonHovered, hoverColor };
            const imgui::ImGuiScopedColor activeStyle{ ImGuiCol_ButtonActive, activeColor };

            // Restore the vertical tools and drag handle from ScenePanel's
            // DrawTransformGizmos (e0887059), using the current widget/input API.
            if ( classic ) {
                static const eastl::string icon{ imgui::GetStringFromUnicode( 58835 ) };
                const ImVec2 dragSize{ buttonSize.x, fontSize };
                ImGui::InvisibleButton( "ClassicToolbarDrag", dragSize );

                // The dots stay unchanged; this area only captures toolbar dragging.
                const auto dragMinimum{ ImGui::GetItemRectMin() };
                const auto textSize{ ImGui::CalcTextSize( icon.c_str() ) };
                ImGui::GetWindowDrawList()->AddText( { dragMinimum.x + ( dragSize.x - textSize.x ) * 0.5f, dragMinimum.y },
                    ImGui::GetColorU32( ImGuiCol_Text ), icon.c_str() );

                if ( ImGui::IsItemActive() && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) ) {
                    const auto delta{ ImGui::GetIO().MouseDelta };
                    offset.x += delta.x / scale;
                    offset.y += delta.y / scale;
                }

                if ( ImGui::IsItemHovered() || ImGui::IsItemActive() ) {
                    mPickingBlockedByOverlay = true;
                }
            }

            const auto drawGrabTool = [&] {
                if ( imgui::widget::ToggleIconButton( "SceneGrabSoftBody", ICON_MD_FRONT_HAND, grabActive, buttonSize ) ) {
                    SetCursorTool( grabActive ? CursorTool::eSelect : CursorTool::eGrabSoftBody );
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_tool_grab_soft_body" ).c_str() );
                imgui::SetCursorHandOnLastItemHovered();
            };

            if ( !classic ) {
                drawGrabTool();
                ImGui::SameLine();
            }

            if ( imgui::widget::ToggleIconButton( "SceneLandscape", ICON_MD_TERRAIN, mCursorTool == CursorTool::eLandscape, buttonSize ) ) {
                SetCursorTool( mCursorTool == CursorTool::eLandscape ? CursorTool::eSelect : CursorTool::eLandscape );
            }
            imgui::widget::ItemTooltip( "%s", MKT_LOC( "terrain_brush_tooltip" ).c_str() );
            if ( !classic ) {
                ImGui::SameLine();
            }

            const eastl::string iconTranslate{ imgui::GetStringFromUnicode( 57523 ) };

            if ( imgui::widget::ToggleIconButton( "SceneTranslate", iconTranslate.c_str(), mCursorTool == CursorTool::eSelect && mGizmoType == imgui::GizmoType::eTranslation, buttonSize ) ) {
                SetGizmoType( imgui::GizmoType::eTranslation );
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_tool_move" ).c_str() );
            imgui::SetCursorHandOnLastItemHovered();

            if ( !classic ) {
                ImGui::SameLine();
            }

            const eastl::string iconRotate{ imgui::GetStringFromUnicode( 58423 ) };

            if ( imgui::widget::ToggleIconButton( "SceneRotate", iconRotate.c_str(), mCursorTool == CursorTool::eSelect && mGizmoType == imgui::GizmoType::eRotation, buttonSize ) ) {
                SetGizmoType( imgui::GizmoType::eRotation );
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_tool_rotate" ).c_str() );
            imgui::SetCursorHandOnLastItemHovered();

            if ( !classic ) {
                ImGui::SameLine();
            }

            const eastl::string iconScale{ imgui::GetStringFromUnicode( 58165 ) };

            if ( imgui::widget::ToggleIconButton( "SceneScale", iconScale.c_str(), mCursorTool == CursorTool::eSelect && mGizmoType == imgui::GizmoType::eScale, buttonSize ) ) {
                SetGizmoType( imgui::GizmoType::eScale );
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_tool_scale" ).c_str() );
            imgui::SetCursorHandOnLastItemHovered();

            if ( !classic ) {
                ImGui::SameLine();
            }

            const eastl::string iconBounds{ imgui::GetStringFromUnicode( 59902 ) };

            if ( imgui::widget::ToggleIconButton( "SceneBounds", iconBounds.c_str(), mEditorState->mViewSettings.mShowBounds, buttonSize ) ) {
                mEditorState->mViewSettings.mShowBounds = !mEditorState->mViewSettings.mShowBounds;
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_tool_bounds" ).c_str() );
            imgui::SetCursorHandOnLastItemHovered();

            if ( !classic ) {
                ImGui::SameLine();
                const auto divider{ ImGui::GetCursorScreenPos() };
                ImGui::GetWindowDrawList()->AddLine( { divider.x + 5.0f * scale, divider.y + 4.0f * scale },
                    { divider.x + 5.0f * scale, divider.y + buttonSize.y - 4.0f * scale }, IM_COL32( 80, 83, 88, 255 ) );
                ImGui::Dummy( { 10.0f * scale, buttonSize.y } );
                ImGui::SameLine();
            }

            // Gizmo mode (local space or world)
            const eastl::string iconGizmoMode{ imgui::GetStringFromUnicode( mGizmoMode == imgui::GizmoMode::eWorld ? 58832 : 58833 ) };

            if ( compact ? imgui::widget::IconButton( "GizmoMode", iconGizmoMode.c_str(), buttonSize )
                         : ImGui::Button( modeLabel.c_str(), { modeWidth, buttonSize.y } ) ) {
                if ( mGizmoMode == imgui::GizmoMode::eWorld ) {
                    mGizmoMode = imgui::GizmoMode::eLocal;
                }
                else {
                    mGizmoMode = imgui::GizmoMode::eWorld;
                }
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC(
                                                  mGizmoMode == imgui::GizmoMode::eWorld ? "viewport_tool_world" : "viewport_tool_local" )
                                                  .c_str() );
            imgui::SetCursorHandOnLastItemHovered();

            if ( !classic ) {
                ImGui::SameLine();
            }

            const auto iconGizmoCenter{ imgui::GetStringFromUnicode( 58292 ) };
            const auto iconGizmoNormal{ imgui::GetStringFromUnicode( 58293 ) };

            if ( compact ? imgui::widget::IconButton( "GizmoPivot", mEditorState->mCenterPivot ? iconGizmoCenter.c_str() : iconGizmoNormal.c_str(), buttonSize )
                         : ImGui::Button( pivotLabel.c_str(), { pivotWidth, buttonSize.y } ) ) {
                mEditorState->mCenterPivot = !mEditorState->mCenterPivot;
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC(
                                                  mEditorState->mCenterPivot ? "viewport_center_pivot_manipulate_around_the_object_s_bounds_click_for"
                                                                             : "viewport_pivot_manipulate_around_the_entity_origin_click_for_center" )
                                                  .c_str() );
            imgui::SetCursorHandOnLastItemHovered();

            if ( classic ) {
                const bool orthographic{ mEditorState->mActiveCamera && mEditorState->mActiveCamera->IsOrthographic() };
                const auto iconProjection{ imgui::GetStringFromUnicode( orthographic ? 58301 : 58381 ) };

                if ( imgui::widget::IconButton( "SceneProjection", iconProjection.c_str(), buttonSize ) && mEditorState->mRequestEditorProjection ) {
                    mEditorState->mRequestEditorProjection( orthographic ? scene::ProjectionType::ePerspective : scene::ProjectionType::eOrthographic );
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( orthographic ? "viewport_tool_orthographic" : "viewport_tool_perspective" ).c_str() );

                if ( imgui::widget::ToggleIconButton( "SceneGrid", ICON_MD_GRID_ON, mEditorState->mViewSettings.mShowGrid, buttonSize ) ) {
                    mEditorState->mViewSettings.mShowGrid = !mEditorState->mViewSettings.mShowGrid;
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_show_grid" ).c_str() );

                if ( showClassicGrab ) {
                    drawGrabTool();
                }
            }
        }

        ImGui::EndGroup();
        mPickingBlockedByOverlay |= ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) || ImGui::IsItemActive();
        const ImRect content{ ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };

        if ( !classic ) {
            DragToolbar( "##TransformToolbarDrag", minimum, size, content, mTransformToolbarOffset );
        }
    }

    auto ViewportPanel::DrawManipulationGizmos() -> void {
        MKT_BEGIN_PROFILER();
        scene::Entity *currentSelection{ mEditorState->mSelectedEntity };

        if ( currentSelection == nullptr || !currentSelection->IsValid() || currentSelection->HasComponent<scene::GroupComponent>() || !currentSelection->GetComponent<scene::TagComponent>().IsActive() ) {
            mDraggingGizmo = false;
            mGizmoMeshBounds.clear();
            return;
        }

        scene::TransformComponent &transformComponent{ currentSelection->GetComponent<scene::TransformComponent>() };
        scene::TextComponent *screenText{};

        core::float4x4 cameraView{ mEditorState->mActiveCamera->GetViewMatrix() };
        core::float4x4 cameraProjection{ mEditorState->mActiveCamera->GetProjection() };
        ImGuizmo::SetOrthographic( mEditorState->mActiveCamera->IsOrthographic() );

        if ( currentSelection->HasComponent<scene::TextComponent>() ) {
            const auto &text{ currentSelection->GetComponent<scene::TextComponent>() };

            if ( !text.IsWorldText() && mEditorState->mEditorRenderer ) {
                screenText = &currentSelection->GetComponent<scene::TextComponent>();
                const auto dimensions{ InferDimensions( mEditorState->mEditorRenderer->GetRenderResolution() ) };
                const auto width{ dimensions.first };
                const auto height{ dimensions.second };

                cameraView = core::float4x4{ 1.0f };
                cameraProjection = glm::ortho( 0.0f, width, height, 0.0f, -1.0f, 1.0f );
                ImGuizmo::SetOrthographic( true );
            }
            else if ( text.GetCamera() ) {
                cameraView = text.GetCamera()->GetViewMatrix();
                cameraProjection = text.GetCamera()->GetProjection();
                ImGuizmo::SetOrthographic( text.GetCamera()->IsOrthographic() );
            }
        }

        ++mGizmoBoundsFrame;

        if ( mGizmoDragRevision != mEditorState->GetRevision() || !ImGuizmo::IsUsing() ) {
            mDraggingGizmo = false;
        }

        core::float3 localCenter{ mDraggingGizmo ? mGizmoDragCenter : core::float3{} };

        // Text alignment affects its anchor, but rotation always uses its bounds center.
        const bool centerText{ mGizmoType == imgui::GizmoType::eRotation && currentSelection->HasComponent<scene::TextComponent>() };

        if ( ( mEditorState->mCenterPivot || centerText ) && !mDraggingGizmo ) {
            core::float3 minimum{};
            core::float3 maximum{};
            bool hasBounds{};
            const auto include = [&]( const core::float3 &point ) {
                if ( !hasBounds ) {
                    minimum = maximum = point;
                    hasBounds = true;
                }
                else {
                    minimum = glm::min( minimum, point );
                    maximum = glm::max( maximum, point );
                }
            };
            const auto gather = [&]( auto &&self, const scene::Entity &entity, const core::float4x4 &relative ) -> void {
                if ( !entity.GetComponent<scene::TagComponent>().IsActive() ) {
                    return;
                }

                if ( !screenText && entity.HasComponent<scene::MeshComponent>() ) {
                    const auto &mesh{ entity.GetComponent<scene::MeshComponent>() };

                    if ( mesh.HasMesh() ) {
                        const auto *node = mesh.GetMesh();
                        auto linear{ relative };
                        linear[3] = core::float4{ 0, 0, 0, 1 };
                        const auto id{ entity.GetComponent<scene::TagComponent>().GetGuid() };
                        const auto entry{ mGizmoMeshBounds.try_emplace( id ) };
                        auto &cached{ entry.first->second };
                        const bool changed{ entry.second || cached.mModel != mesh.GetModel() ||
                                            cached.mMeshIndex != node->GetMeshIndex() || cached.mTransform != linear };
                        bool posed{};

                        if ( entity.HasComponent<scene::SkinnedMeshRenderer>() && !node->GetSkinningVertices().empty() ) {
                            auto *animations{ animation::AnimationSystem::GetPtr() };
                            const auto *animator{ animations ? animations->GetAnimator( entity.GetComponent<scene::SkinnedMeshRenderer>().GetAnimatorID() ) : nullptr };

                            if ( animator && !animator->GetJointModelMatrices( node->GetSkinBinding().mRigIndex ).empty() ) {
                                const auto &pose{ animator->GetJointModelMatrices( node->GetSkinBinding().mRigIndex ) };
                                bool poseChanged{ cached.mPose.size() != pose.size() };

                                for ( core::usize joint{}; !poseChanged && joint < pose.size(); ++joint ) {
                                    poseChanged = cached.mPose[joint] != pose[joint];
                                }
                                // Reuse unchanged poses. During animation, transform cached joint
                                // influence boxes instead of skinning every vertex for the gizmo.
                                // These conservative bounds follow animation and IK in O(joints);
                                // their center may differ slightly from a tight per-vertex box.
                                if ( !changed && cached.mPosed && !poseChanged ) {
                                    posed = true;
                                }
                                else {
                                    // BuildSkinningPalette initializes the full palette before use.
                                    eastl::array<core::float4x4, asset::kMaxBonesPerMesh> palette;

                                    if ( animation::BuildSkinningPalette( node->GetSkinBinding(),
                                             { pose.data(), pose.size() }, palette ) ) {
                                        cached.mBounds = node->GetSkinnedBounds( palette, linear );
                                        cached.mPose.resize( pose.size() );

                                        for ( core::usize joint{}; joint < pose.size(); ++joint ) {
                                            cached.mPose[joint] = pose[joint];
                                        }

                                        posed = true;
                                    }
                                }
                            }
                        }

                        if ( !posed ) {
                            if ( changed || cached.mPosed ) {
                                cached.mBounds = node->GetTransformedBounds( linear );
                            }

                            cached.mPose.clear();
                        }

                        cached.mModel = mesh.GetModel();
                        cached.mMeshIndex = node->GetMeshIndex();
                        cached.mTransform = linear;
                        cached.mPosed = posed;
                        cached.mFrame = mGizmoBoundsFrame;
                        // Translation moves the cached bounds without changing their extents.
                        math::GizmoBounds meshBounds{ cached.mBounds };
                        meshBounds.mMinimum += core::float3{ relative[3] };
                        meshBounds.mMaximum += core::float3{ relative[3] };

                        if ( meshBounds.HasExtent() ) {
                            include( meshBounds.mMinimum );
                            include( meshBounds.mMaximum );
                        }
                    }
                }

                if ( entity.HasComponent<scene::TextComponent>() ) {
                    const auto &text = entity.GetComponent<scene::TextComponent>();

                    if ( text.HasFont() && text.IsWorldText() == ( screenText == nullptr ) ) {
                        const auto layout{ text::LayoutText( *text.GetFont(), text.GetUnicodeContents(),
                            { text.GetSize(), text.GetSpacing(),
                                text.GetLineSpacing(), core::as<text::Alignment>( text.GetAlignment() ), !text.IsWorldText() } ) };

                        if ( !layout.mGlyphs.empty() ) {
                            for ( core::f32 x: { layout.mMinimum.x, layout.mMaximum.x } ) {
                                for ( core::f32 y: { layout.mMinimum.y, layout.mMaximum.y } ) {
                                    include( core::float3{ relative * core::float4{ x, y, 0, 1 } } );
                                }
                            }
                        }
                    }
                }

                if ( screenText || !mEditorState->mTransformChildren ) {
                    return;// Only include descendants when they participate in the edit.
                }

                for ( const auto childID: entity.GetComponent<scene::RelationComponent>().GetChildren() ) {
                    if ( const auto *child{ mEditorState->mActiveScene->FindByID( childID ) } ) {
                        self( self, *child, relative * child->GetComponent<scene::TransformComponent>().GetTransform() );
                    }
                }
            };

            gather( gather, *currentSelection, core::float4x4{ 1.0f } );

            std::erase_if( mGizmoMeshBounds, [this]( const auto &entry ) {
                return entry.second.mFrame != mGizmoBoundsFrame;
            } );

            if ( hasBounds ) {
                localCenter = minimum * 0.5f + maximum * 0.5f;
            }
        }

        core::float4x4 parentWorld{ 1.0f };

        if ( const auto parentID = currentSelection->GetComponent<scene::RelationComponent>().GetParent(); !screenText && parentID ) {
            if ( const auto *parent = mEditorState->mActiveScene->FindByID( *parentID ) ) {
                parentWorld = parent->GetComponent<scene::TransformComponent>().GetWorldTransform();
            }
        }

        core::float4x4 objectTransform{ math::MakeGizmoTransform( screenText ? text::ScreenTextModel( screenText->GetScreenPlacement() ) : parentWorld * transformComponent.GetTransform(), localCenter ) };

        ImGuizmo::MODE mode{ GetGizmoMode( mGizmoMode ) };
        ImGuizmo::OPERATION type{ GetGizmoType( mGizmoType ) };

        if ( screenText ) {
            if ( mGizmoType == imgui::GizmoType::eTranslation ) {
                type = ImGuizmo::OPERATION( ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y );
            }

            if ( mGizmoType == imgui::GizmoType::eRotation ) {
                type = ImGuizmo::ROTATE_Z;
            }

            if ( mGizmoType == imgui::GizmoType::eScale ) {
                type = ImGuizmo::OPERATION( ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y );
            }
        }

        // Rotation handles must not recompose the mesh's scaled/sheared basis.
        // Apply their world-space delta to the exact centered object matrix instead.
        const bool rotating{ mGizmoType == imgui::GizmoType::eRotation };
        const auto centeredObject{ objectTransform };

        if ( rotating ) {
            objectTransform = math::MakeGizmoRotationFrame( centeredObject );
        }

        core::float4x4 delta{ 1.0f };
        ImGuizmo::Enable( !mPickingBlockedByOverlay || ImGuizmo::IsUsing() );
        const bool manipulated{ ImGuizmo::Manipulate( glm::value_ptr( cameraView ), glm::value_ptr( cameraProjection ),
            type, mode, glm::value_ptr( objectTransform ), glm::value_ptr( delta ) ) };
        ImGuizmo::Enable( true );

        if ( ImGuizmo::IsUsing() && !mDraggingGizmo ) {
            mGizmoDragCenter = localCenter;
            mGizmoDragRevision = mEditorState->GetRevision();
        }

        // Include the final changed frame on mouse release, but never rewrite a
        // transform merely because the mouse is resting on an active handle.
        if ( manipulated ) {
            if ( rotating ) {
                objectTransform = delta * centeredObject;
            }

            core::float4x4 local{};

            if ( math::GizmoToLocalTransform( objectTransform, localCenter, parentWorld, local ) ) {
                if ( screenText ) {
                    text::SetScreenTextModel( screenText->GetScreenPlacement(), local );
                }
                else {
                    mEditorState->mTransformEditRejected = !mEditorState->mActiveScene->SetLocalTransform(
                        currentSelection->GetComponent<scene::TagComponent>().GetGuid(), local, mEditorState->mTransformChildren );
                }
            }
        }

        mDraggingGizmo = ImGuizmo::IsUsing();
    }


    auto ViewportPanel::DrawOrientationAxis() -> void {
        MKT_BEGIN_PROFILER();
        auto *camera{ mEditorState->mActiveCamera };

        if ( !camera ) {
            return;
        }

        // Screen-space size follows the UI font, not the camera's FOV, lens or distance.
        // Upper anchors leave room for both toolbar rows.
        constexpr core::f32 kOrientationSizeScale{ 0.88f };
        constexpr core::f32 kOrientationEdgePadding{ 6.0f };

        const core::f32 scale{ glm::clamp( ImGui::GetFontSize() / 14.0f, 0.85f, 2.5f ) * kOrientationSizeScale };
        const core::f32 size{ 84.0f * scale };
        const core::f32 padding{ kOrientationEdgePadding * scale };

        const float toolbarScale{ ImGui::GetFontSize() / 17.0f };
        const float toolbarHeight{ kToolbarButtonHeight + 2.0f * kToolbarInset };
        const float toolbarBottom{ ( toolbarHeight + 16.0f +
                                       ( mEnableUtilityOverlay && mContentViewport.mWidth < 900.0f * toolbarScale ? toolbarHeight + 8.0f : 0.0f ) +
                                       ( mEditorState->mViewSettings.mShowViewportControls ? toolbarHeight : 0.0f ) ) *
                                   toolbarScale };

        if ( mContentViewport.mWidth < size + padding * 2.0f || mContentViewport.mHeight < size + toolbarBottom + padding ) {
            return;
        }

        using Anchor = EditorState::ViewSettings::OrientationAnchor;
        const auto &view{ mEditorState->mViewSettings };
        const auto anchor{ view.mToolsLayout == EditorState::ViewSettings::ToolsLayout::eClassic ? view.mClassicOrientationAnchor : view.mOrientationAnchor };
        const bool left{ anchor == Anchor::eBottomLeft || anchor == Anchor::eTopLeft };
        const bool top{ anchor == Anchor::eTopLeft || anchor == Anchor::eTopRight };
        const ImVec2 minimum{
            mContentViewport.mX + ( left ? padding : mContentViewport.mWidth - size - padding ),
            mContentViewport.mY + ( top ? toolbarBottom : mContentViewport.mHeight - size - padding )
        };
        const ImVec2 center{ minimum.x + size * 0.5f, minimum.y + size * 0.5f };
        const core::f32 armLength{ 26.0f * scale };

        constexpr ImU32 colors[]{ IM_COL32( 239, 91, 105, 255 ), IM_COL32( 132, 206, 92, 255 ), IM_COL32( 85, 164, 245, 255 ) };
        constexpr core::cstr labels[]{ "X", "Y", "Z", "-X", "-Y", "-Z" };
        constexpr core::cstr views[]{ "Right (+X)", "Top (+Y)", "Front (+Z)", "Left (-X)", "Bottom (-Y)", "Back (-Z)" };
        eastl::array<OrientationMarker, 6> markers{};

        for ( core::usize index{}; index < markers.size(); ++index ) {
            auto &marker{ markers[index] };
            marker.mOrder = index;
            marker.mPositive = index < 3;
            marker.mAxis[index % 3] = marker.mPositive ? 1.0f : -1.0f;

            // Transform directions (w=0), never positions. A perspective
            // projection would distort this orientation-only indicator.
            const core::float3 direction{ camera->GetViewMatrix() * core::float4{ marker.mAxis, 0.0f } };
            marker.mPosition = { center.x + direction.x * armLength, center.y - direction.y * armLength };
            marker.mDepth = direction.z;
            marker.mRadius = ( marker.mPositive ? 7.5f : 4.0f ) * scale;
            marker.mColor = colors[index % 3];
            marker.mLabel = labels[index];
            marker.mViewName = views[index];
        }

        // RH view space looks down -Z: draw distant markers first. Hit testing
        // walks the reverse order so overlapping axis ends select the visible one.
        eastl::sort( markers.begin(), markers.end(), []( const auto &a, const auto &b ) {
            return a.mDepth != b.mDepth ? a.mDepth < b.mDepth : a.mOrder < b.mOrder;
        } );

        const bool interactive{ camera == mEditorState->mEditorCamera && !ImGuizmo::IsUsing() &&
                                !ImGui::IsMouseDown( ImGuiMouseButton_Right ) && !ImGui::IsMouseDown( ImGuiMouseButton_Middle ) &&
                                !ImGui::GetDragDropPayload() };

        const ImVec2 cursor{ ImGui::GetCursorScreenPos() };
        ImGui::SetCursorScreenPos( minimum );
        ImGui::BeginDisabled( !interactive );
        ImGui::InvisibleButton( "##SceneOrientation", { size, size } );
        const bool hovered{ ImGui::IsItemHovered() };

        if ( ImGui::IsItemActivated() ) {
            mOrientationDragged = false;
        }

        const bool dragging{ ImGui::IsItemActive() && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) };
        const bool clicked{ ImGui::IsItemDeactivated() && hovered && !mOrientationDragged };

        if ( dragging ) {
            mOrientationDragged = true;
            const ImVec2 delta{ ImGui::GetIO().MouseDelta };
            const core::float3 forward{ camera->GetTargetForward() };
            const core::f32 yaw{ glm::atan2( forward.x, -forward.z ) + delta.x * 0.008f };
            const core::f32 pitch{ glm::clamp( glm::asin( glm::clamp( forward.y, -1.0f, 1.0f ) ) - delta.y * 0.008f,
                glm::radians( -89.9f ), glm::radians( 89.9f ) ) };
            // Respect the selected navigation mode and reset the spring together
            // with the view, so an axis drag cannot drift back afterward.
            camera->SetViewDirection( { glm::sin( yaw ) * glm::cos( pitch ), glm::sin( pitch ), -glm::cos( yaw ) * glm::cos( pitch ) } );
        }

        mPickingBlockedByOverlay |= ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) || ImGui::IsItemActive();
        ImGui::EndDisabled();
        ImGui::SetCursorScreenPos( cursor );

        // Restoring an overlay cursor can cross the current content bounds in
        // a small viewport. Submit an item so ImGui accounts for that position.
        ImGui::Dummy( { 0, 0 } );
        const auto mouse{ ImGui::GetMousePos() };
        const OrientationMarker *highlighted{};

        if ( hovered ) {
            for ( auto it{ markers.rbegin() }; it != markers.rend(); ++it ) {
                const core::f32 x{ mouse.x - it->mPosition.x };
                const core::f32 y{ mouse.y - it->mPosition.y };

                if ( x * x + y * y <= it->mRadius * it->mRadius ) {
                    highlighted = &*it;
                    break;
                }
            }
        }

        auto *drawList{ ImGui::GetWindowDrawList() };
        drawList->PushClipRect( { mContentViewport.mX, mContentViewport.mY },
            { mContentViewport.mX + mContentViewport.mWidth, mContentViewport.mY + mContentViewport.mHeight }, true );

        // Keep the original unobtrusive axes: no disc, labelled positive ends
        // and small negative dots. The invisible hit area also supports dragging.
        for ( const auto &marker: markers ) {
            if ( marker.mPositive ) {
                drawList->AddLine( center, marker.mPosition, marker.mColor, 1.5f * scale );
            }
        }

        const core::f32 fontSize{ 11.0f * scale };

        for ( const auto &marker: markers ) {
            const bool selected{ highlighted == &marker };
            drawList->AddCircleFilled( marker.mPosition, marker.mRadius, marker.mPositive ? marker.mColor : ( marker.mColor & 0x00ffffffu ) | ( 150u << 24 ) );

            if ( selected ) {
                drawList->AddCircle( marker.mPosition, marker.mRadius + scale, IM_COL32_WHITE, 0, scale );
            }

            if ( marker.mPositive ) {
                const ImVec2 textSize{ ImGui::GetFont()->CalcTextSizeA( fontSize, 1000.0f, 0.0f, marker.mLabel ) };
                const ImVec2 textPosition{ glm::floor( marker.mPosition.x - textSize.x * 0.5f ), glm::floor( marker.mPosition.y - textSize.y * 0.5f ) };
                drawList->AddText( ImGui::GetFont(), fontSize, textPosition, IM_COL32( 17, 22, 28, 255 ), marker.mLabel );
            }
        }

        drawList->PopClipRect();

        if ( highlighted ) {
            ImGui::SetMouseCursor( ImGuiMouseCursor_Hand );
            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "viewport_click_to_align_drag_to_rotate_using_the_selected_camera" ).c_str(), highlighted->mViewName );
        }

        if ( clicked && highlighted ) {
            core::float3 axis{ highlighted->mAxis };

            if ( glm::dot( camera->GetTargetForward(), -axis ) > 0.9999f ) {
                axis = -axis;
            }

            camera->TransitionViewDirection( -axis );
        }
    }

    auto ViewportPanel::InitOverlayIcons() -> void {
        MKT_BEGIN_PROFILER();
        using namespace renderer::rhi;
        constexpr const char *filenames[]{
            "camera-4-512.png", "idea-512.png", "sun-2-512.png",
            "text-512.png", "text-bg-color-512.png", "volume-up-4-512.png"
        };

        for ( core::usize index{}; index < mOverlayIcons.size(); ++index ) {
            try {
                const filesystem::Path path{ string::Format( "Resources/Overlay Icon/{}", filenames[index] ) };

                if ( auto image{ asset::ProcessImage2D( path ) } ) {
                    mOverlayIcons[index] = mGpuDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
                            .SetName( filenames[index] )
                            .SetImageData( image )
                            .SetWidth( image->mWidth )
                            .SetHeight( image->mHeight )
                            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
                            .SetUsage( renderer::rhi::TextureUsageFlagsBits::ShaderResource )
                            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM ) );
                }
            } catch ( const std::exception &error ) {
                MKT_CORE_LOGGER_WARN( "Cannot load scene overlay icon '{}': {}", filenames[index], error.what() );
            }
        }
    }

    auto ViewportPanel::DrawViewSettings() -> void {
        const auto cursor{ ImGui::GetCursorPos() };
        const bool classic{ mEditorState->mViewSettings.mToolsLayout == EditorState::ViewSettings::ToolsLayout::eClassic };
        const float scale{ ImGui::GetFontSize() / 17.0f };
        const ImVec2 buttonSize{ kToolbarButtonWidth * scale, kToolbarButtonHeight * scale };
        const bool compact{ mContentViewport.mWidth < 600.0f * scale };
        const bool showFps{ mContentViewport.mWidth >= 600.0f * scale };

        struct AspectPreset {
            core::cstr mName;
            float mRatio;
        };

        static constexpr eastl::array<AspectPreset, 6> aspectPresets{ { { "", 0.0f }, { "16:9", 16.0f / 9.0f }, { "16:10", 16.0f / 10.0f }, { "4:3", 4.0f / 3.0f }, { "1:1", 1.0f }, { "9:16", 9.0f / 16.0f } } };
        const auto freeAspect{ MKT_LOC( "viewport_toolbar_free_aspect" ) };
        core::cstr aspectLabel{ freeAspect.c_str() };

        for ( const auto &preset: aspectPresets ) {
            if ( preset.mRatio == mAspectRatio && preset.mRatio != 0.0f ) {
                aspectLabel = preset.mName;
            }
        }

        const float aspectWidth{ compact ? 82.0f * scale : ImGui::CalcTextSize( freeAspect.c_str() ).x + 42.0f * scale };

        const bool orthographic{ mEditorState->mActiveCamera && mEditorState->mActiveCamera->IsOrthographic() };
        const auto projectionLabel{ MKT_LOC( orthographic ? "viewport_toolbar_orthographic" : "viewport_toolbar_perspective" ) };
        const auto projectionButtonLabel{ string::Format( "{}###SceneProjection", compact ? ICON_MD_VIDEOCAM : projectionLabel.c_str() ) };
        const float projectionWidth{ compact ? buttonSize.x : ImGui::CalcTextSize( projectionLabel.c_str() ).x + 12.0f * scale };

        // Reserve both formats so switching units does not move the toolbar.
        const float counterWidth{ glm::max( ImGui::CalcTextSize( "9999 FPS" ).x, ImGui::CalcTextSize( "999.99 ms" ).x ) };

        // Classic keeps the settings button in the viewport corner, separate
        // from the original vertical tools. Both layouts share the same popup.
        if ( classic ) {
            ImGui::SetCursorScreenPos( { mContentViewport.mX + glm::max( 8.0f * scale, mContentViewport.mWidth - buttonSize.x - 8.0f * scale ),
                mContentViewport.mY + 8.0f * scale } );

            if ( imgui::widget::IconButton( "SceneViewSettings", ICON_MD_SETTINGS, buttonSize ) ) {
                ImGui::OpenPopup( imgui::widget::LocalizedLabel( "viewport_scene_view_settings", "Scene view settings" ).c_str() );
            }

            imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_scene_view_settings" ).c_str() );
            mPickingBlockedByOverlay |= ImGui::IsItemHovered() || ImGui::IsItemActive();
        }
        else {
            const float width{ projectionWidth + aspectWidth + buttonSize.x * 4.0f + 24.0f * scale +
                               ( showFps ? counterWidth + buttonSize.x + 16.0f * scale : 0.0f ) };
            float top{ 8.0f * scale + ( mEditorState->mViewSettings.mShowViewportControls ? buttonSize.y + 2.0f * kToolbarInset * scale : 0.0f ) };
            // Narrow viewports get a second toolbar row instead of overlapping transform controls.
            if ( mEnableUtilityOverlay && mContentViewport.mWidth < 900.0f * scale ) {
                top += buttonSize.y + ( 2.0f * kToolbarInset + 8.0f ) * scale;
            }

            const ImVec2 size{ width, buttonSize.y + 2.0f * kToolbarInset * scale };
            const auto minimum{ DrawToolbarBackground(
                { mContentViewport.mX + glm::max( 8.0f * scale, mContentViewport.mWidth - width - 8.0f * scale ), mContentViewport.mY + top }, size, mViewToolbarOffset ) };
            ImGui::SetCursorScreenPos( { minimum.x + kToolbarInset * scale, minimum.y + kToolbarInset * scale } );
            ImGui::BeginGroup();
            {
                const imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_FramePadding, ImVec2{ 6.0f * scale, ( buttonSize.y - ImGui::GetFontSize() ) * 0.5f } };
                const imgui::ImGuiScopedStyleVar spacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 2.0f * scale, 2.0f * scale } };
                const imgui::ImGuiScopedStyleVar comboPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 8.0f * scale, 8.0f * scale } };
                const imgui::ImGuiScopedStyleVar rounding{ ImGuiStyleVar_FrameRounding, 4.0f * scale };
                ImGui::PushStyleColor( ImGuiCol_Button, ImVec4{ 0, 0, 0, 0 } );
                ImGui::SetNextItemWidth( aspectWidth );

                if ( ImGui::BeginCombo( "##SceneAspect", aspectLabel ) ) {
                    mPickingBlockedByOverlay = true;

                    for ( const auto &preset: aspectPresets ) {
                        const bool selected{ mAspectRatio == preset.mRatio };

                        if ( ImGui::Selectable( preset.mRatio == 0.0f ? freeAspect.c_str() : preset.mName, selected ) ) {
                            mAspectRatio = preset.mRatio;
                        }

                        if ( selected ) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_toolbar_aspect_ratio" ).c_str() );
                mPickingBlockedByOverlay |= ImGui::IsItemActive();
                ImGui::SameLine();
                const bool projectionClicked{ compact ? imgui::widget::IconButton( "SceneProjection", ICON_MD_VIDEOCAM, buttonSize )
                                                      : ImGui::Button( projectionButtonLabel.c_str(), { projectionWidth, buttonSize.y } ) };

                if ( projectionClicked && mEditorState->mRequestEditorProjection ) {
                    mEditorState->mRequestEditorProjection( orthographic ? scene::ProjectionType::ePerspective : scene::ProjectionType::eOrthographic );
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( orthographic ? "viewport_tool_orthographic" : "viewport_tool_perspective" ).c_str() );
                ImGui::SameLine();
                auto &view{ mEditorState->mViewSettings };

                if ( imgui::widget::ToggleIconButton( "SceneGrid", ICON_MD_GRID_ON, view.mShowGrid, buttonSize ) ) {
                    view.mShowGrid = !view.mShowGrid;
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_show_grid" ).c_str() );
                ImGui::SameLine();

                if ( imgui::widget::ToggleIconButton( "SceneGizmos", ICON_MD_OPEN_WITH, view.mShowGizmos, buttonSize ) ) {
                    view.mShowGizmos = !view.mShowGizmos;
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_show_gizmos" ).c_str() );
                ImGui::SameLine();
                auto *physics{ mEditorState->mEditorRenderer ? &mEditorState->mEditorRenderer->GetPhysicsDebugSettings() : nullptr };
                ImGui::BeginDisabled( !physics );

                if ( imgui::widget::ToggleIconButton( "ScenePhysics", ICON_MD_BUG_REPORT, physics && physics->mEnabled, buttonSize ) && physics ) {
                    physics->mEnabled = !physics->mEnabled;
                }

                ImGui::EndDisabled();
                imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_tool_physics" ).c_str() );
                ImGui::SameLine();

                if ( imgui::widget::IconButton( "SceneViewSettings", ICON_MD_SETTINGS, buttonSize ) ) {
                    ImGui::OpenPopup( imgui::widget::LocalizedLabel( "viewport_scene_view_settings", "Scene view settings" ).c_str() );
                }

                imgui::widget::ItemTooltip( "%s", MKT_LOC( "viewport_scene_view_settings" ).c_str() );
                mPickingBlockedByOverlay |= ImGui::IsItemHovered() || ImGui::IsItemActive();

                if ( showFps ) {
                    ImGui::SameLine();
                    const auto divider{ ImGui::GetCursorScreenPos() };
                    ImGui::GetWindowDrawList()->AddLine( { divider.x + 5.0f * scale, divider.y + 4.0f * scale },
                        { divider.x + 5.0f * scale, divider.y + buttonSize.y - 4.0f * scale }, IM_COL32( 80, 83, 88, 255 ) );
                    ImGui::Dummy( { 10.0f * scale, buttonSize.y } );
                    ImGui::SameLine();
                    static const eastl::string icon{ imgui::GetStringFromUnicode( 60424 ) };

                    if ( imgui::widget::IconButton( "SceneFrameTiming", icon.c_str(), buttonSize ) ) {
                        mShowFrameTime = !mShowFrameTime;
                    }

                    mPickingBlockedByOverlay |= ImGui::IsItemHovered() || ImGui::IsItemActive();
                    imgui::widget::ItemTooltip( "%s", MKT_LOC( mShowFrameTime ? "viewport_show_fps" : "viewport_show_frame_time" ).c_str() );
                    ImGui::SameLine( 0.0f, 6.0f * scale );
                    ImGui::AlignTextToFramePadding();
                    const float fps{ ImGui::GetIO().Framerate };

                    if ( mShowFrameTime ) {
                        ImGui::Text( "%.2f ms", fps > 0.0f ? 1000.0f / fps : 0.0f );
                    }
                    else {
                        ImGui::Text( "%.0f FPS", fps );
                    }
                }

                ImGui::PopStyleColor();
            }

            ImGui::EndGroup();
            const ImRect content{ ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
            DragToolbar( "##ViewToolbarDrag", minimum, size, content, mViewToolbarOffset );
        }

        // The Scene viewport uses zero padding, restore comfortable popup spacing.
        imgui::ImGuiScopedStyleVar popupPadding{ ImGuiStyleVar_WindowPadding, ImVec2{ 10.0f, 10.0f } };
        imgui::ImGuiScopedStyleVar itemSpacing{ ImGuiStyleVar_ItemSpacing, ImVec2{ 8.0f, 6.0f } };

        ImGui::SetNextWindowSizeConstraints( { 310, 0 }, { 410, 800 } );

        if ( ImGui::BeginPopup( imgui::widget::LocalizedLabel( "viewport_scene_view_settings", "Scene view settings" ).c_str() ) ) {
            mPickingBlockedByOverlay = true;
            auto &view{ mEditorState->mViewSettings };

            int toolsLayout{ core::as<int>( view.mToolsLayout ) };
            const eastl::array<const char *, 2> layouts{ MKT_LOC( "viewport_tools_layout_default" ).c_str(), MKT_LOC( "viewport_tools_layout_classic" ).c_str() };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_tools_layout", "Viewport tools" ).c_str(), &toolsLayout, layouts.data(), core::as<int>( layouts.size() ) ) ) {
                view.mToolsLayout = static_cast<EditorState::ViewSettings::ToolsLayout>( toolsLayout );
            }

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "viewport_restore_toolbar_positions", "Restore toolbar positions" ).c_str() ) ) {
                mTransformToolbarOffset = {};
                mViewToolbarOffset = {};
                mClassicToolbarOffset = {};
            }

            if ( classic && ImGui::BeginCombo( imgui::widget::LocalizedLabel( "viewport_toolbar_aspect_ratio", "Aspect ratio" ).c_str(), aspectLabel ) ) {
                for ( const auto &preset: aspectPresets ) {
                    const bool selected{ mAspectRatio == preset.mRatio };

                    if ( ImGui::Selectable( preset.mRatio == 0.0f ? freeAspect.c_str() : preset.mName, selected ) ) {
                        mAspectRatio = preset.mRatio;
                    }

                    if ( selected ) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            imgui::CheckBox( MKT_LOC( "viewport_show_play_controls" ).c_str(), view.mShowViewportControls );
            auto &orientationAnchor{ classic ? view.mClassicOrientationAnchor : view.mOrientationAnchor };
            int anchor{ core::as<int>( orientationAnchor ) };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_orientation_anchor", "OrientationAnchor" ).c_str(),
                     &anchor, MKT_LOC( "viewport_orientation_anchor_options" ).c_str() ) ) {
                orientationAnchor = static_cast<EditorState::ViewSettings::OrientationAnchor>( anchor );
            }

            ImGui::SeparatorText( MKT_LOC( "viewport_general" ).c_str() );
            int mode = view.mSelectRoot ? 1 : 0;

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_selection_mode", "Selection mode" ).c_str(), &mode, MKT_LOC( "viewport_entity_hierarchy_root" ).c_str() ) ) {
                view.mSelectRoot = mode == 1;
            }

            int pivot = mEditorState->mCenterPivot ? 0 : 1;

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_transform_pivot", "Transform pivot" ).c_str(), &pivot, MKT_LOC( "viewport_bounds_center_entity_origin" ).c_str() ) ) {
                mEditorState->mCenterPivot = pivot == 0;
            }

            ImGui::Spacing();
            ImGui::SeparatorText( MKT_LOC( "viewport_display" ).c_str() );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_icons", "Show icons" ).c_str(), view.mShowIcons );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_camera_frustums", "Show camera frustums" ).c_str(), view.mShowCameraFrustums );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_point_light_ranges", "Show point light ranges" ).c_str(), view.mShowPointLightRanges );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_spot_light_ranges", "Show spot light ranges" ).c_str(), view.mShowSpotLightRanges );
            int volumeVisibility{ view.mCameraLightVolumesSelectedOnly ? 0 : 1 };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_volume_visibility", "Volume visibility" ).c_str(), &volumeVisibility, MKT_LOC( "viewport_selected_only_always" ).c_str() ) ) {
                view.mCameraLightVolumesSelectedOnly = volumeVisibility == 0;
            }

            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "viewport_controls_enabled_camera_frustums_and_point_spot_light_ranges" ).c_str() );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_gizmos", "Show gizmos" ).c_str(), view.mShowGizmos );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_gizmos_in_play_mode", "Show gizmos in Play mode" ).c_str(), view.mShowGizmosInPlayMode );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_bounding_boxes", "Show bounding boxes" ).c_str(), view.mShowBounds );
            int vertexVisibility{ core::as<int>( view.mSoftBodyVertexVisibility ) };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_cloth_vertices", "Cloth vertices" ).c_str(),
                     &vertexVisibility, MKT_LOC( "viewport_cloth_vertices_options" ).c_str() ) ) {
                view.mSoftBodyVertexVisibility = static_cast<EditorState::ViewSettings::SoftBodyVertexVisibility>( vertexVisibility );
            }

            ImGui::TextUnformatted( MKT_LOC( "viewport_wind_zones" ).c_str() );
            ImGui::SameLine();
            ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
            int windDebugView{ core::as<int>( view.mWindDebugView ) };

            if ( ImGui::Combo( "##WindZones", &windDebugView, MKT_LOC( "viewport_wind_zone_modes" ).c_str() ) ) {
                view.mWindDebugView = static_cast<renderer::WindDebugView>( windDebugView );
            }

            imgui::widget::ItemTooltip( MKT_LOC( "viewport_wind_zones_hint" ).c_str() );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_grid", "Show grid" ).c_str(), view.mShowGrid );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_selected_wireframe", "Show selected wireframe" ).c_str(), view.mShowSelectedWireframe );
            int armatureMode{ core::as<int>( view.mArmatureDebugMode ) };

            if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_armature", "Armature" ).c_str(),
                     &armatureMode, MKT_LOC( "viewport_armature_modes" ).c_str() ) ) {
                view.mArmatureDebugMode = static_cast<renderer::ArmatureDebugMode>( armatureMode );
            }
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_renderer_info", "Renderer info" ).c_str(), mEnablePerformanceOverlay );

            if ( mEditorState->mEditorRenderer ) {
                auto &physics{ mEditorState->mEditorRenderer->GetPhysicsDebugSettings() };
                imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_show_physics_colliders", "Show physics colliders" ).c_str(), physics.mEnabled );
                int filter{ physics.mSelectedOnly ? 1 : 0 };

                if ( ImGui::Combo( imgui::widget::LocalizedLabel( "viewport_physics_collider_mode", "Physics collider mode" ).c_str(), &filter, MKT_LOC( "viewport_all_entities_selected_entity" ).c_str() ) ) {
                    physics.mSelectedOnly = filter == 1;
                }

                physics.mSelectedEntity = mEditorState->mSelectedEntityID;
            }

            ImGui::SliderFloat( imgui::widget::LocalizedLabel( "viewport_line_width", "Line width" ).c_str(), &view.mLineWidth, 1.0f, 6.0f, "%.1f px" );
            ImGui::Spacing();
            ImGui::SeparatorText( MKT_LOC( "viewport_scene_camera" ).c_str() );
            imgui::CheckBox( imgui::widget::LocalizedLabel( "viewport_override_exposure", "Override exposure" ).c_str(), view.mExposureOverride );

            if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "viewport_exposure", "Exposure" ).c_str(), &view.mExposure, 0.05f, 10.0f ) ) {
                view.mExposureOverride = true;
            }

            if ( auto *camera{ mEditorState->mEditorCamera } ) {
                auto speed{ camera->GetMovementSpeed() };

                if ( ImGui::DragFloat( imgui::widget::LocalizedLabel( "viewport_speed", "Speed" ).c_str(), &speed, 0.5f, 0.01f, 5000.0f, "%.2f u/s", ImGuiSliderFlags_AlwaysClamp ) ) {
                    camera->SetMovementSpeed( speed );
                }
            }

            ImGui::EndPopup();
        }

        if ( mEditorState->mEditorRenderer ) {
            auto &physics = mEditorState->mEditorRenderer->GetPhysicsDebugSettings();
            physics.mSelectedEntity = mEditorState->mSelectedEntityID;
            physics.mLineWidth = mEditorState->mViewSettings.mLineWidth;
        }

        ImGui::SetCursorPos( cursor );
        ImGui::Dummy( { 0, 0 } );
    }

    auto ViewportPanel::DrawSceneAnnotations() -> void {
        const auto &settings = mEditorState->mViewSettings;
        auto *scene = mEditorState->mActiveScene;

        if ( !scene || !mEditorState->mActiveCamera ||
             ( !settings.mShowIcons && !settings.mShowSelectedWireframe ) ) {
            return;
        }

        auto *draw = ImGui::GetWindowDrawList();
        draw->PushClipRect( { mViewport.mX, mViewport.mY }, { mViewport.mX + mViewport.mWidth, mViewport.mY + mViewport.mHeight }, true );
        const auto vp = mEditorState->mActiveCamera->GetViewProjection();

        const auto project = [&]( core::float4 p ) -> ImVec2 {
            p /= p.w;
            return { mViewport.mX + ( p.x * 0.5f + 0.5f ) * mViewport.mWidth,
                mViewport.mY + ( 0.5f - p.y * 0.5f ) * mViewport.mHeight };
        };

        const auto line = [&]( core::float3 a, core::float3 b, ImU32 color, float width = 0.0f ) {
            core::float4 p = vp * float4( a, 1 );
            core::float4 q = vp * float4( b, 1 );

            // Clip all six planes before dividing: large light ranges and far
            // frustums must not generate enormous screen-space line coordinates.
            constexpr core::float4 planes[]{
                { 1, 0, 0, 1 },
                { -1, 0, 0, 1 },
                { 0, 1, 0, 1 },
                { 0, -1, 0, 1 },
                { 0, 0, 1, 0 },
                { 0, 0, -1, 1 }
            };

            for ( const auto &plane: planes ) {
                const float da{ glm::dot( p, plane ) };
                const float db{ glm::dot( q, plane ) };

                if ( ( da < 0 && db < 0 ) ) {
                    return;
                }

                if ( ( da < 0 ) != ( db < 0 ) ) {
                    const auto intersection{ glm::mix( p, q, da / ( da - db ) ) };

                    if ( da < 0 ) {
                        p = intersection;
                    }
                    else {
                        q = intersection;
                    }
                }
            }

            if ( p.w <= 0 || q.w <= 0 ) {
                return;
            }

            draw->AddLine( project( p ), project( q ), color, width > 0.0f ? width : settings.mLineWidth );
        };

        // BuildSkinningPalette initializes every entry before a successful palette is read.
        // Reuse the storage; static meshes and icon-only overlays never need it filled.
        eastl::array<core::float4x4, asset::kMaxBonesPerMesh> palette;

        for ( const auto &entry: scene->GetEntities() ) {
            const auto id{ entry.first };
            const auto &entity{ *entry.second };

            if ( !entity.GetComponent<scene::TagComponent>().IsActive() ) {
                continue;
            }

            const auto world{ entity.GetComponent<scene::TransformComponent>().GetWorldTransform() };

            if ( settings.mShowIcons ) {
                std::optional<OverlayIcon> icon;

                if ( entity.HasComponent<scene::LightComponent>() ) {
                    icon = entity.GetComponent<scene::LightComponent>().GetActiveType() == renderer::LightType::eDirectional
                               ? OverlayIcon::eSun
                               : OverlayIcon::eLight;
                }
                else if ( entity.HasComponent<scene::CameraComponent>() ) {
                    icon = OverlayIcon::eCamera;
                }
                else if ( entity.HasComponent<scene::AudioSourceComponent>() ) {
                    icon = OverlayIcon::eAudio;
                }
                else if ( entity.HasComponent<scene::TextComponent>() ) {
                    icon = OverlayIcon::eText;
                }
                else if ( entity.HasComponent<scene::CanvasComponent>() ) {
                    icon = OverlayIcon::eCanvas;
                }

                const auto p = vp * world[3];

                if ( icon && p.w > 0 && p.z >= 0 && p.z <= p.w ) {
                    const auto &texture{ mOverlayIcons[static_cast<core::usize>( *icon )] };

                    if ( texture ) {
                        const auto center{ project( p ) };
                        constexpr float halfSize{ 14.0f };
                        draw->AddImage( imgui::ImGuiService::Get()->GetTextureID( texture ),
                            { center.x - halfSize, center.y - halfSize },
                            { center.x + halfSize, center.y + halfSize } );
                    }
                }
            }

            if ( !settings.mShowSelectedWireframe || settings.mArmatureDebugMode == renderer::ArmatureDebugMode::eArmatureOnly ) {
                continue;
            }

            bool selected{};

            if ( settings.mShowSelectedWireframe ) {
                selected = id == mEditorState->mSelectedEntityID;

                for ( auto parent = entity.GetComponent<scene::RelationComponent>().GetParent(); !selected && parent; ) {
                    selected = *parent == mEditorState->mSelectedEntityID;
                    auto *ancestor = scene->FindByID( *parent );
                    parent = ancestor ? ancestor->GetComponent<scene::RelationComponent>().GetParent() : eastl::nullopt;
                }
            }

            const bool showWireframe{ selected && settings.mShowSelectedWireframe };

            if ( !showWireframe ) {
                continue;
            }

            if ( !entity.HasComponent<scene::MeshComponent>() || !entity.GetComponent<scene::MeshComponent>().HasMesh() ) {
                continue;
            }

            const auto *mesh = entity.GetComponent<scene::MeshComponent>().GetMesh();
            const animation::Animator *animator{};
            bool skinned{};

            if ( entity.HasComponent<scene::SkinnedMeshRenderer>() ) {
                auto *animations = animation::AnimationSystem::GetPtr();
                animator = animations ? animations->GetAnimator( entity.GetComponent<scene::SkinnedMeshRenderer>().GetAnimatorID() ) : nullptr;

                if ( animator && showWireframe ) {
                    const auto &pose = animator->GetJointModelMatrices( mesh->GetSkinBinding().mRigIndex );
                    skinned = animation::BuildSkinningPalette( mesh->GetSkinBinding(), { pose.data(), pose.size() }, palette );
                }
            }

            if ( showWireframe ) {
                const auto &vertices = mesh->GetCollisionVertices();
                const auto &indices = mesh->GetCollisionIndices();
                const auto &skin = mesh->GetSkinningVertices();

                eastl::vector<core::float3> positions{};
                positions.reserve( vertices.size() );

                for ( core::usize i = 0; i < vertices.size(); ++i ) {
                    const auto p = skinned && i < skin.size() ? animation::SkinVertexPosition( skin[i], palette ) : vertices[i];
                    positions.push_back( float3( world * float4( p, 1 ) ) );
                }

                for ( core::usize i = 0; i + 2 < indices.size(); i += 3 ) {
                    if ( indices[i] >= positions.size() || indices[i + 1] >= positions.size() || indices[i + 2] >= positions.size() ) {
                        continue;
                    }

                    for ( int edge = 0; edge < 3; ++edge ) {
                        line( positions[indices[i + edge]], positions[indices[i + ( edge + 1 ) % 3]], IM_COL32( 255, 210, 100, 160 ) );
                    }
                }
            }
        }

        draw->PopClipRect();
    }
}// namespace mikoto::editor
