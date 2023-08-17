#version 450
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTexCoord;

// Output variables
layout (location = 0) out vec4 outColor;

// Uniform variables
layout(binding = 1) uniform sampler2D texSampler;

void main() {
    outColor = vec4(0.4f, 0.2f, 0.3f, 1.0f);
    //outColor = texture(texSampler, inTexCoord);
}