//
// Created by kate on 1/4/25.
//

#ifndef RENDERCONTEXTDATA_HH
#define RENDERCONTEXTDATA_HH

#include <Models/Enums.hh>
#include <Platform/Window/Window.hh>
#include <memory>

namespace Mikoto {
    /**
     * @brief Represents the specifications required to initialize the rendering context.
     * */
    struct RenderContextData {
        /** Graphics backend for the render context. */
        GraphicsAPI TargetAPI{};

        /** Shared pointer to the window for rendering. */
        std::shared_ptr<Window> Handle{};
    };
}
#endif //RENDERCONTEXTDATA_HH
