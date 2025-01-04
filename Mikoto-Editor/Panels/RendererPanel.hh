//
// Created by kate on 10/13/23.
//

#ifndef MIKOTO_RENDERER_PANEL_HH
#define MIKOTO_RENDERER_PANEL_HH

#include "Panel.hh"

namespace Mikoto {
    class RendererPanel final : public Panel {
    public:
        explicit RendererPanel();
        auto operator=(RendererPanel && other) -> RendererPanel & = default;

        auto OnUpdate(float timeStep) -> void override;

        ~RendererPanel() override = default;
    private:


    };
}


#endif//MIKOTO_RENDERER_PANEL_HH
