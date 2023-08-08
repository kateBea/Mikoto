#version 410 core
layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec3 vertexNormals;
layout (location = 2) in vec2 vertexTexture;

out vec3 fragPosition;
out vec3 normals;
out vec2 textureCoordinates;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    fragPosition = vec3(model * vec4(vertexPosition, 1.0));
    normals = mat3(transpose(inverse(model))) * vertexNormals;
    textureCoordinates = vertexTexture;

    gl_Position = projection * view * vec4(fragPosition, 1.0);
}