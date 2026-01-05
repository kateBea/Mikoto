// Parameters
#define MAX_LIGHT_CLUSTERS 256

#define MAX_LIGHTS 50

#define DISPLAY_NORMAL 1
#define DISPLAY_COLOR 2
#define DISPLAY_METAL 3
#define DISPLAY_AO 4
#define DISPLAY_ROUGH 5

#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2
#define LIGHT_TYPE_DIRECTIONAL 3
#define LIGHT_TYPE_INACTIVE -1

// Definitions
struct LightInfo {
    vec4 Position;
    vec4 Direction;
    vec3 Diffuse;

    float CutOff;
    float OuterCutOff;
    float Intensity;
    float Radius;

    int  ActiveLightType;
};

struct Cluster  {
    vec4 MinPoint;
    vec4 MaxPoint;
    uint Count;
    uint LightIndices[MAX_LIGHT_CLUSTERS];
};

vec3 GetHeatMapColor(uint count) {
    // Define density levels (adjust based on expected max light per cluster)
    float intensity = clamp(float(count) / MAX_LIGHT_CLUSTERS, 0.0, 1.0);

    // Simple Blue -> Green -> Red gradient
    vec3 color = vec3(0.0);
    color.r = intensity;
    color.g = 1.0 - abs(intensity - 0.5) * 2.0;
    color.b = 1.0 - intensity;
    return color;
}
