#version 450

layout (location = 0) in vec3 fragmentPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec3 inNormals;
layout (location = 3) in vec2 inTexCoord;

// Output variables
layout (location = 0) out vec4 outColor;

// Uniform variables
layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 3) uniform sampler2D specTexSampler;


/**  Constants */

#define MAX_LIGHTS 5
#define LIGHT_HAS_SPECULAR_MAP      1
#define LIGHT_HAS_NO_SPECULAR_MAP   0

#define LIGHT_HAS_DIFFUSE_MAP       1
#define LIGHT_HAS_NO_DIFFUSE_MAP    0

/**  Structures */

struct PointLight {
    vec4 position;

    vec4 ambient;
    vec4 diffuse;
    vec4 specular;

    // x=constant, y=linear, y=quadratic, z=radius
    // Used for now for simplicity for proper aligment
    vec4 components;

    // x=ambient, rest unused for now
    vec4 intensity;
};

layout(set = 0, binding = 2) uniform UniformBufferObject {
    PointLight PointLights[MAX_LIGHTS];

    vec4 ViewPosition;

    // Stores x=lights count, y=has diffuse, z=has specular, w=shininess
    vec4 LightMeta;

} UniformBufferData;

// calculates the color when using a point light.
vec4 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position.rgb - fragPos);

    // [Diffuse shading]
    float diff = max(dot(normal, lightDir), 0.0);

    // [Specular shading]
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), UniformBufferData.LightMeta.w);

    // [Attenuation]
    const float radius = 1.0f / light.components.w;
    float distance = radius * length(light.position.rgb - fragPos);
    float attenuation = 5.0 / (light.components.x + light.components.y * distance + light.components.z * (distance * distance));

    // [Combine results]
    vec4 ambient;
    vec4 diffuse;

    const int hasDiffuse = int(UniformBufferData.LightMeta.y);
    switch (hasDiffuse) {
        case LIGHT_HAS_DIFFUSE_MAP:
        ambient = light.ambient * texture(texSampler, inTexCoord);
        diffuse = light.diffuse * diff * texture(texSampler, inTexCoord);
        break;

        case LIGHT_HAS_NO_DIFFUSE_MAP:
        ambient = light.ambient * inColor;
        diffuse = light.diffuse * diff * inColor;
        break;
    }

    vec4 specular;
    const int hasSpecular = int(UniformBufferData.LightMeta.z);
    switch (hasSpecular) {
        case LIGHT_HAS_SPECULAR_MAP:
            specular = light.specular * spec * texture(specTexSampler, inTexCoord);
            break;

        case LIGHT_HAS_NO_SPECULAR_MAP:
            specular = light.specular * spec * inColor;
            break;
    }

    // [Apply attenuation]
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}

void main() {
    // properties
    const vec3 norm = normalize(inNormals);
    const vec3 viewDir = normalize(vec3(UniformBufferData.ViewPosition) - fragmentPos);
    const float defaulAmbientIntensity = 0.4;

    vec4 result;

    // [Compute base values, when there's no lights]
    const int hasDiffuse = int(UniformBufferData.LightMeta.y);
    switch (hasDiffuse) {
    case LIGHT_HAS_DIFFUSE_MAP:
        const vec4 resultTextureSampling = texture(texSampler, inTexCoord);
        result = vec4(resultTextureSampling.rgb * defaulAmbientIntensity, resultTextureSampling.a);
        break;

    case LIGHT_HAS_NO_DIFFUSE_MAP:
        result = vec4(inColor.rgb * defaulAmbientIntensity, inColor.a);
        break;
    }

    // [Compute point lights]
    const int limit = int(UniformBufferData.LightMeta.x);
    for (int index = 0; index < limit; ++index) {
        result += CalcPointLight(UniformBufferData.PointLights[index], norm, fragmentPos, viewDir);
    }

    // [Final pixel output color]
    outColor = result;
}