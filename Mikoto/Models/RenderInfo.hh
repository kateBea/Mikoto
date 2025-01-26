//
// Created by kate on 1/19/25.
//

#ifndef RENDERSUBMITINFO_HH
#define RENDERSUBMITINFO_HH
#include <Material/Core/Material.hh>
#include <memory>
#include <string>

#include "GameObjectData.hh"
#include <Models/LightData.hh>

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
     * @brief Contains real-time usage information about the renderer resources.
     * This struct holds statistics related to the real-time usage of renderer resources.
     * */
    struct RendererStatistics {
        UInt64_T VRAMUsage{}; /**< Amount of VRAM used in bytes. */
    };

    struct LightRenderInfo {
        LightType Type{};
        LightData Data{};
        bool IsActive{};
    };
}

#endif //RENDERSUBMITINFO_HH
