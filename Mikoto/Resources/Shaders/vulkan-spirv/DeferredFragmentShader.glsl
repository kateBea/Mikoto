#version 450

// Sampler locations
layout (set = 0, binding = 1) uniform sampler2D samplerPosition;
layout (set = 0, binding = 2) uniform sampler2D samplerNormals;
layout (set = 0, binding = 3) uniform sampler2D samplerAlbedo;

// Deferred meta data uniform buffer
layout (set = 0, binding = 0) uniform MetaData {
    vec4 ViewPosition;
    int DisplayDebugTarget; // Type of view to display (albedo, normals, position)
} MetaDataUBO;

// Input data
layout (location = 0) in vec2 textureCoordinates;

// Output data
layout (location = 0) out vec4 outColor;

void main() {
    // Fetch geometry data from G-Buffer
    vec3 position   = texture(samplerPosition, textureCoordinates).rgb;
    vec3 normal     = texture(samplerNormals, textureCoordinates).rgb;
    vec4 albedo     = texture(samplerAlbedo, textureCoordinates);

    // For debug purposes. Handle debug display
    // Debug display
    if (MetaDataUBO.DisplayDebugTarget > 0) {
        switch (MetaDataUBO.DisplayDebugTarget) {
            case 1:
                outColor.rgb = position;
                break;
            case 2:
                outColor.rgb = normal;
                break;
            case 3:
                outColor.rgb = albedo.rgb;
                break;
            case 4:
                outColor.rgb = albedo.aaa;
                break;
        }
        outColor.a = 1.0;
        return;
    }

    // Process lighting

    // Ambient coefficient
    const float ambientValue = 0.1f;

    vec3 finalFragmentColor  = albedo.rgb * ambientValue;


    // Final fragment color
    outColor = vec4(finalFragmentColor, 1.0f);
}