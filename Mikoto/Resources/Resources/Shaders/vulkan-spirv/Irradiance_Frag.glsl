// Credits: https://github.com/SaschaWillems/Vulkan/tree/master/examples

#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderBase.glsl"

layout(set = PERPASS_SETINDEX, binding = 1) uniform IrradianceParamsUBO {
    float DeltaPhi;
    float DeltaTheta;
} u_Constants;

layout (set = PERPASS_SETINDEX, binding = 2) uniform samplerCube u_SamplerEnv;

// In variables
layout (location = 0) in vec3 v_Pos;

// Out variables
layout (location = 0) out vec4 o_Color;

void main() {
    vec3 N = normalize(v_Pos);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = cross(N, right);

    const float TWO_PI = PI * 2.0;
    const float HALF_PI = PI * 0.5;

    vec3 color = vec3(0.0);
    uint sampleCount = 0u;
    for (float phi = 0.0; phi < TWO_PI; phi += u_Constants.DeltaPhi) {
        for (float theta = 0.0; theta < HALF_PI; theta += u_Constants.DeltaTheta) {
            vec3 tempVec = cos(phi) * right + sin(phi) * up;
            vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;
            color += texture(u_SamplerEnv, sampleVector).rgb * cos(theta) * sin(theta);
            sampleCount++;
        }
    }

    o_Color = vec4(PI * color / float(sampleCount), 1.0);
}