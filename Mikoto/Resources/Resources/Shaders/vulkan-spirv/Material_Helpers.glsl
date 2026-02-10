struct MaterialParameters {
    Mat4F Transform;

    // --- Base Color ---
    Vec4F BaseColorFactor{ 1, 1, 1, 1 };
    Int32 AlbedoIndex{ INVALID_TEXTURE_INDEX };
    Int32 BaseColorTexCoord{ 0 };

    // --- Metallic-Roughness ---
    float MetallicFactor{ 1.0f };
    float RoughnessFactor{ 1.0f };
    Int32 MetallicRoughnessIndex{ INVALID_TEXTURE_INDEX };
    Int32 MetallicRoughnessTexCoord{ 0 };

    // --- Normal Map ---
    Int32 NormalIndex{ INVALID_TEXTURE_INDEX };
    float NormalScale{ 1.0f };
    Int32 NormalTexCoord{ 0 };

    // --- Occlusion ---
    Int32 OcclusionIndex{ INVALID_TEXTURE_INDEX };
    float OcclusionStrength{ 1.0f };
    Int32 OcclusionTexCoord{ 0 };

    // --- Emissive ---
    Vec3F EmissiveFactor{ 1, 1, 1 };
    float EmissiveStrength{ 1.0f };
    Int32 EmissiveIndex{ INVALID_TEXTURE_INDEX };
    Int32 EmissiveTexCoord{ 0 };

    // --- Alpha ---
    float AlphaCutoff{ 0.5f };
    Int32 AlphaMode{ 0 };// 0=OPAQUE, 1=MASK, 2=BLEND

    // --- Workflow ---
    Int32 Workflow{ 0 };// 0 = Metallic-Roughness, 1 = Specular-Glossiness
};