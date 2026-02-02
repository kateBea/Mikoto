//    Copyright 2025 ケイト
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

#include <memory>

#include <ankerl/unordered_dense.h>

#include <Assets/Model.hh>
#include <Core/LayerStack.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Library/Data/Registry.hh>
#include <Material/TextureCube.hh>
#include <Panels/Panel.hh>
#include <Platform/Window.hh>
#include <Scene/Entity.hh>
#include <Scene/Scene.hh>
#include <Scene/SceneCamera.hh>
#include <Scene/SceneSerializer.hh>
#include <Renderer/Core/SceneRenderer.hh>

namespace Mikoto {

    struct EditorState {
        Entity* SelectedEntity{};

        // Used when scene is not simulating
        SceneCamera* EditorCamera{};

        // Scene currently active
        Scene* ActiveEditorScene{};

        // The final composition from the scene renderer
        TextureHandle FinalComposition{};
        TextureHandle WireframeComposition{};

        // Debug
        TextureHandle PreviewMaterial{};

        TextureHandle TextureHDR_2D{};

        // Pass name and output value
        ankerl::unordered_dense::map<std::string, TextureHandle> PassesCompositions{};

        // Editor specifies which texture gets rendered on the window
        TextureHandle RenderImage{};

        SceneRenderer* EditorSceneRenderer{};

        ImGuiUtils::GuizmoManipulationMode Manipulation{ ImGuiUtils::GuizmoManipulationMode::TRANSLATION };

        bool ApplicationCloseFlag{ true };
        bool ShowHeatMap{ false };
        bool ShowWireframe{ false };
    };

    class EditorLayer final : public ILayer {
    public:
        explicit EditorLayer(Window* window);

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate(float timeStep) -> void override;

        auto OnEvent(Event &event) -> void override;

    private:
        auto UpdatePanels(float timeStep) -> void;

        auto SaveScene() const -> void;
        auto LoadScene() -> void;
        auto InitializeEmptyScene(std::string_view name) -> void;

        auto SaveProject() -> void;
        auto OpenProject() -> void;
        auto CreateProject() -> void;

        auto CreatePanels() -> void;
        auto CreateCameras() -> void;
        auto HandleWindowScreenMode() const -> void;
        auto SetRendererResolution() const -> void;
        auto UpdateDockSpace() -> void;

        auto PrepareNewScene() -> void;

        auto PrepareRenderer(double timeStep) -> void;
        auto PrepareCamera(double timeStep) -> void;

        auto SetupRenderer() -> void ;

        auto SetupEditorState() -> void ;

        auto SetupPresentTarget(Event& event) -> void;

        auto LoadResources() -> void;

        auto SetPresentTarget() -> void;

    private:
        enum class RenderScreenTarget { WINDOW, PANEL };

    private:
        Unique<EditorState> m_EditorState{};

        Window* m_Window{ nullptr };

        Path m_ModelsRootDirectory{};
        Path m_FontsRootDirectory{};

        Scene* m_ActiveScene{};
        Unique<SceneRenderer> m_SceneRenderer{};

        TextureHandle m_TextureCubeMap{};
        TextureHandle m_TextureHDR{};

        Unique<SceneCamera> m_EditorCamera{};

        Registry<Panel> m_PanelRegistry{};

        RenderScreenTarget m_RenderScreenTarget{ RenderScreenTarget::PANEL };
    };
}

#endif // MIKOTO_EDITOR_LAYER_HH
