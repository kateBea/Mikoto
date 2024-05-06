// Outputs albedo, normals and positions attribute values to render targets

#version 450

// Output locations. The locations should correspond to the subpass attachment indices.
layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec4 outPosition;
layout (location = 2) out vec4 outNormal;

// Input data (geometry information)
layout(location = 0) in vec4 vertexPosition;
layout(location = 1) in vec3 vertexNormals;
layout(location = 2) in vec4 vertexColor;
layout(location = 3) in vec2 vertexTextureCoord;

// Sampler locations
layout (set = 0, binding = 1) uniform sampler2D samplerColor;
layout (set = 0, binding = 2) uniform sampler2D samplerNormalMap;

void main() {
    // Output positions in world space coordinates
    outPosition = vertexPosition;

    // Output normals values
    // TODO: implement and review
    outNormal = vec4(vertexNormals, 1.0f);

    // Output albedo values
    outAlbedo = texture(samplerColor, vertexTextureCoord);
}