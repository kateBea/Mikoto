#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderBase.glsl"

// --------------------------------------------------
// Per-frame uniform
// --------------------------------------------------
layout(set = PERPASS_SETINDEX, binding = 0) uniform FrameUBO {
    mat4 View;
    mat4 Projection;
    vec4 CameraPosition;
} frame;

layout(std430, set = PERPASS_SETINDEX, binding = 4) readonly buffer MeshInfoSSBO {
    MeshInfo Meshes[];
};

// --------------------------------------------------
// Per-vertex attributes (mesh)
// --------------------------------------------------
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TexCoord;

// --------------------------------------------------
// Outputs to fragment shader
// --------------------------------------------------
layout(location = 0) out vec3 out_FragmentWorldPos;
layout(location = 1) out vec3 out_VertexNormal;
layout(location = 2) out vec2 out_TexCoord;
layout(location = 3) out vec3 out_Color;
layout(location = 4) out vec3 out_CameraPos;

// instance output
layout(location = 5) flat out int out_AlbedoIndex;
layout(location = 6) flat out int out_NormalIndex;
layout(location = 7) flat out int out_MetallicIndex;
layout(location = 8) flat out int out_RoughnessIndex;
layout(location = 9) flat out int out_AoIndex;
layout(location = 10) flat out vec4 out_Albedo;
layout(location = 11) flat out vec4 out_Factors;

layout(location = 12) out vec3 out_FragmentViewPos;

// --------------------------------------------------
// Main
// --------------------------------------------------
void main() {
    MeshInfo meshInfo = Meshes[gl_InstanceIndex];

    mat4 model = mat4(meshInfo.Transform);

    // Per-vertex
    out_Color        = a_Color;
    out_TexCoord     = a_TexCoord;
    out_CameraPos    = frame.CameraPosition.xyz;

    // Normal transform
    out_VertexNormal  = transpose(inverse(mat3(model))) * a_Normal;

    // Fragment position
    out_FragmentWorldPos = vec3(model * vec4(a_Position, 1.0));
    out_FragmentViewPos = vec3(frame.View * vec4(a_Position, 1.0));

    // Per-instance material values
    out_AlbedoIndex    = meshInfo.AlbedoIndex;
    out_NormalIndex    = meshInfo.NormalIndex;
    out_MetallicIndex  = meshInfo.MetallicIndex;
    out_RoughnessIndex = meshInfo.RoughnessIndex;
    out_AoIndex        = meshInfo.AoIndex;
    out_Albedo         = meshInfo.Albedo;
    out_Factors        = meshInfo.Factors;

    gl_Position = frame.Projection * frame.View * model * vec4(a_Position, 1.0);
}
