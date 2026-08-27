//    Copyright 2025 ケイト
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
#include "base/Math_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Material_Helpers.slang"
#include "base/Lighting_Helpers.slang"

struct VSOutput {
    float3 Direction : TEXCOORD0;
    float4 PositionH : SV_Position;
};

struct DrawParams {
    uint CameraBufferID;
    uint BasicSamplerID;

    uint mVertexBufferID;
    uint mIndexBufferID;

    uint SkyboxCubeRtID;
    uint PrefilterCubeRtID;

    uint BackGroundType; // Prefilter map, Original skybox, clear color

    uint MaxMipLevel;
    float Exposure;
    float Gamma;
};

MKT_DECLARE_PUSH_CONSTANT(DrawParams) Parameters;
MKT_DECLARE_STRUCTURED_SRV SRV_Buffers;

[shader("vertex")]
VSOutput main(uint vID: SV_VertexID) {
    VSOutput o;

    ByteAddressBuffer vertexBuffer = SRV_Buffers[Parameters.mVertexBufferID];
    ByteAddressBuffer indexBuffer = SRV_Buffers[Parameters.mIndexBufferID];

    uint index = indexBuffer.Read<uint>(0, vID);
    VertexDescription vertex = vertexBuffer.Read<VertexDescription>(0, index);

    CameraInfo camera = SRV_Buffers[Parameters.CameraBufferID].Read<CameraInfo>(0, 0);

    // Remove translation from view matrix
    float4x4 viewNoTrans = RemoveTranslation(camera.mViewMatrix);

    // Skybox direction
    o.Direction = vertex.position;

    // Rotate by the Y axis for Vulkan otherwise cube appears upside down
#if defined(MKT_SUPPORT_VULKAN)
    o.Direction.y *= -1.0;
#endif

    // Project to clip space
    float4 clip = mul(camera.mProjection, mul(viewNoTrans, float4(vertex.position, 1.0)));
    o.PositionH = clip;

    return o;
}
