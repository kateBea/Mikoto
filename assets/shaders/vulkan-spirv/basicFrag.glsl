#version 450
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTexCoord;

// Output variables
layout (location = 0) out vec4 outColor;

// Uniform variables
layout(binding = 1) uniform sampler2D texSampler;

void main() {
    outColor = texture(texSampler, inTexCoord);
}