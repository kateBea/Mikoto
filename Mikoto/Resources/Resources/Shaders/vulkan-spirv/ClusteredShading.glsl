// Parameters
#define MAX_LIGHT_CLUSTERS 100

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
