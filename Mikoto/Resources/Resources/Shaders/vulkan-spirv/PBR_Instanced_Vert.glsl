#version 450
#extension GL_EXT_nonuniform_qualifier : require

// --------------------------------------------------
// Structures
// --------------------------------------------------
struct ShadingPassMeshBufferUBO {
    mat4 Transform;
    vec4 Albedo;
    vec4 Factors;
    int AlbedoIndex;
    int NormalIndex;
    int MetallicIndex;
    int RoughnessIndex;
    int AoIndex;
};

// --------------------------------------------------
// Per-frame uniform
// --------------------------------------------------
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 View;
    mat4 Projection;
    vec4 CameraPosition;
} frame;

// --------------------------------------------------
// Per-instance storage buffer
// --------------------------------------------------
layout(std430, set = 2, binding = 0) readonly buffer InstanceData {
    ShadingPassMeshBufferUBO instances[];
};

// --------------------------------------------------
// Vertex attributes
// --------------------------------------------------
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TexCoord;

// --------------------------------------------------
// Outputs to fragment shader (flat when no interpolation)
// --------------------------------------------------
layout(location = 0) out vec3 out_FragmentPos;
layout(location = 1) out vec3 out_VertexNormal;
layout(location = 2) out vec2 out_TexCoord;
layout(location = 3) out vec3 out_Color;
layout(location = 4) out vec3 out_CameraPos;

// ---- Instance data (flat) ----
layout(location = 5) flat out int out_AlbedoIndex;
layout(location = 6) flat out int out_NormalIndex;
layout(location = 7) flat out int out_MetallicIndex;
layout(location = 8) flat out int out_RoughnessIndex;
layout(location = 9) flat out int out_AoIndex;
layout(location = 10) flat out vec4 out_Albedo;
layout(location = 11) flat out vec4 out_Factors;

// --------------------------------------------------
// Main
// --------------------------------------------------
void main() {
    ShadingPassMeshBufferUBO data = instances[gl_InstanceIndex];

    mat4 model = data.Transform;
    mat3 normalMatrix = mat3(model);

    out_Color        = a_Color;
    out_TexCoord     = a_TexCoord;
    out_CameraPos    = frame.CameraPosition.xyz;
    out_VertexNormal = normalMatrix * a_Normal;
    out_FragmentPos  = vec3(model * vec4(a_Position, 1.0));

    // instance values passed flat
    out_AlbedoIndex    = data.AlbedoIndex;
    out_NormalIndex    = data.NormalIndex;
    out_MetallicIndex  = data.MetallicIndex;
    out_RoughnessIndex = data.RoughnessIndex;
    out_AoIndex        = data.AoIndex;
    out_Albedo         = data.Albedo;
    out_Factors        = data.Factors;

    gl_Position = frame.Projection * frame.View * model * vec4(a_Position, 1.0);
}
