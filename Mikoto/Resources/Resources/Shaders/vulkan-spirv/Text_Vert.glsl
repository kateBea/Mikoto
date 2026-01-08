/**************************************************
    Shader vertex for the Text pass with MSDF

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

#include "ShaderBase.glsl"

struct FontRenderParams {
    mat4 Model;
    vec4 Position;
    vec4 Size;
    vec4 Color;
    vec2 TextureCoords[4];
    uint TextureIndex;
};

layout(set = PERPASS_SETINDEX, binding = 0) uniform UniformBufferObject {
    mat4 Projection;
    mat4 View;

    vec4 OutlineColor;
    float OutlineWidth;
} Ubo;

layout(set = PERPASS_SETINDEX, binding = 1) readonly buffer FontRenderParamsBuffer {
    FontRenderParams params[];
} fontParams;

layout(location = 0) in vec3 a_Position;
layout(location = 1) in uint a_TexCoordIndex;

layout(location = 0) out vec2 out_TexCoord;
layout(location = 1) out float out_TexIndex;
layout(location = 2) out vec4 out_Color;

layout(location = 3) out vec4 out_OutlineColor;
layout(location = 4) out float out_OutlineWidth;

void main() {
    FontRenderParams params = fontParams.params[gl_InstanceIndex];

    out_TexCoord = params.TextureCoords[a_TexCoordIndex];
    out_TexIndex = float(params.TextureIndex);
    out_Color = params.Color;

    out_OutlineColor = Ubo.OutlineColor;
    out_OutlineWidth = Ubo.OutlineWidth;

    vec3 pos = a_Position * params.Size.xyz + params.Position.xyz;

    //gl_Position = Ubo.Projection * Ubo.View * params.Model * vec4(pos, 1.0);
    gl_Position = Ubo.Projection * vec4(pos, 1.0);
}