//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_CLUSTERED_SHADING_HH
#define MIKOTO_CLUSTERED_SHADING_HH

#include <Scene/Camera.hh>
#include <Scene/Scene.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/RenderUtility.hh>
#include <Renderer/Passes/MeshCulling.hh>
#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShaderParameteres.hh>

namespace Mikoto {

    class ClusteredShading {
    public:
        explicit ClusteredShading(RenderResolution resolution);

        auto SetScene(Scene* scene) -> void;
        auto SetCamera(const Camera *camera) -> void;
        auto RegisterPasses(FrameGraph &graph) -> void;

        auto SetMeshCulling(MeshCulling& cullingPass) -> void;

    private:
        auto BuildAABB( FrameGraph& graph ) -> void;
        auto BuildGBuffer( FrameGraph& graph ) -> void;
        auto BuildDepthPrepass( FrameGraph& graph ) -> void;
        auto BuildLightCulling( FrameGraph& graph ) -> void;
        auto BuildCameraInfo( FrameGraph& graph ) -> void;

        auto SetupLightList(CommandContext& ctx) -> void;

    private:
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

        struct Cluster  {
            glm::vec4 Center{};
            glm::vec4 ClosestPoint{};
            glm::vec4 DistanceSquared{};

            glm::vec4 MinPoint{};
            glm::vec4 MaxPoint{};
            UInt32 Count{};
            UInt32 LightIndices[MAX_LIGHT_CLUSTERS];
        };

        struct LightCullingUBO {
            UInt32 LightCount{};
        };

        struct GBufferConstants {
            float NearPlane{};
            float FarPlane{};
        };

    private:
        Scene* m_Scene{};
        const Camera* m_Camera{};

        UInt32 m_GridSizeX{ 12 };
        UInt32 m_GridSizeY{ 12 };
        UInt32 m_GridSizeZ{ 24 };
        UInt32 m_NumClusters{ m_GridSizeX * m_GridSizeY * m_GridSizeZ };
        UInt32 m_LocalSize{ 128 }; // for light culling

        CameraUBO m_CameraUBO{};
        LightCullingUBO m_LightCullingUBO{};

        // GBuffer
        GBufferConstants m_GBufferParams{};

        RenderResolution m_Resolution{ RenderResolution::FHD_1080 };

        std::vector<ShaderLightTypeParams> m_Lights{};

        MeshCulling* m_MeshCullingPass{};
    };
}


#endif//MIKOTO_CLUSTERED_SHADING_HH
