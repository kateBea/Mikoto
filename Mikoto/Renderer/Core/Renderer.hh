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
#include <Models/LightRenderData.hh>
#include <Models/LightTypeData.hh>
#include <Models/RendererDrawData.hh>
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
        static auto Init(RendererSpec&& spec) -> void;

        /**
         * @brief Shuts down the Renderer.
         * */
        static auto Shutdown() -> void;

        /**
         * @brief Begins the scene rendering with the provided preparation data.
         * @param prepareData Data required to prepare the scene for rendering.
         * */
        static auto BeginScene(const ScenePrepareData& prepareData) -> void;

        /**
         * @brief Ends the scene rendering.
         * */
        static auto EndScene() -> void;


        /**
         * @brief Submits an object for rendering with specific transformation and material.
         * @param id Identifier of the object to be submitted
         * @param objectData Information about the object to be rendered.
         * @param transform Transformation matrix for the object.
         * @param material Material properties for rendering the object.
         * */
        static auto Submit(const std::string &id, const GameObject &objectData, const glm::mat4 &transform,
                           std::shared_ptr<Material> &material) -> void;

        static auto RemoveFromDrawQueue(const std::string& id) -> bool;

        /**
         * @brief Flushes the queued draw data to be drawn.
         * */
        static auto Flush() -> void;


        /**
         * @brief Retrieves the active graphics API.
         * @returns The active graphics API.
         * */
        MKT_NODISCARD static auto GetActiveGraphicsAPI() -> GraphicsAPI { return s_ActiveAPI; }

        /**
         * @brief Retrieves a pointer to the active graphics backend.
         * @returns Pointer to the active graphics backend.
         * */
        MKT_NODISCARD static auto GetActiveGraphicsAPIPtr() -> RendererBackend* { return s_ActiveRendererAPI; }

        /**
         * @brief Retrieves the scene rendering statistics.
         * @returns Const reference to the RenderingStats.
         * */
        MKT_NODISCARD static auto GetSceneRenderStats() -> const RenderingStats& { return *s_SavedSceneStats; }

        /**
         * @brief Retrieves information about the renderer data.
         * @returns Reference to the RendererData.
         * */
        MKT_NODISCARD static auto GetRendererData() -> RendererData& { return s_RendererData; }

        /**
         * @brief Retrieves real-time statistics about renderer resources usage.
         * @returns Constant reference to the RendererStatistics.
         * */
        MKT_NODISCARD static auto GetRendererStatistics() -> const RendererStatistics&;

        static auto AddLightObject(const LightRenderInfo& info) -> void;
        static auto RemoveLightObject(const std::string& id) -> void;
        static auto GetLightObjects() -> const std::unordered_map<std::string, LightRenderInfo>& { return s_LightObjects; }
        static auto GetLightsView() -> const glm::vec4& { return  s_LightViewPos; }
        static auto SetLightsViewPos(const glm::vec4& viewPos) -> void;

        // active lights
        static auto GetActivePointLightsCount() -> Size_T { return s_ActivePointLightCount; }
        static auto GetActiveDirLightsCount() -> Size_T { return s_ActiveDirLightCount; }
        static auto GetActiveSpotLightsCount() -> Size_T { return s_ActiveSpotLightCount; }

        // Point
        static auto SetPointLightInfo( PointLight& info, Size_T index ) -> void;
        static auto SetActivePointLightsCount( Size_T count ) -> void { s_ActivePointLightCount = count; }

        static auto GetPointLights() -> std::array<PointLight, MAX_LIGHTS_PER_SCENE>& { return s_PointLights; }


        static auto SetDirLightInfo( DirectionalLight& info, Size_T index ) -> void;
        static auto SetActiveDirLightsCount( Size_T count ) -> void { s_ActiveDirLightCount = count; }

        static auto GetDirLights() -> std::array<DirectionalLight, MAX_LIGHTS_PER_SCENE>& { return s_DirectionalLights; }
        static auto SetSpotLightInfo( SpotLight& info, Size_T index ) -> void;
        static auto SetActiveSpotLightsCount( Size_T count ) -> void { s_ActiveSpotLightCount = count; }

        static auto GetSpotLights() -> std::array<SpotLight, MAX_LIGHTS_PER_SCENE>& { return s_SpotLights; }


    private:

        /**
         * @brief Updates the renderer statistics such as VRAM usage, etc.
         * */
        static auto UpdateRendererStatistics() -> void;

        /**
         * @brief Submits data for rendering.
         * @param info Shared pointer to the DrawData to be rendered.
         * */
        static auto Submit(RenderSubmitInfo &&info) -> void;

    private:
        static inline RendererSpec                                  s_Spec{};                 /**< Specifications for the renderer */
        static inline GraphicsAPI                                   s_ActiveAPI{};            /**< Graphics API currently active */
        static inline RendererBackend*                              s_ActiveRendererAPI{};    /**< Pointer to the currently active graphics backend */
        static inline std::unique_ptr<RendererDrawData>             s_DrawData{};             /**< Data necessary to prepare a scene for rendering */

        static inline std::unique_ptr<RenderingStats>               s_RenderingStats{};       /**< Rendering statistics (e.g., number of vertices, number of indices, etc.) */
        static inline std::unique_ptr<RenderingStats>               s_SavedSceneStats{};      /**< Saved scene statistics */

        static inline RendererData                                  s_RendererData{};         /**< Information about renderer resources */
        static inline RendererStatistics                            s_Statistics{};           /**< Information about renderer resources' real-time usage */

        static inline glm::vec4 s_LightViewPos{};
        static inline std::unordered_map<std::string, LightRenderInfo> s_LightObjects{};

        // point
        static inline Size_T s_ActivePointLightCount{};
        static inline std::array<PointLight, MAX_LIGHTS_PER_SCENE> s_PointLights{};

        // directional
        static inline Size_T s_ActiveDirLightCount{};
        static inline std::array<DirectionalLight, MAX_LIGHTS_PER_SCENE> s_DirectionalLights{};

        // spot
        static inline Size_T s_ActiveSpotLightCount{};
        static inline std::array<SpotLight, MAX_LIGHTS_PER_SCENE> s_SpotLights{};

    };

}

#endif // MIKOTO_RENDERER_HH