/**
 * ScenePanel.hh
 * Created by kate on 6/27/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_HH
#define MIKOTO_SCENE_PANEL_HH

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Panels/Panel.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Scene/Scene/Scene.hh>

namespace Mikoto {

    struct SceneApiCreateInfo {
        UInt32_T ViewportWidth{};
        UInt32_T ViewportHeight{};

        Scene* TargetScene{};
        const RendererBackend* Renderer{};

        const SceneCamera* EditorMainCamera{};
        std::function<Entity*()> GetActiveEntityCallback{};
    };

    enum class GuizmoManipulationMode {
        TRANSLATION,
        ROTATION,
        SCALE,
    };

    /**
     * Abstracts away the active ImGui render backend according
     * to the currently active graphics API.
     * */
    class ScenePanelApi {
    public:
        explicit ScenePanelApi(const SceneApiCreateInfo& createInfo)
            : m_ViewPortWidth{ static_cast<float>( createInfo.ViewportWidth ) },
            m_ViewPortHeight{ static_cast<float>( createInfo.ViewportHeight ) },
            m_TargetScene{ createInfo.TargetScene },
            m_Renderer{ createInfo.Renderer },
            m_EditorMainCamera{ createInfo.EditorMainCamera },
            m_GetActiveEntityCallback{ createInfo.GetActiveEntityCallback }
        {}

        virtual auto Init() -> void = 0;
        virtual auto OnUpdate() -> void = 0;

        auto SetupGuizmos() const -> void;

        auto SetEditorCamera(const SceneCamera* camera) -> void {
            m_EditorMainCamera = camera;
        }

        MKT_NODISCARD auto GetViewportWidth() const -> float { return m_ViewPortWidth; }
        MKT_NODISCARD auto GetViewportHeight() const -> float { return m_ViewPortHeight; }

        virtual ~ScenePanelApi() = default;

    protected:

        auto HandleManipulationMode() const -> void;

    protected:
        float m_ViewPortWidth{};
        float m_ViewPortHeight{};

        Scene* m_TargetScene{};

        const RendererBackend* m_Renderer{};

        const SceneCamera* m_EditorMainCamera{};
        GuizmoManipulationMode m_ActiveManipulationMode{};
        std::function<Entity*()> m_GetActiveEntityCallback{};
    };


    struct ScenePanelCreateInfo {
        UInt32_T Width{};
        UInt32_T Height{};

        Scene* TargetScene{};
        const RendererBackend* Renderer{};
        const SceneCamera* EditorMainCamera{};

        std::function<Entity*()> GetActiveEntityCallback{};
    };

    class ScenePanel final : public Panel {
    public:
        explicit ScenePanel(const ScenePanelCreateInfo& createInfo);

        auto OnUpdate(float ts) -> void override;

        MKT_NODISCARD auto GetViewportWidth() const -> float { return m_Implementation->GetViewportWidth(); }
        MKT_NODISCARD auto GetViewportHeight() const -> float { return m_Implementation->GetViewportHeight(); }

        ~ScenePanel() override = default;

    protected:
        Scope_T<ScenePanelApi> m_Implementation{};
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
