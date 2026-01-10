#define TEXTURES_SETINDEX 0
#define PERFRAME_SETINDEX 1
#define PERPASS_SETINDEX 2

#define MKT_SHADER_TRUE 1
#define MKT_SHADER_FALSE 0

struct MeshInfo {
    mat4 Transform;

    vec4 Albedo;
    vec4 Factors;

    int AlbedoIndex;
    int NormalIndex;
    int MetallicIndex;
    int RoughnessIndex;
    int AoIndex;
};