//
// Created by kate on 1/11/26.
//

#ifndef MIKOTO_CLUSTERED_SHADING_HH
#define MIKOTO_CLUSTERED_SHADING_HH

#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <ankerl/unordered_dense.h>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>

namespace Mikoto {

    class AABBGenComp final : public FramePass {
    public:
        constexpr static UInt32 MAX_LIGHT_CLUSTERS{ 256 };
        struct CameraUBO {
            glm::mat4 ViewMatrix{};
            glm::mat4 InverseProjection{};

            glm::vec4 GridSize{};
            glm::vec4 ViewPosition{};

            // xy = Planes, zw = ScreenDimensions
            glm::vec4 Screen{};

            // x = show heat map
            glm::vec4 LightInfo{};
        };

        struct alignas(sizeof(glm::vec4)) Cluster  {
            glm::vec4 Center{};
            glm::vec4 ClosestPoint{};
            glm::vec4 DistanceSquared{};

            glm::vec4 MinPoint{};
            glm::vec4 MaxPoint{};
            UInt32 Count{};
            UInt32 LightIndices[MAX_LIGHT_CLUSTERS];
        };

        explicit AABBGenComp()
            : FramePass{ "AABBGenComp", PassType::COMPUTE } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& cmdList) -> void override;

        auto SetCamera(const Camera* camera) -> void;
        auto SetHeatMap(bool enable) -> void;

        MKT_NODISCARD auto GetClusterCount() const -> UInt32 { return m_NumClusters; }

    private:
        UInt32 m_GridSizeX{ 12 };
        UInt32 m_GridSizeY{ 12 };
        UInt32 m_GridSizeZ{ 24 };
        UInt32 m_NumClusters{ m_GridSizeX * m_GridSizeY * m_GridSizeZ };

        const Camera* m_Camera{};
        CameraUBO m_CameraUBO{};
    };

    class LightCullingComp final : public FramePass {
    public:
        explicit LightCullingComp()
            : FramePass{ "LightCullingComp", PassType::COMPUTE } {}

        auto Setup(FrameGraphBuilder& builder) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;

        auto SetClusterCount(UInt32 clusterCount) -> void;

        auto SetScene(Scene* scene) -> void;

    private:
        auto TraverseLights( const PassCommandList & commandList ) -> void;

        struct alignas(16) LightCullingUBO {
            UInt32 LightCount{};
        };

    private:

        UInt32 m_LocalSize{ 128 }; // from light culling comp shader
        UInt32 m_NumClusters{ 0 };

        LightCullingUBO m_LightCullingUBO{};

        Scene* m_Scene{};
        std::array<ShaderLightTypeParams, MAX_LIGHTS> m_Lights{};
    };

    class ShadowPass final : public FramePass {
    public:

        explicit ShadowPass()
            : FramePass{ "ShadowPass", PassType::RENDER } {}

        auto Setup(FrameGraphBuilder& device) -> void override;
        auto Execute(PassCommandList& commandList) -> void override;

        auto SetScene(Scene* scene) -> void;

    private:
        GpuDevice* m_Device{};

        Scene* m_Scene{};

        Vec4F m_ClearColor{ 0.1f, 0.3f, 0.4f, 1.0f };
    };

}


#endif//MIKOTO_CLUSTERED_SHADING_HH
