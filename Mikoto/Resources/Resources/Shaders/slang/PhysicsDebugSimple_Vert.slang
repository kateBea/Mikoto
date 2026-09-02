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

struct DebugVertex {
    float4 mPosition;
    float4 mColor;
};

struct CameraData {
    float4x4 mViewProjection;
};

[[vk::push_constant]]
CameraData gPushConstants;

[[vk::binding(0, 0)]]
StructuredBuffer<DebugVertex> gVertices;

struct VSOutput {
    float4 mPosition : SV_Position;
    float4 mColor : TEXCOORD0;
};

VSOutput main(uint vertexID: SV_VertexID) {
    DebugVertex vertex = gVertices[vertexID];

    VSOutput output;

    output.mPosition = mul(
        gPushConstants.mViewProjection,
        float4(vertex.mPosition.xyz, 1.0)
    );

    output.mColor = vertex.mColor;

    return output;
}