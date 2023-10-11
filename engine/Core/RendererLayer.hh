/**
 * RendererLayer.hh
 * Created by kate on 10/6/23.
 * */

#ifndef MIKOTO_RENDERER_LAYER_HH
#define MIKOTO_RENDERER_LAYER_HH


// C++ Standard Library
#include <string_view>

// Project Headers
#include <Core/Layer.hh>

namespace Mikoto {
    /**
     * Handles the rendering
     * */
    class RendererLayer : public Layer {
    public:
        explicit RendererLayer(std::string_view name = "Renderer Layer")
            :   Layer{ name } {}

        auto OnAttach() -> void override;
        auto OnDetach() -> void override;
        auto OnUpdate(double ts) -> void override;

    };
}

#endif // MIKOTO_RENDERER_LAYER_HH
