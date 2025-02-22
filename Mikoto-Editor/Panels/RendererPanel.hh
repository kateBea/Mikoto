//
// Created by kate on 10/13/23.
//

#ifndef MIKOTO_RENDERER_PANEL_HH
#define MIKOTO_RENDERER_PANEL_HH

#include <GUI/RenderViewport.hh>
#include <Panels/Panel.hh>
#include <Scene/Scene/Scene.hh>

namespace Mikoto {
    struct RendererPanelCreateInfo {
        UInt32_T Width{};
        UInt32_T Height{};

        Scene* TargetScene{};
        RendererBackend* Renderer{};
        SceneCamera* EditorMainCamera{};
    };

    class RendererPanel final : public Panel {
    public:
        explicit RendererPanel(const RendererPanelCreateInfo& createInfo);

        auto OnUpdate(float timeStep) -> void override;

        ~RendererPanel() override;
    private:
        Scope_T<RenderViewport> m_Implementation{};

        Scene* m_TargetScene{};

    };
}


#endif//MIKOTO_RENDERER_PANEL_HH
