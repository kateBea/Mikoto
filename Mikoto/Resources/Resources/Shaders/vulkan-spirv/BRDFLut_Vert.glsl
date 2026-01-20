// Credits: https://github.com/SaschaWillems/Vulkan/tree/master/examples

#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) out vec2 o_UV;

void main()  {
    o_UV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(o_UV * 2.0f - 1.0f, 0.0f, 1.0f);
}