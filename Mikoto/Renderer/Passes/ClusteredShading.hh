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
#include <Renderer/Passes/CameraPass.hh>
#include <Renderer/Passes/ShaderParameteres.hh>

namespace mikoto::renderer {

    class ClusteredShading {
    public:
        explicit ClusteredShading(RenderResolution resolution);

        auto SetScene(Scene* scene) -> void;
        auto SetCameraPass(CameraPass& camPass) -> void;
        auto RegisterPasses(FrameGraph &graph) -> void;

        auto SetMeshCulling(MeshCulling& cullingPass) -> void;

    private:
        auto RegisterAABB( FrameGraph& graph ) -> void;
        auto RegisterGBuffer( FrameGraph& graph ) -> void;
        auto RegisterDepthPrepass( FrameGraph& graph ) -> void;
        auto RegisterLightCulling( FrameGraph& graph ) -> void;

        auto SetupLightList(CommandContext& ctx) -> void;

    private:
        constexpr static UInt32 MAX_LIGHT_CLUSTERS{ 256 };

        struct ClusteredShadingParams {
            Vec4F GridSize{};
            UInt32 ShowHeatMap{};
            UInt32 ActiveLightCount{};
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

    private:
        Scene* m_Scene{};
        const Camera* m_Camera{};

        UInt32 m_GridSizeX{ 12 };
        UInt32 m_GridSizeY{ 12 };
        UInt32 m_GridSizeZ{ 24 };
        UInt32 m_NumClusters{ m_GridSizeX * m_GridSizeY * m_GridSizeZ };
        UInt32 m_LocalSize{ 128 }; // for light culling

        CameraPass* m_CameraPass{ nullptr };
        ClusteredShadingParams m_ClusterShadingParams{};

        RenderResolution m_Resolution{ RenderResolution::e1080P };

        std::vector<ShaderLightTypeParams> m_Lights{};

        MeshCulling* m_MeshCullingPass{};
    };
}


#endif//MIKOTO_CLUSTERED_SHADING_HH
