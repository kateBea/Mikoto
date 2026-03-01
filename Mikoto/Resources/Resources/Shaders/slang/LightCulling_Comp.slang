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

#include "base/Base.slang"
#include "base/LightInfo.slang"
#include "base/Camera_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"

static const uint LOCAL_SIZE = 128;

[[vk::binding(0, BUFFER_VIEWS_SET_INDEX)]]
ConstantBuffer<CameraInfo, ScalarDataLayout> u_Camera;

[[vk::binding(0, UAV_SET_INDEX)]]
RWStructuredBuffer<Cluster, ScalarDataLayout> Clusters;

[[vk::binding(1, UAV_SET_INDEX)]]
StructuredBuffer<LightInfo, ScalarDataLayout> Lights;

[[vk::push_constant]]
ClusteredShadingParams u_Params;

[shader("compute")]
[numthreads(LOCAL_SIZE, 1, 1)]
void main(uint3 groupID: SV_GroupID,
          uint3 groupThreadID: SV_GroupThreadID,
          uint3 dispatchID: SV_DispatchThreadID)
{
    uint index = groupID.x * LOCAL_SIZE + groupThreadID.x;
    if (index >= Clusters.getCount()) {
        return;
    }

    Cluster cluster = Clusters[index];

    // we need to reset count because culling runs every frame,
    // otherwise it would accumulate.
    cluster.Count = 0;

    uint lightCount = u_Params.ActiveLightCount;

    // I go up until lightCount because I want to find accross all
    // lights in the scene which ones affect this cluster
    for (uint i = 0; i < lightCount; ++i) {
        bool visible = false;

        LightInfo light = Lights[i];

        switch (Lights[i].ActiveLightType) {
        case LIGHT_TYPE_POINT:
            visible = TestSphereAABBPointLight(light, cluster, u_Camera.ViewMatrix);
            break;

        case LIGHT_TYPE_SPOT:
            // TODO: real spotlight culling
            // improve. https://simoncoenen.com/blog/programming/graphics/SpotlightCulling
            visible = TestSphereAABBPointLight(light, cluster, u_Camera.ViewMatrix);
            break;

        case LIGHT_TYPE_DIRECTIONAL:
            // All clusters are affected by directional light
            // Reconsider for clusters where there is no geometry
            visible = true;
            break;

        default:
            break;
        }

        // Write into cluster list
        if (visible && cluster.Count < MAX_LIGHT_CLUSTERS) {
            cluster.LightIndices[cluster.Count] = i;
            cluster.Count++;
        }
    }

    // Write back modified struct
    Clusters[index] = cluster;
}