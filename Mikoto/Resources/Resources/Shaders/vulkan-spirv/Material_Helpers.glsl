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
    uint MeshIndex;
    uint MaterialIndex;
    //mat4 JointMatrix[MAX_NUM_JOINTS];

    //uint JointCount;
};