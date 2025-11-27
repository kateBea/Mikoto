#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D g_BindlessTextures[];

layout(location = 0) in vec2 v_TexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 texColor = texture(g_BindlessTextures[3], v_TexCoord);
    outColor = texColor;
}