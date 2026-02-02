#version 450
#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderBase.glsl"

layout(set = PERPASS_SETINDEX, binding = 0) uniform FrameUBO {
    mat4 View;
    mat4 Projection;
    vec4 CameraPosition;
} u_Camera;

layout(std430, set = PERPASS_SETINDEX, binding = 1) readonly buffer MeshInfoSSBO {
    MeshInfo Meshes[];
};

layout(location = 0) in vec3 a_Position;

void main() {
    MeshInfo meshInfo = Meshes[gl_InstanceIndex];
    mat4 model = mat4(meshInfo.Transform);
    gl_Position = u_Camera.Projection * u_Camera.View * model * vec4(a_Position, 1.0);
}
