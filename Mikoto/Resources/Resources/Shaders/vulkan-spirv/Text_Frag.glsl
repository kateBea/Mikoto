/**************************************************
    Shader fragment for the Text pass with MSDF

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "ShaderBase.glsl"

layout(location = 0) in vec2 in_TexCoord;
layout(location = 1) in float in_AtlasIndex;
layout(location = 2) in vec4 in_Color;

layout(location = 3) in vec4 in_OutlineColor;
layout(location = 4) in float in_OutlineWidth;

layout(set = TEXTURES_SETINDEX, binding = 0) uniform sampler2D g_BindlessTextures[];

layout(location = 0) out vec4 outColor;

float Median(vec3 msd) {
    return max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
}

float ScreenPxRange() {
    vec2 unitRange = vec2(2.0) / vec2(textureSize(g_BindlessTextures[int(in_AtlasIndex)], 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(in_TexCoord);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

float ScreenPxRange3D(sampler2D msdf) {
    float pxRange = 2.0; // taken from font factory

    vec2 unitRange = vec2(pxRange)/vec2(textureSize(msdf, 0));
    // If inversesqrt is not available, use vec2(1.0)/sqrt
    vec2 screenTexSize = inversesqrt(sqrt(dFdx(in_TexCoord))+sqrt(dFdy(in_TexCoord)));
    // Can also be approximated as screenTexSize = vec2(1.0)/fwidth(texCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

void main() {
    float threshold = 0.5;

    vec3 msd = texture(g_BindlessTextures[int(in_AtlasIndex)], in_TexCoord).rgb;

    float sd = Median(msd);

    float pxRange = ScreenPxRange();
    //float pxRange = ScreenPxRange3D(g_BindlessTextures[int(in_AtlasIndex)]);
    float screenPxDist = pxRange * (sd - threshold);

    // Fill alpha
    float fillAlpha = clamp(screenPxDist + threshold, 0.0, 1.0);

    // Outline alpha
    float outlineDist = screenPxDist + in_OutlineWidth;
    float outlineAlpha = clamp(outlineDist + threshold, 0.0, 1.0);

    // Outline only where fill is not
    float outlineOnlyAlpha = outlineAlpha - fillAlpha;

    vec4 fill    = in_Color * fillAlpha;
    vec4 outline = in_OutlineColor * outlineOnlyAlpha;

    outColor = fill + outline;


}