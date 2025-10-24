/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Vertex
    Version: GLSL 4.5.0
**************************************************/

#version 450

// [Uniform buffer containing per frame data]
layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 View;
    mat4 Projection;
    vec4 CameraPosition;
} frame;

// [Uniform buffer containing per object data]
layout(set = 1, binding = 0) uniform ObjectUBO {
    mat4 Transform;
} object;

// [Vertex attributes]
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TexCoord;

// [Output data]
layout(location = 0) out vec3 out_FragmentPos;
layout(location = 1) out vec3 out_VertexNormal;
layout(location = 2) out vec2 out_TexCoord;
layout(location = 3) out vec3 out_Color;
layout(location = 4) out vec3 out_CameraPos;

void main() {
    mat4 model = object.Transform;
    mat3 normalMatrix = mat3(model);

    out_Color = a_Color;
    out_TexCoord = a_TexCoord;
    out_CameraPos = frame.CameraPosition.xyz;
    out_VertexNormal = normalize(normalMatrix * a_Normal);
    out_FragmentPos = vec3(model * vec4(a_Position, 1.0));

    gl_Position = frame.Projection * frame.View * model * vec4(a_Position, 1.0);
}