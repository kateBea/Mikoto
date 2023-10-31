/**
 * ScenePanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_HH
#define MIKOTO_SCENE_PANEL_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Common/Types.hh"
#include "HierarchyPanel.hh"
#include "Panel.hh"

namespace Mikoto {
    struct ScenePanelCreateInfo {
        // This camera is holds the eye to the world (the scene we are editing)
        const EditorCamera* EditorMainCamera{};
    };

    struct ScenePanelData {
        float ViewPortWidth{};
        float ViewPortHeight{};
    };

    /**
     * Abstracts away the active ImGui render backend according
     * to the currently active graphics API.
     * */
    class ScenePanelInterface {
    public:
        explicit ScenePanelInterface() = default;

        virtual auto Init_Impl(ScenePanelData&& data) -> void = 0;
        virtual auto OnUpdate_Impl() -> void = 0;

        auto HandleGuizmos() -> void;

        auto SetEditorCamera(const EditorCamera* camera) -> void {
            m_EditorMainCamera = camera;
        }

        MKT_NODISCARD auto GetData() const -> const ScenePanelData& { return m_Data; }
        MKT_NODISCARD auto GetData() -> ScenePanelData& { return m_Data; }

        virtual ~ScenePanelInterface() = default;

    protected:
        enum class ManipulationMode {
            TRANSLATION,
            ROTATION,
            SCALE,
        };

        auto HandleManipulationMode() -> void;

    protected:
        ScenePanelData m_Data{};
        const EditorCamera* m_EditorMainCamera{};
        ManipulationMode m_ActiveManipulationMode{};
    };


    class ScenePanel : public Panel {
    public:
        explicit ScenePanel(ScenePanelCreateInfo&& createInfo);
        auto operator=(ScenePanel&& other) -> ScenePanel& = default;

        auto OnUpdate(float ts) -> void override;

        MKT_NODISCARD auto GetData() const -> const ScenePanelData& { return m_Implementation->GetData(); }
        MKT_NODISCARD auto GetData() -> ScenePanelData& { return m_Implementation->GetData(); }

        ~ScenePanel() override = default;

    private:
        auto DrawScenePlayButtons() -> void;

    protected:
        ScenePanelCreateInfo m_CreateInfo{};

        // Contains the ImGui implementation for the currently active graphics backend
        std::unique_ptr<ScenePanelInterface> m_Implementation{};
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
