#version 450

#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderBase.glsl"

layout(set = TEXTURES_SETINDEX, binding = 0) uniform sampler2D g_BindlessTextures[];
layout(set = PERPASS_SETINDEX, binding = 0) uniform HelloTextureUniformBuffer {
    int TextureIndex;
} metaData;

layout(location = 0) in vec2 v_TexCoord;
layout(location = 1) in vec3 v_Color;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(v_Color, 1.0);

    if (metaData.TextureIndex != -1) {
        outColor = texture(g_BindlessTextures[metaData.TextureIndex], v_TexCoord);
    }
}