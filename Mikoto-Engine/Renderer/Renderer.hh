/**
 * @file Renderer.hh
 * @brief Definition of the Renderer class and related structures.
 * @details Includes declarations of rendering functionality and specifications for the Renderer.
 * @date Created on 6/5/23.
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

#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/RendererBackend.hh>

#include <Scene/Camera.hh>

#define MAX_LIGHTS_PER_SCENE 5

namespace Mikoto {
    /**
     * @brief Represents the specifications required to initialize the Renderer.
     * This struct encapsulates the necessary specifications for initializing a Renderer,
     * for now it just includes the graphics backend to be used.
     * */
    struct RendererSpec {
        GraphicsAPI Backend{}; /**< Graphics backend for the renderer. */
    };

    /**
     * @brief Contains information about the renderer resources.
     * This struct holds data related to the resources available to the renderer.
     * */
    struct RendererData {
        std::string GPUName{ "Unknown" };        /**< Name of the GPU used. */
        std::string CPUName{ "Unknown" };        /**< Name of the CPU used. */
        std::string DriverVersion{ "Unknown" };  /**< Version of the driver. */
        double RAMSize{ 0.0f };                  /**< Amount of RAM available in MB. */
        double VRAMSize{ 0.0f };                 /**< Amount of VRAM available in MB. */
    };

    /**
     * @brief Contains real-time usage information about the renderer resources.
     * This struct holds statistics related to the real-time usage of renderer resources.
     * */
    struct RendererStatistics {
        UInt64_T VRAMUsage{}; /**< Amount of VRAM used in bytes. */
    };

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
         * @brief Submits data for rendering.
         * @param data Shared pointer to the DrawData to be rendered.
         * */
        static auto Submit(std::shared_ptr<DrawData> &&data) -> void;

        /**
         * @brief Submits an object for rendering with specific transformation and material.
         * @param objectData Information about the object to be rendered.
         * @param transform Transformation matrix for the object.
         * @param material Material properties for rendering the object.
         * */
        static auto Submit(const SceneObjectData& objectData, const glm::mat4& transform) -> void;

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




        // Temporary API for light handling
        static auto GetLightsView() -> const glm::vec4& { return  s_LightViewPos; }
        static auto SetLightsViewPos(const glm::vec4& viewPos) -> void;

        // Point
        static auto SetPointLightInfo( PointLight& info, Size_T index ) -> void;
        static auto SetActivePointLightsCount( Size_T count ) -> void { s_ActivePointLightCount = count; }

        static auto GetPointLights() -> std::array<PointLight, 5>& { return s_PointLights; }
        static auto GetActivePointLightsCount() -> Size_T { return s_ActivePointLightCount; }

        // Directional light
        static auto SetDirLightInfo( DirectionalLight& info, Size_T index ) -> void;
        static auto SetActiveDirLightsCount( Size_T count ) -> void { s_ActiveDirLightCount = count; }

        static auto GetDirLights() -> std::array<DirectionalLight, 5>& { return s_DirectionalLights; }
        static auto GetActiveDirLightsCount() -> Size_T { return s_ActiveDirLightCount; }

        // Spotlights
        static auto SetSpotLightInfo( SpotLight& info, Size_T index ) -> void;
        static auto SetActiveSpotLightsCount( Size_T count ) -> void { s_ActiveSpotLightCount = count; }

        static auto GetSpotLights() -> std::array<SpotLight, 5>& { return s_SpotLights; }
        static auto GetActiveSpotLightsCount() -> Size_T { return s_ActiveSpotLightCount; }


    private:

        /**
         * @brief Updates the renderer statistics such as VRAM usage, etc.
         * */
        static auto UpdateRendererStatistics() -> void;

    private:
        static inline RendererSpec                                  s_Spec{};                 /**< Specifications for the renderer */
        static inline GraphicsAPI                                   s_ActiveAPI{};            /**< Graphics API currently active */
        static inline RendererBackend*                              s_ActiveRendererAPI{};    /**< Pointer to the currently active graphics backend */
        static inline std::unique_ptr<RendererDrawData>             s_DrawData{};             /**< Data necessary to prepare a scene for rendering */

        static inline std::unique_ptr<RenderingStats>               s_RenderingStats{};       /**< Rendering statistics (e.g., number of vertices, number of indices, etc.) */
        static inline std::unique_ptr<RenderingStats>               s_SavedSceneStats{};      /**< Saved scene statistics */

        static inline RendererData                                  s_RendererData{};         /**< Information about renderer resources */
        static inline RendererStatistics                            s_Statistics{};           /**< Information about renderer resources' real-time usage */

        // supporting only five lights atm
        static inline glm::vec4 s_LightViewPos{};

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