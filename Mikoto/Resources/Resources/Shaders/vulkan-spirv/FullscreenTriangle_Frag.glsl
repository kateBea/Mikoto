#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout (set = 0, binding = 1) uniform sampler2D textures[];

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 outColor;

void main()
{
    // Sample texture at index 0
    vec4 texColor = texture(textures[1], v_TexCoord);
    //texColor = vec4(v_TexCoord, 0.3f, 1.0f);
    outColor = texColor;
}