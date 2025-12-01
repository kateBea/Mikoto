/**************************************************
    Shader vertex for the Text pass with MSDF

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

struct FontRenderParams {
    vec2 pos;
    vec2 size;
    vec4 color;
    uint texIndex;
    vec2 texCoords[4];
};

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 proj;
} ubo;

layout(set = 1, binding = 1) readonly buffer FontRenderParamsBuffer {
    FontRenderParams params[];
} fontParams;

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in uint a_TexCoordIndex;

layout(location = 0) out vec2 out_TexCoord;
layout(location = 1) out float out_TexIndex;
layout(location = 2) out vec4 out_Color;
layout(location = 3) flat out uint out_TexCoordIndex;

void main() {
    FontRenderParams params = fontParams.params[gl_InstanceIndex];
    out_TexCoord = params.texCoords[a_TexCoordIndex];
    out_TexCoordIndex = a_TexCoordIndex;
    out_TexIndex = float(params.texIndex);
    out_Color = params.color;

    gl_Position = ubo.proj * vec4(a_Position.x * params.size.x + params.pos.x, a_Position.y * params.size.y + params.pos.y, 0.0, 1.0);
}