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

#ifndef MIKOTO_EDITOR_LAYER_HH
#define MIKOTO_EDITOR_LAYER_HH

#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Core.hh>
#include <Core/Event.hh>
#include <Core/String.hh>
#include <Core/Registry.hh>
#include <Core/LayerStack.hh>
#include <Core/ActionManager.hh>

#include <Platform/Window.hh>

#include <ImGui/GraphEditor.hh>

#include <Theme/Theme.hh>
#include <Assets/Model.hh>
#include <Panels/Panel.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/Core/ThumbnailRenderer.hh>
#include <Renderer/Core/PhysicsDebugRenderer.hh>

#include <Project/Project.hh>
#include <Project/ProjectSerializer.hh>

namespace mikoto::editor {

    enum class PrefabModelType {
        eCube,
        eSphere,
        eCone,
        eCylinder,
    };

    enum class ScreenPresentTarget {
        ePanels,
        eFinalImage
    };

    struct EditorState {
        scene::Scene* mActiveScene{};
        scene::Entity* mSelectedEntity{};
        scene::SceneCamera* mActiveCamera{};
        renderer::SceneRenderer* mSceneRenderer{};
        renderer::ThumbnailRenderer* mThumbnailRenderer{};
        renderer::PhysicsDebugRenderer* mPhysicsDebugRenderer{};
        renderer::PhysicsDebugRendererSimple* mPhysicsDebugRendererSimple{};

        Theme* mActiveTheme{};

        // Image that will be used for presentation
        renderer::rhi::TextureHandle mFinalComposition{};

        // Current desired resolution. If changed a resize on the final
        // composition should be performed.
        renderer::rhi::RenderResolution mResolution{ renderer::rhi::RenderResolution::e1080P };

        // List of prefab models
        ankerl::unordered_dense::map<PrefabModelType, asset::ModelHandle> mPrefabPaths{};

        MKT_NODISCARD auto GetPrefab( PrefabModelType model ) -> asset::ModelHandle;
    };

    class EditorLayer final : public core::ILayer {
    public:
        explicit EditorLayer( platform::Window* window );

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float timeStep ) -> void override;

        auto OnEvent( core::IEvent& event ) -> void override;

    private:
        auto InitAssets() -> void;
        auto InitEditorState() -> void;
        auto InitSceneRenderer() -> void;
        auto InitEditorCamera() -> void;
        auto InitEditorPanels() -> void;
        auto InitDockingSpace() -> void;
        auto InitActionCallbacks() -> void;
        auto InitRenderGraphEditor() -> void;

        auto InitLastProject() -> void;

        auto InitEmptyScene() -> void;

        auto InitInstancingTestScene() -> void;
        auto InitSphereMaterialsScene() -> void;

        auto UpdateDockSpace() -> void;
        auto UpdateDockSpacePanels( float ts ) -> void;

        auto RenderScene( float ts ) -> void;

        auto RenderFrameGraphEditor() -> void;

        auto UpdateViewportState( float ts ) -> void;
        auto UpdateSceneState( float ts ) -> void;
        auto UpdateCameraState( float ts ) -> void;
        auto UpdateRendererState( float ts ) -> void;

        auto OpenProject() -> void;
        auto SaveProject() -> void;
        auto CreateProject() -> void;

    private:
        platform::Window* mWindow{};
        renderer::rhi::IGpuDevice* mDevice{};
        core::Registry<Panel> mPanelRegistry{};

        eastl::unique_ptr<EditorState> mEditorState{};
        eastl::unique_ptr<scene::SceneCamera> mEditorCamera{};
        eastl::unique_ptr<renderer::SceneRenderer> mSceneRenderer{};
        eastl::unique_ptr<renderer::ThumbnailRenderer> mThumbnailRenderer{};

        eastl::unique_ptr<core::ActionManager> mActionManager{};

        project::ProjectHandle mProject{};
        project::ProjectSerializer mProjectSerializer{};

        // Renderer display settings
        bool mShowRenderGraph{};
        bool mShowInfiniteGrid{};
        bool mShowImGuiDebugInfo{};
        bool mShowPolygonComplexity{};

        gui::GraphEditor mGraphEditor{ "RenderGraph" };

        ScreenPresentTarget mScreenPresentTarget{ ScreenPresentTarget::ePanels };
    };
}// namespace mikoto::editor

#endif // MIKOTO_EDITOR_LAYER_HH
