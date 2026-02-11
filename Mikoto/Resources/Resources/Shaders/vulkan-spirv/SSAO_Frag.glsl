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

#include "ShaderBase.glsl"

#extension GL_EXT_scalar_block_layout : require

#extension GL_EXT_debug_printf : enable

const int SSAO_KERNEL_SIZE = 64;
const float SSAO_RADIUS = 0.5;

layout (scalar, set = PERPASS_SETINDEX, binding = 0) uniform UBO {
    mat4 Projection;
    vec4 Samples[SSAO_KERNEL_SIZE];
} u_Parameters;

layout (set = PERPASS_SETINDEX, binding = 1) uniform sampler2D u_samplerPosition;
layout (set = PERPASS_SETINDEX, binding = 2) uniform sampler2D u_samplerNormal;
layout (set = PERPASS_SETINDEX, binding = 3) uniform sampler2D u_SsaoNoise;

layout (location = 0) in vec2 v_UV;
layout (location = 0) out float o_Color;

void main() {
    // Get G-Buffer values
    vec3 fragPos = texture(u_samplerPosition, v_UV).rgb;
    vec3 normal = normalize(texture(u_samplerNormal, v_UV).rgb * 2.0 - 1.0);

    // Get a random vector using a noise lookup
    ivec2 texDim = textureSize(u_samplerPosition, 0);
    ivec2 noiseDim = textureSize(u_SsaoNoise, 0);
    const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * v_UV;
    vec3 randomVec = texture(u_SsaoNoise, noiseUV).xyz * 2.0 - 1.0;

    // Create TBN matrix
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(tangent, normal);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // Calculate occlusion value
    float occlusion = 0.0f;
    // remove banding
    const float bias = 0.025f;
    for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
    {
        vec3 samplePos = TBN * u_Parameters.Samples[i].xyz;
        samplePos = fragPos + samplePos * SSAO_RADIUS;

        // project
        vec4 offset = vec4(samplePos, 1.0f);
        offset = u_Parameters.Projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5f + 0.5f;

        float sampleDepth = -texture(u_samplerPosition, offset.xy).w;

        float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
    o_Color = occlusion;

    //debugPrintfEXT("Float %.2f\n", o_Color);
}

