//
// Created by kate on 1/19/25.
//

#ifndef RENDERSUBMITINFO_HH
#define RENDERSUBMITINFO_HH
#include <Material/Core/Material.hh>
#include <memory>
#include <string>

#include "GameObjectData.hh"
#include "LightRenderData.hh"

namespace Mikoto {
    /**
     * @brief Represents the specifications required to initialize the Renderer.
     * This struct encapsulates the necessary specifications for initializing a Renderer,
     * for now it just includes the graphics backend to be used.
     * */
    struct RendererSpec {
        GraphicsAPI Backend{}; /**< Graphics backend for the renderer. */
    };

    struct RenderSubmitInfo {
        std::string Id{};
        std::shared_ptr<GameObject> Data{};
        std::shared_ptr<Material> MatInfo{};
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

    struct LightRenderInfo {
        LightType Type{};
        LightData Data{};
        glm::vec4 Position{};
        bool IsActive{};
    };
}

#endif //RENDERSUBMITINFO_HH
