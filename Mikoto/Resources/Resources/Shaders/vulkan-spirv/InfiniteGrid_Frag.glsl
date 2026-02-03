#version 450

layout(location = 0) in vec3 in_WordlPos;
layout(location = 1) in vec3 in_CameraWorldPos;

layout(location = 0) out vec4 o_Color;

// Grid settings
const float gGridSize                   = 100.0;
const float gGridMinPixelsBetweenCells  = 2.0;
const float gGridCellSize               = 0.025;
const vec4  gGridColorThin              = vec4(0.5, 0.5, 0.5, 1.0);
const vec4  gGridColorThick             = vec4(0.3, 0.2, 0.5, 1.0);


float log10(float x)
{
    float f = log(x) / log(10.0);
    return f;
}


float satf(float x)
{
    float f = clamp(x, 0.0, 1.0);
    return f;
}


vec2 satv(vec2 x)
{
    vec2 v = clamp(x, vec2(0.0), vec2(1.0));
    return v;
}


float max2(vec2 v)
{
    float f = max(v.x, v.y);
    return f;
}


void main()
{
    vec2 dvx = vec2(dFdx(in_WordlPos.x), dFdy(in_WordlPos.x));
    vec2 dvy = vec2(dFdx(in_WordlPos.z), dFdy(in_WordlPos.z));

    float lx = length(dvx);
    float ly = length(dvy);

    vec2 dudv = vec2(lx, ly);

    float l = length(dudv);

    float LOD = max(0.0, log10(l * gGridMinPixelsBetweenCells / gGridCellSize) + 1.0);

    float GridCellSizeLod0 = gGridCellSize * pow(10.0, floor(LOD));
    float GridCellSizeLod1 = GridCellSizeLod0 * 10.0;
    float GridCellSizeLod2 = GridCellSizeLod1 * 10.0;

    dudv *= 4.0;

    vec2 mod_div_dudv = mod(in_WordlPos.xz, GridCellSizeLod0) / dudv;
    float Lod0a = max2(vec2(1.0) - abs(satv(mod_div_dudv) * 2.0 - vec2(1.0)) );

    mod_div_dudv = mod(in_WordlPos.xz, GridCellSizeLod1) / dudv;
    float Lod1a = max2(vec2(1.0) - abs(satv(mod_div_dudv) * 2.0 - vec2(1.0)) );

    mod_div_dudv = mod(in_WordlPos.xz, GridCellSizeLod2) / dudv;
    float Lod2a = max2(vec2(1.0) - abs(satv(mod_div_dudv) * 2.0 - vec2(1.0)) );

    float LOD_fade = fract(LOD);
    vec4 Color;

    if (Lod2a > 0.0) {
        Color = gGridColorThick;
        Color.a *= Lod2a;
    } else {
        if (Lod1a > 0.0) {
            Color = mix(gGridColorThick, gGridColorThin, LOD_fade);
            Color.a *= Lod1a;
        } else {
            Color = gGridColorThin;
            Color.a *= (Lod0a * (1.0 - LOD_fade));
        }
    }

    float OpacityFalloff = (1.0 - satf(length(in_WordlPos.xz - in_CameraWorldPos.xz) / gGridSize));

    Color.a *= OpacityFalloff;

    o_Color = Color;
}
