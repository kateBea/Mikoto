/**
 * RenderContext.hh
 * Created by kate on 6/4/23.
 * */

#ifndef MIKOTO_RENDER_CONTEXT_HH
#define MIKOTO_RENDER_CONTEXT_HH

// C++ Standard Library
#include <any>
#include <memory>

// Project Headers
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>

#include <Platform/Window.hh>

#include <Renderer/Renderer.hh>

namespace Mikoto {
    /**
     * @brief Represents the specifications required to initialize the rendering context.
     * This struct encapsulates the necessary specifications for initializing a RenderContext,
     * including the graphics backend and a shared pointer to a Window for rendering.
     * */
    struct RenderContextSpec {
        GraphicsAPI Backend{};                  /**< Graphics backend for the render context. */
        std::shared_ptr<Window> WindowHandle{}; /**< Shared pointer to the window for rendering. */
    };

    /**
     * @brief A class managing rendering functionality and the RenderContext lifecycle.
     * */
    class RenderContext {
    public:
        /**
         * @brief Initializes the RenderContext with provided specifications.
         * The render context is also responsible of initializing the GUI.
         * @param spec The specifications for initializing the RenderContext.
         * */
        static auto Init(RenderContextSpec&& spec) -> void;

        /**
         * @brief Shuts down the RenderContext.
         * */
        static auto Shutdown() -> void;

        /**
         * @brief Presents the ready-to-present frame to the screen.
         * */
        static auto Present() -> void;

        /**
         * @brief Enables vertical synchronization for rendering.
         * */
        static auto EnableVSync() -> void;

        /**
         * @brief Disables vertical synchronization for rendering.
         * */
        static auto DisableVSync() -> void;

        /**
         * @brief Prepares the frame for rendering.
         * */
        static auto PrepareFrame() -> void;

        /**
         * @brief Submits the rendered frame for presentation.
         * */
        static auto SubmitFrame() -> void;

        /**
         * @brief Checks if vertical synchronization is active.
         * @return A boolean indicating if vertical synchronization is active.
         * */
        MKT_UNUSED_FUNC static auto IsVSyncActive() -> bool;

    public:
        DISABLE_COPY_AND_MOVE_FOR(RenderContext);

        /**
         * @brief Sets event handles for the RenderContext.
         * */
        static auto SetEventHandles() -> void;

    private:
        inline static Random::GUID::UUID s_Guid{};  /**< Globally unique identifier for the RenderContext. */
        inline static RenderContextSpec s_Spec{};   /**< Specifications for the RenderContext. */
    };

}

#endif // MIKOTO_RENDER_CONTEXT_HH
