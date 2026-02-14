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

#version 450

#extension GL_EXT_scalar_block_layout : require

#include "ShaderBase.glsl"
#include "Material_Helpers.glsl"

// GPU only
layout(std430, scalar, set = STATIC_SETINDEX, binding = 0) buffer ObjectBuffer {
    MeshParameters objects[];
};

// Written from CPU
layout(std430, scalar, set = PERPASS_SETINDEX, binding = 1) buffer StagingData {
    MeshParameters staged[];
};

// Written from CPU
layout(std430, scalar, set = PERPASS_SETINDEX, binding = 2) buffer UpdateIndices {
    uint indices[];
};

layout(scalar, push_constant) uniform ScatterPushConstant {
    uint UpdatedCount;
} u_Parameters;

layout(local_size_x = 64) in;

void main()  {
    uint threadID = gl_GlobalInvocationID.x;

    if (threadID >= u_Parameters.UpdatedCount)
        return;

    uint objectIndex = indices[threadID];
    objects[objectIndex] = staged[threadID];
}
