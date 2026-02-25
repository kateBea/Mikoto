// Alpha modes
#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_MASK 1
#define ALPHA_MODE_BLEND 2

// Workflow
#define WORKFLOW_METALLIC_ROUGHNESS 0
#define WORKFLOW_SPECULAR_ROUGHNESS 1

// Animation
#define MAX_NUM_JOINTS 128

struct MaterialParameters {

    // Alpha
    float AlphaCutoff;
    int AlphaMode;

    // Workflow
    int Workflow;
};

struct MeshParameters {
    mat4 Transform;
    mat4 InverseModelView;
    uint MaterialIndex;

    int BonesID;

    vec4 Albedo;
    int AlbedoIndex;

    float AlphaCutoff;
    float MetallicFactor;
    float RoughnessFactor;
    float OcclusionStrength;

    vec3 EmissiveFactors;
    float EmissiveIntensity;

    int NormalIndex;
    int MetallicIndex;
    int RoughnessIndex;
    int AoIndex;
    int EmissiveIndex;
};