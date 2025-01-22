/**
 * @file Renderer.hh
 * @date 6/5/23.
 * @author kate
 * */

#ifndef MIKOTO_RENDERER_HH
#define MIKOTO_RENDERER_HH

// C++ Standard Library
#include <memory>
#include <unordered_map>

// Third-Party Libraries
#include <glm/glm.hpp>

// Project headers
#include <Common/Common.hh>
#include <Common/RenderingUtils.hh>
#include <Material/Core/Material.hh>
#include <Models/GameObjectData.hh>
#include <Models/RenderingStats.hh>
#include <Models/ScenePrepareData.hh>
#include <Renderer/Core/RendererBackend.hh>
#include <Scene/Camera.hh>

#include "Models/RenderInfo.hh"

namespace Mikoto {
    /**
     * @brief Manages rendering functionality and the Renderer's lifecycle.
     * */
    class Renderer {
    public:
        /**
         * @brief Initializes the Renderer with provided specifications.
         * @param spec The specifications for initializing the Renderer.
         * */
        static auto Init( RendererSpec&& spec ) -> void;

        /**
         * @brief Shuts down the Renderer.
         * */
        static auto Shutdown() -> void;

        /**
         * @brief Begins the scene rendering with the provided preparation data.
         * @param prepareData Data required to prepare the scene for rendering.
         * */
        static auto BeginScene( const ScenePrepareData& prepareData ) -> void;

        /**
         * @brief Ends the scene rendering.
         * */
        static auto EndScene() -> void;


        /**
         * @brief Retrieves the active graphics API.
         * @returns The active graphics API.
         * */
        MKT_NODISCARD static auto GetActiveGraphicsAPI() -> GraphicsAPI { return s_ActiveAPI; }

        /**
         * @brief Retrieves a pointer to the active graphics backend.
         * @returns Pointer to the active graphics backend.
         * */
        MKT_NODISCARD static auto GetActiveGraphicsAPIPtr() -> IRendererBackend* { return s_ActiveRendererAPI; }

        /**
         * @brief Retrieves real-time statistics about renderer resources usage.
         * @returns Constant reference to the RendererStatistics.
         * */
        MKT_NODISCARD static auto GetRendererStatistics() -> const RendererStatistics&;


        static auto AddLightObject( const std::string& id, const LightRenderInfo& info ) -> void;
        static auto RemoveLightObject( const std::string& id ) -> void;
        static auto GetLightObjects() -> const std::unordered_map<std::string, LightRenderInfo>& { return s_LightObjects; }
        static auto GetCameraPosition() -> const glm::vec4& { return s_DrawData->CameraPosition; }

      static auto GetActiveDirLightsCount() -> Size_T { return s_ActiveDirectionalLights; }
        static auto GetActivePointLightsCount() -> Size_T { return s_ActivePointLights; }
        static auto GetActiveSpotLightsCount() -> Size_T { return s_ActiveSpotLights; }


    private:
        friend class RenderCommandPushDraw;
        friend class RenderCommandPopDraw;

    private:
        static auto Flush() -> void;
        static auto RemoveFromDrawQueue( const std::string& id ) -> bool;
        static auto UpdateRendererStatistics() -> void;

    private:
        static inline RendererSpec s_Spec{};                          /**< Specifications for the renderer */
        static inline GraphicsAPI s_ActiveAPI{};                      /**< Graphics API currently active */
        static inline IRendererBackend* s_ActiveRendererAPI{};        /**< Pointer to the currently active graphics backend */
          static inline RendererStatistics s_Statistics{};            /**< Information about renderer resources' real-time usage */
      static inline   std::unique_ptr<ScenePrepareData>             s_DrawData{};             /**< Data necessary to prepare a scene for rendering */

      static inline Size_T s_ActiveDirectionalLights{ 0 };
        static inline Size_T s_ActivePointLights{ 0 };
      static inline Size_T s_ActiveSpotLights{ 0 };
        static inline std::unordered_map<std::string, LightRenderInfo> s_LightObjects{};
    };

}

#endif// MIKOTO_RENDERER_HH