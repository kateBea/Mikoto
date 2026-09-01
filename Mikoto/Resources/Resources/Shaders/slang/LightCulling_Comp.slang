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

static const uint kLocalSize = 128;

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/ClusteringLights_Helpers.slang"

MKT_DECLARE_STRUCTURED_SRV srvBuffers;
MKT_DECLARE_STRUCTURED_UAV uavBuffers;

struct ClusteredParams {
    float4 mGridSize;

    LightInfo* mLightsPtr;
    Cluster* mClustersPtr;
    CameraInfo* mCameraPtr;

    uint mClusterCount;
    uint mActiveLightCount;
};

MKT_DECLARE_PUSH_CONSTANT(ClusteredParams) parameters;

[shader("compute")]
[numthreads(kLocalSize, 1, 1)]
void main(uint3 groupID: SV_GroupID,
          uint3 groupThreadID: SV_GroupThreadID,
          uint3 dispatchID: SV_DispatchThreadID)
{
    // Get the buffers
    CameraInfo camera = *parameters.mCameraPtr;
    LightInfo* lights = parameters.mLightsPtr;
    Cluster* clusters = parameters.mClustersPtr;

    uint index = groupID.x * kLocalSize + groupThreadID.x;
    if (index >= parameters.mClusterCount) {
        return;
    }

    // we need to reset count because culling runs every frame,
    // otherwise it would accumulate.
    clusters[index].mCount = 0;

    uint lightCount = parameters.mActiveLightCount;

    // I go up until lightCount because I want to find accross all
    // lights in the scene which ones affect this cluster
    for (uint i = 0; i < lightCount; ++i) {
        bool visible = false;

        switch (lights[index].ActiveLightType) {
        case LIGHT_TYPE_POINT:
            visible = TestSphereAABBPointLight(lights, index, clusters[index], camera.mViewMatrix);
            //visible = true;
            break;

        case LIGHT_TYPE_SPOT:
            // TODO: real spotlight culling
            // improve. https://simoncoenen.com/blog/programming/graphics/SpotlightCulling
            visible = TestSphereAABBPointLight(lights, index, clusters[index], camera.mViewMatrix);
            //visible = true;
            break;

        case LIGHT_TYPE_DIRECTIONAL:
            // All clusters are affected by directional light
            // Reconsider for clusters where there is no geometry
            visible = true;
            break;

        default:
            break;
        }

        if (visible && clusters[index].mCount <= MAX_LIGHT_CLUSTERS) {
            clusters[index].mLightIndices[clusters[index].mCount] = i;
            clusters[index].mCount++;
        }
    }
}