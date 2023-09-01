#version 430 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Color;
layout(location = 3) in vec2 a_TextureCoordinates;

out vec3 vertexColor;
out vec2 vertexTexCoord;

uniform mat4 u_ProjectionView;
uniform mat4 u_Transform;

void main() {
    vertexColor = a_Color;
    vertexTexCoord = a_TextureCoordinates;
    gl_Position = u_ProjectionView * u_Transform * vec4(a_Position, 1.0);
}