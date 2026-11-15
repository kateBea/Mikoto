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

#ifndef MIKOTO_PANELS_VIEWPORT_PANEL_HH
#define MIKOTO_PANELS_VIEWPORT_PANEL_HH

#include <imgui.h>

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>

#include <Assets/AssetModel.hh>
#include <Assets/AssetTexture.hh>
#include <Assets/AssetMaterial.hh>
#include <Assets/AssetLoadRegistry.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/ForwardRenderer/Passes/MousePickingModule.hh>

#include <Scene/Scene.hh>

#include <ImGui/Utility.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {

    struct EditorState;

    struct ViewportPanelCreateInfo {
        EditorState* mState{};

        core::u32 mInstanceId{ 1 };
        renderer::rhi::IGpuDevice* mGpuDevice{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class ViewportPanel final : public Panel {
    public:
        explicit ViewportPanel(const ViewportPanelCreateInfo& createInfo);

        auto OnRender(float ts) -> void override;

        auto SuspendInput() -> void;

        auto CancelPicking() -> void;
        auto UpdateFullscreenPicking() -> void;

        auto SetGizmoType( imgui::GizmoType type ) -> void;
        auto SetGizmoMode( imgui::GizmoMode mode ) -> void;
        auto SetTexture( renderer::rhi::TextureHandle texture ) -> void;

        auto SetEnableUtilityOverlay( bool enable ) -> void;
        auto SetEnablePerformanceOverlay( bool enable ) -> void;

        MKT_NODISCARD auto GetWidth() const -> float;
        MKT_NODISCARD auto GetHeight() const -> float;
        MKT_NODISCARD auto IsCameraNavigating() const -> bool;

        ~ViewportPanel() override;

    private:
        enum class CursorTool : core::u8 {
            eSelect,
            eGrabSoftBody
        };

        enum class RightMouseGesture : core::u8 {
            eIdle,
            ePendingClick,
            eNavigate
        };

        enum class OverlayIcon : core::usize {
            eCamera, eLight, eSun, eText, eCanvas, eAudio, eCount
        };

        struct CachedMeshBounds {
            asset::ModelHandle mModel;
            core::usize mMeshIndex{};
            core::float4x4 mTransform{ 1.0f };
            math::GizmoBounds mBounds;
            eastl::vector<core::float4x4> mPose{};
            bool mPosed{};
            core::u64 mFrame{};
        };


        struct SoftBodyDrag {
            scene::SceneHandle mScene{};
            entt::entity mEntity{ entt::null };
            core::u32 mParticle{};
            core::f32 mDepth{};
            core::float3 mOffset{};
        };

    private:
        auto DrawOrientationAxis() -> void;

        auto AddEntityWithModel( const asset::AssetRef<asset::Model>& model, scene::Entity* root ) -> void;

        auto InitOverlayIcons() -> void;
        auto DrawFinalImage() -> void;
        auto DrawCameraFrustum() -> void;
        auto ApplyPickingResult() -> void;
        auto SetCursorTool( CursorTool tool ) -> void;
        auto UpdateCursorTool() -> void;
        auto UpdateSoftBodyTool() -> void;
        auto ReleaseSoftBodyGrab() -> void;
        auto UpdateTextureDrop() -> void;
        auto UpdateMaterialDrop() -> void;
        auto UpdateSceneContextMenu() -> void;
        auto UpdateManipulation() -> void;
        auto UpdatePicking( bool fullscreen = false ) -> void;

        auto DrawViewSettings() -> void;
        auto DrawUtilitiesOverlay() -> void;
        auto DrawSceneButtonsOverlay() -> void;
        auto DrawSceneAnnotations() -> void;
        auto DrawPerformanceOverlay( bool showOnlyFps ) -> void;
        auto DrawToolbarBackground( const ImVec2 &defaultPosition, const ImVec2 &size, ImVec2 &offset, bool classic = false ) -> ImVec2;
        auto DragToolbar( core::cstr id, const ImVec2 &minimum, const ImVec2 &size, const ImRect &content, ImVec2 &offset ) -> void;

        auto BlankSpacePopupMenu() -> void;
        auto OnEntityRightClickMenu() -> void;

        auto DrawManipulationGizmos() -> void;

        MKT_NODISCARD auto IsDisplayTextureValid() const -> bool;

    private:
        static constexpr core::f32 kToolbarButtonHeight{ 32.0f };
        static constexpr core::f32 kToolbarButtonWidth{ 34.0f };
        static constexpr core::f32 kToolbarInset{ 6.0f };

        EditorState* mEditorState{};
        renderer::rhi::IGpuDevice* mGpuDevice{};

        core::u32 mInstanceId{ 1 };

        renderer::rhi::TextureHandle mColorImage{};
        eastl::array<renderer::rhi::TextureHandle, static_cast<core::usize>( OverlayIcon::eCount )> mOverlayIcons{};

        core::f32 mViewportWidth{ 1920 };
        core::f32 mViewportHeight{ 1080 };
        core::f32 mAspectRatio{}; // Zero fills the panel; fixed ratios fit inside it.
        ViewportInfo mContentViewport{};

        ImGuiViewport* mPanelViewport{};
        ImVec2 mStartWindowPos{ 0.0f, 0.0f };
        ImVec2 mStartCursorPos{ 0.0f, 0.0f };
        ImVec2 mStartWindowSize{ 0.0f, 0.0f };

        bool mEnablePerformanceOverlay{ false };
        bool mEnableUtilityOverlay{ true };
        bool mShowFrameTime{};
        ImVec2 mTransformToolbarOffset{};
        ImVec2 mViewToolbarOffset{};
        ImVec2 mClassicToolbarOffset{};

        bool mSceneImageHovered{};
        bool mPickingBlockedByOverlay{};
        bool mOrientationDragged{};

        CursorTool mCursorTool{ CursorTool::eSelect };
        SoftBodyDrag mSoftBodyDrag{};

        imgui::GizmoType mGizmoType{ imgui::GizmoType::eTranslation };
        imgui::GizmoMode mGizmoMode{ imgui::GizmoMode::eLocal };
        bool mDraggingGizmo{};
        core::float3 mGizmoDragCenter{};
        core::RevisionTracked::Revision mGizmoDragRevision{};

        ankerl::unordered_dense::map<core::u64, CachedMeshBounds> mGizmoMeshBounds{};
        const scene::Scene* mGizmoBoundsScene{};
        core::RevisionTracked::Revision mGizmoBoundsRevision{};
        core::u64 mGizmoBoundsFrame{};

        const scene::Scene* mPickScene{};
        core::RevisionTracked::Revision mPickRevision{};
        bool mPickParent{};
        bool mPickAppend{};
        bool mPickForContext{};
        bool mPickRectangle{};
        renderer::ReadPixelViewportInfo mPendingRectangleFirst{};
        renderer::ReadPixelViewportInfo mPendingRectangleLast{};

        const scene::Scene* mBoxScene{};
        ImVec2 mBoxStart{};
        ImVec2 mBoxEnd{};
        float mBoxOpacity{};
        bool mBoxActive{};
        bool mBoxDragged{};
        bool mBoxAppend{};

        RightMouseGesture mRightMouseGesture{ RightMouseGesture::eIdle };
        const scene::Scene* mRightClickScene{};
        const scene::Scene* mContextScene{};
        core::u64 mContextEntityID{};
        ImVec2 mContextPopupPosition{};
        bool mContextReady{};

        filesystem::Path mTextureDropPath{};
        const scene::Scene* mTextureDropScene{};
        core::u32 mTextureTargetID{};
        bool mTexturePickPending{};
        bool mPickForTexture{};
        bool mTextureDelivered{};
        bool mOpenTexturePopup{};

        asset::AssetHandle<asset::Texture> mTextureRequest{};
        bool mTextureImported{};
        bool mTextureUnique{ true };
        int mTextureUsage{};
        int mTextureMap{};
        core::float4 mTextureUv{ 1, 1, 0, 0 };
        eastl::string mTextureError{};

        filesystem::Path mMaterialDropPath{};
        const scene::Scene* mMaterialDropScene{};
        asset::AssetHandle<asset::Material> mMaterialRequest{};
        renderer::ReadPixelViewportInfo mMaterialDropViewport{};
        core::u32 mMaterialTargetID{};
        bool mMaterialPickPending{};
        bool mPickForMaterial{};
        bool mMaterialDelivered{};
        bool mOpenMaterialPopup{};
        bool mMaterialUnique{};
    };
}

#endif// MIKOTO_PANELS_VIEWPORT_PANEL_HH

