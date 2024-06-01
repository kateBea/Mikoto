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
     * */
    struct RenderContextSpec {
        GraphicsAPI Backend{};                  /**< Graphics backend for the render context. */
        std::shared_ptr<Window> WindowHandle{}; /**< Shared pointer to the window for rendering. */
    };


    /**
     * @brief A class managing the render context lifecycle.
     * */
    class RenderContext {
    public:
        /**
         * @brief Initializes the RenderContext with provided specifications.
         * @param spec The specifications for initializing the render context.
         * */
        static auto Init(RenderContextSpec&& spec) -> void;


        /**
         * @brief Shuts down the render context.
         * */
        static auto Shutdown() -> void;


        /**
         * @brief Exposes the ready-to-present frame to the screen.
         * */
        static auto Present() -> void;


        /**
         * @brief Enables vertical synchronization.
         * */
        static auto EnableVSync() -> void;


        /**
         * @brief Disables vertical synchronization.
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
         * @returns A boolean indicating if vertical synchronization is active.
         * */
        MKT_UNUSED_FUNC static auto IsVSyncActive() -> bool;


    public:
        /**
         * @brief Sets event handles for the render context.
         * */
        static auto SetEventHandles() -> void;

    private:
        inline static UUID s_Guid{};  /**< Globally unique identifier for the render context. */
        inline static RenderContextSpec s_Spec{};   /**< Specifications for the RenderContext. */
    };

}

#endif // MIKOTO_RENDER_CONTEXT_HH
