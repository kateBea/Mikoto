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
#include <Models/RenderContextData.hh>
#include <STL/Random/Random.hh>

namespace Mikoto {

    /**
     * @brief A class managing the render context lifecycle.
     * */
    class RenderContext {
    public:
        /**
         * @brief Initializes the RenderContext with provided specifications.
         * @param spec The specifications for initializing the render context.
         * */
        static auto Init( RenderContextData&& spec ) -> void;


        /**
         * @brief Shuts down the render context.
         * */
        static auto Shutdown() -> void;


        /**
         * @brief Exposes the ready-to-present frame to the screen.
         * */
        static auto PresentFrame() -> void;


        /**
         * @brief Prepares the frame for rendering.
         * */
        static auto PrepareFrame() -> void;


        /**
         * @brief Submits the rendered frame for presentation.
         * */
        static auto SubmitFrame() -> void;


        /**
         * @brief Enables vertical synchronization.
         * */
        static auto EnableVSync() -> void;


        /**
         * @brief Disables vertical synchronization.
         * */
        static auto DisableVSync() -> void;

        static auto PushShutdownCallback(const std::function<void()>& func) -> void;


        /**
         * @brief Checks if vertical synchronization is active.
         * @returns A boolean indicating if vertical synchronization is active.
         * */
        MKT_UNUSED_FUNC static auto IsVSyncActive() -> bool;


    private:
        /** Globally unique identifier for the render context. */
        inline static UUID s_Guid{};

        /** Specifications for the RenderContext. */
        inline static RenderContextData s_Spec{};

     inline static std::vector<std::function<void()>> s_ShutdownCallbacks{};
    };

}

#endif// MIKOTO_RENDER_CONTEXT_HH
