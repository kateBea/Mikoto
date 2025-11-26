#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 v_TexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 texColor = vec4(1.0f, 0.3f, v_TexCoord);
    outColor = texColor;
}