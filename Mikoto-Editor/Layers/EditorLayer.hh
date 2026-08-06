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
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Event.hh>
#include <Core/Registry.hh>
#include <Core/LayerStack.hh>
#include <Core/ActionManager.hh>

#include <Assets/Model.hh>

#include <Platform/Window.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/Core/ThumbnailRenderer.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>

#include <Panels/Panel.hh>

#include <Theme/Theme.hh>

namespace mikoto::editor {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::platform;
    using namespace mikoto::renderer;

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
        Scene* mActiveScene{};
        Entity *mSelectedEntity{};
        SceneCamera* mActiveCamera{};
        SceneRenderer *mSceneRenderer{};
        ThumbnailRenderer *mThumbnailRenderer{};

        Theme* mActiveTheme{};

        // Image that will be used for presentation
        TextureHandle mFinalComposition{};

        // Current desired resolution. If changed a resize on the final
        // composition should be performed.
        RenderResolution mResolution{ RenderResolution::e1080P };

        // List of prefab models
        ankerl::unordered_dense::map<PrefabModelType, ModelHandle> mPrefabPaths{};

        MKT_NODISCARD auto GetPrefab( PrefabModelType model ) -> ModelHandle;
    };

    class EditorLayer final : public ILayer {
    public:
        explicit EditorLayer( Window* window);

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate(float timeStep) -> void override;

        auto OnEvent(IEvent &event) -> void override;

    private:
        auto InitAssets() -> void;
        auto InitEditorState() -> void;
        auto InitSceneRenderer() -> void;
        auto InitEditorCamera() -> void;
        auto InitEditorPanels() -> void;
        auto InitDockingSpace() -> void;
        auto InitActionCallbacks() -> void;

        auto InitEmptyScene() -> void;

        auto InitInstancingTestScene() -> void;
        auto InitSphereMaterialsScene() -> void;

        auto UpdateDockSpace() -> void;
        auto UpdateDockSpacePanels( float ts ) -> void;

        auto RenderScene( float ts ) -> void;

        auto UpdateViewportState( float ts ) -> void;
        auto UpdateSceneState( float ts ) -> void;
        auto UpdateCameraState( float ts  ) -> void;
        auto UpdateRendererState( float ts  ) -> void;

    private:
        Window* mWindow{};
        IGpuDevice* mDevice{};
        Registry<Panel> mPanelRegistry{};

        eastl::unique_ptr<EditorState> mEditorState{};
        eastl::unique_ptr<SceneCamera> mEditorCamera{};
        eastl::unique_ptr<SceneRenderer> mSceneRenderer{};
        eastl::unique_ptr<ThumbnailRenderer> mThumbnailRenderer{};

        eastl::unique_ptr<ActionManager> mActionManager{};

        CommandListHandle mCommandList{};

        // [DEBUG] To remove
        TextureHandle mTestSkybox{};

        ScreenPresentTarget mScreenPresentTarget{ ScreenPresentTarget::ePanels };
    };
}

#endif // MIKOTO_EDITOR_LAYER_HH
