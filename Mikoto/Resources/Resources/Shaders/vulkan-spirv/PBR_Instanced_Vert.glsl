#version 450
#extension GL_EXT_nonuniform_qualifier : require

// --------------------------------------------------
// Per-frame uniform
// --------------------------------------------------
layout(set = 1, binding = 0) uniform FrameUBO {
    mat4 View;
    mat4 Projection;
    vec4 CameraPosition;
} frame;

// --------------------------------------------------
// Per-vertex attributes (mesh)
// --------------------------------------------------
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TexCoord;

// --------------------------------------------------
// Per-instance attributes
// --------------------------------------------------

// mat4 = 4 vec4 slots
layout(location = 4) in vec4 i_Model0;
layout(location = 5) in vec4 i_Model1;
layout(location = 6) in vec4 i_Model2;
layout(location = 7) in vec4 i_Model3;

// Material parameters
layout(location = 8) in vec4 i_Albedo;
layout(location = 9) in vec4 i_Factors;

// Texture indices (flat!)
layout(location = 10) in int i_AlbedoIndex;
layout(location = 11) in int i_NormalIndex;
layout(location = 12) in int i_MetallicIndex;
layout(location = 13) in int i_RoughnessIndex;
layout(location = 14) in int i_AoIndex;

// --------------------------------------------------
// Outputs to fragment shader
// --------------------------------------------------
layout(location = 0) out vec3 out_FragmentPos;
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


// --------------------------------------------------
// Main
// --------------------------------------------------
void main() {
    mat4 model = mat4(i_Model0, i_Model1, i_Model2, i_Model3);

    // Per-vertex
    out_Color        = a_Color;
    out_TexCoord     = a_TexCoord;
    out_CameraPos    = frame.CameraPosition.xyz;

    // Normal transform
    mat3 normalMatrix = mat3(model);
    out_VertexNormal  = normalMatrix * a_Normal;

    // Fragment position
    out_FragmentPos = vec3(model * vec4(a_Position, 1.0));

    // Per-instance material values
    out_AlbedoIndex    = i_AlbedoIndex;
    out_NormalIndex    = i_NormalIndex;
    out_MetallicIndex  = i_MetallicIndex;
    out_RoughnessIndex = i_RoughnessIndex;
    out_AoIndex        = i_AoIndex;
    out_Albedo         = i_Albedo;
    out_Factors        = i_Factors;

    gl_Position = frame.Projection * frame.View * model * vec4(a_Position, 1.0);
}
