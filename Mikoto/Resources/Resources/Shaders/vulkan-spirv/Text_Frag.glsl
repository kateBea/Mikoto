/**************************************************
    Shader fragment for the Text pass with MSDF

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 in_TexCoord;
layout(location = 1) in float in_AtlasIndex;
layout(location = 2) in vec4 in_Color;
layout(location = 3) flat in uint in_TexCoordIndex;

layout(set = 0, binding = 0) uniform sampler2D g_BindlessTextures[];

layout(location = 0) out vec4 outColor;

float median(vec3 msd) {
    return max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
}

float ScreenPxRange() {
    vec2 unitRange = vec2(2.0) / vec2(textureSize(g_BindlessTextures[int(in_AtlasIndex)], 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(in_TexCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    float threshold = 0.5;
    vec3 msd = texture(g_BindlessTextures[int(in_AtlasIndex)], in_TexCoord).rgb;
    float sd = median(msd);
    float screenPxDistance = ScreenPxRange() * (sd - threshold); // pre-computed uniform value (call screenPxRange for dynamic scale)
    float opacity = clamp(screenPxDistance + threshold, 0.0, 1.0);

    outColor = in_Color * vec4(1.0, 1.0, 1.0, opacity);
}