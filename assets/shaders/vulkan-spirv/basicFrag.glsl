#version 450
layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inTexCoord;

// Output variables
layout (location = 0) out vec4 outColor;

// Uniform variables
layout(binding = 1) uniform sampler2D texSampler;

void main() {
    outColor = vec4(1.0f, 0.4f, 1.0f, 1.0f);
    //outColor = texture(texSampler, inTexCoord);
}