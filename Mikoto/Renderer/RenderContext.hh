/**
 * RenderContext.hh
 * Created by kate on 6/4/23.
 * */

#ifndef MIKOTO_RENDER_CONTEXT_HH
#define MIKOTO_RENDER_CONTEXT_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>
#include <Platform/Window.hh>

namespace Mikoto {

    /**
     * @brief Represents the specifications required to initialize the rendering context.
     * */
    struct RenderContextSpec {
        /** Graphics backend for the render context. */
        GraphicsAPI TargetAPI{};

        /** Shared pointer to the window for rendering. */
        std::shared_ptr<Window> Handle{};
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
        static auto Init( RenderContextSpec&& spec ) -> void;


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


    private:
        /** Globally unique identifier for the render context. */
        inline static UUID s_Guid{};

        /** Specifications for the RenderContext. */
        inline static RenderContextSpec s_Spec{};
    };

}

#endif // MIKOTO_RENDER_CONTEXT_HH
