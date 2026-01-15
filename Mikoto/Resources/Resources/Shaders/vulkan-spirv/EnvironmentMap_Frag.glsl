/**************************************************
    Shader for the SkyBox.

    Stage: Vertex
    Version: GLSL 4.5.0
**************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderBase.glsl"

layout (location = 0) in vec3 v_LocalPos;

layout (location = 0) out vec4 o_Color;

layout (set = PERPASS_SETINDEX, binding = 1) uniform sampler2D u_EquirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)  {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    // make sure to normalize localPos
    vec2 uv = SampleSphericalMap(normalize(v_LocalPos));
    vec3 color = texture(u_EquirectangularMap, uv).rgb;

    o_Color = vec4(color, 1.0f);
}
