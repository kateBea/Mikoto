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
#include <GUI/RenderViewport.hh>

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
        Scope_T<RenderViewport> m_Implementation{};
    };
}

#endif // MIKOTO_SCENE_PANEL_HH
