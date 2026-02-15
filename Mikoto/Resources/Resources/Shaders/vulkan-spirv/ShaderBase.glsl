#define TEXTURES_SETINDEX 0
#define PERFRAME_SETINDEX 1
#define PERPASS_SETINDEX 2
#define STATIC_SETINDEX 3

#define MKT_SHADER_TRUE 1
#define MKT_SHADER_FALSE 0

#define PI 3.1415926535897932384626433832795

#define INVALID_TEXTURE_INDEX -1

struct MeshInfo {
    mat4 Transform;

    vec4 Albedo;
    vec4 Factors;

    vec3 EmissiveFactors;
    float EmissiveIntensity;

    float Alpha;

    int AlbedoIndex;
    int NormalIndex;
    int MetallicIndex;
    int RoughnessIndex;
    int AoIndex;
    int EmissiveIndex;
};