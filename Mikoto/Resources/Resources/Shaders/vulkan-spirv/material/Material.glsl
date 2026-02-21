struct ShaderMaterial {
    vec4 BaseColorFactor;
    vec4 EmissiveFactor;
    vec4 DiffuseFactor;
    vec4 SpecularFactor;

    float Workflow;

    float MetallicFactor;
    float RoughnessFactor;
    float AlphaMask;
    float AlphaMaskCutoff;
    float EmissiveStrength;

    int BaseColorTextureSet;
    int PhysicalDescriptorTextureSet;
    int NormalTextureSet;
    int OcclusionTextureSet;
    int EmissiveTextureSet;

    // Texture indices
    int NormalIndex;
    int MetallicIndex;
    int RoughnessIndex;
    int AoIndex;
    int EmissiveIndex;
};