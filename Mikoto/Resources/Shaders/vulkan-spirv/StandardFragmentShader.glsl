/**************************************************
    Shader for the standard material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

layout (location = 0) in vec3 fragmentPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec3 inNormals;
layout (location = 3) in vec2 inTexCoord;
layout (location = 4) in vec2 inVertexColor;

// Output variables
layout (location = 0) out vec4 outColor;

/**  Constants */

#define MAX_LIGHTS 50
#define LIGHT_HAS_SPECULAR_MAP      1
#define LIGHT_HAS_NO_SPECULAR_MAP   0

#define LIGHT_HAS_DIFFUSE_MAP       1
#define LIGHT_HAS_NO_DIFFUSE_MAP    0

#define C_PI                        3.14159265

/**  Structures */

struct PointLight {
    vec4 position;

    vec4 ambient;
    vec4 diffuse;
    vec4 specular;

    // x=constant, y=linear, z=quadratic, w=radius
    // Used for now for simplicity for proper aligment
    vec4 components;

    // x=ambient, rest unused for now
    vec4 intensity;
};

struct DirectionalLight {
    vec4 Direction;
    vec4 Position;

    vec4 Ambient;
    vec4 Diffuse;
    vec4 Specular;
};

struct SpotLight {
    vec4 Position;
    vec4 Direction;

    vec4 Ambient;
    vec4 Diffuse;
    vec4 Specular;

    // cutoff
    // x=cutOff, y=outerCutOff (both angles in radians)
    vec4 CutOffValues;

    // Components
    // x=constant, y=linear, z=quadratic, w=unused
    // Used for now for simplicity for proper aligment
    vec4 Components;
};

layout(set = 0, binding = 1) uniform sampler2D diffuseSampler;
layout(set = 0, binding = 2) uniform sampler2D specularSampler;
layout(set = 0, binding = 3) uniform UniformBufferObject {
    SpotLight SpotLights[MAX_LIGHTS];
    PointLight PointLights[MAX_LIGHTS];
    DirectionalLight DirectionalLights[MAX_LIGHTS];

    vec4 ViewPosition;

    // Stores x=lights count, y=has diffuse, z=has specular, w=shininess
    vec4 ObjectLightInfo;

    // holds count of each type of light
    // x=dir, y=point, z=spot
    vec4 LightTypesCount;

} UniformBufferData;


vec4 CalcDirLight(DirectionalLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    const float shininess = UniformBufferData.ObjectLightInfo.w;

    vec3 lightDir;
    // careful for floating point errors
    if (light.Direction.w == 1.0f) {
        // Directional light with taking vector as a position vector
        lightDir = normalize(light.Position.rgb - fragPos);
    }
    else {
        // Directional light with taking vector as a direction vector
        lightDir = normalize(-light.Direction.rgb);
    }

    // diffuse shading
    const float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    const vec3 reflectDir = reflect(-lightDir, normal);
    const float spec = pow(max(dot(viewDir, reflectDir), 0.0), UniformBufferData.ObjectLightInfo.w);

    // combine results
    vec4 ambient;
    vec4 diffuse;

    const int hasDiffuse = int(UniformBufferData.ObjectLightInfo.y);
    switch (hasDiffuse) {
        case LIGHT_HAS_DIFFUSE_MAP:
        ambient = light.Ambient * texture(diffuseSampler, inTexCoord);
        diffuse = light.Diffuse * diff * texture(diffuseSampler, inTexCoord);
        break;

        case LIGHT_HAS_NO_DIFFUSE_MAP:
        ambient = light.Ambient * inColor;
        diffuse = light.Diffuse * diff * inColor;
        break;
    }

    vec4 specular;
    const int hasSpecular = int( UniformBufferData.ObjectLightInfo.z );
    const float energyConservation = ( 8.0 + shininess ) / ( 8.0 * C_PI );
    switch (hasSpecular) {
        case LIGHT_HAS_SPECULAR_MAP:
        specular = light.Specular * spec * texture(specularSampler, inTexCoord);
        break;

        case LIGHT_HAS_NO_SPECULAR_MAP:
        specular = light.Specular * spec * inColor;
        break;
    }

    return (ambient + diffuse + specular);
}

// calculates the color when using a spot light.
vec4 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    const vec3 lightDir = normalize(light.Position.rgb - fragPos);

    // Diffuse shading
    const float diff = max(dot(normal, lightDir), 0.0);

    // Specular shading
    const vec3 reflectDir = reflect(-lightDir, normal);
    const float spec = pow(max(dot(viewDir, reflectDir), 0.0), UniformBufferData.ObjectLightInfo.w);

    // Attenuation
    const float distance = length(light.Position.rgb - fragPos);
    const float attenuation = 1.0 / (light.Components.x + light.Components.y * distance + light.Components.z * (distance * distance));

    // spotlight intensity
    const float theta = dot(lightDir, normalize(-light.Direction.rgb));
    const float epsilon = light.CutOffValues.x - light.CutOffValues.y;
    const float intensity = clamp((theta - light.CutOffValues.y) / epsilon, 0.0, 1.0);

    // [Combine results]
    vec4 ambient;
    vec4 diffuse;

    const int hasDiffuse = int(UniformBufferData.ObjectLightInfo.y);
    switch (hasDiffuse) {
        case LIGHT_HAS_DIFFUSE_MAP:
        ambient = light.Ambient * texture(diffuseSampler, inTexCoord);
        diffuse = light.Diffuse * diff * texture(diffuseSampler, inTexCoord);
        break;

        case LIGHT_HAS_NO_DIFFUSE_MAP:
        ambient = light.Ambient * inColor;
        diffuse = light.Diffuse * diff * inColor;
        break;
    }

    vec4 specular;
    const int hasSpecular = int( UniformBufferData.ObjectLightInfo.z );
    switch (hasSpecular) {
        case LIGHT_HAS_SPECULAR_MAP:
        specular = light.Specular * spec * texture(specularSampler, inTexCoord);
        break;

        case LIGHT_HAS_NO_SPECULAR_MAP:
        specular = light.Specular * spec * inColor;
        break;
    }

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    return (ambient + diffuse + specular);
}

/**
 * Caclculates the color for a fragment using a point light.
 *
 * @param light point light ingotmation.
 * @param normal the normal vector.
 * @param fragPos the fragment's position.
 * @vec3 viewDir the camera's view direction.
 * */
vec4 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    // the shininess is a property of the object, not from the light
    const float shininess = UniformBufferData.ObjectLightInfo.w;
    const vec3 lightDir = normalize(light.position.rgb - fragPos);

    // [Diffuse shading]
    float diff = max(dot(normal, lightDir), 0.0);

    // [Specular shading]
    vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    // [Phong]
    // float spec = pow(max(dot(viewDir, reflectDir), 0.0), UniformBufferData.ObjectLightInfo.w);

    // [Blin-Phong]
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), UniformBufferData.ObjectLightInfo.w);

    // [Attenuation]
    float distance = length(light.position.rgb - fragPos);
    float attenuation = 5.0 / (light.components.x + light.components.y * distance + light.components.z * (distance * distance));

    // [Combine results]
    vec4 ambient;
    vec4 diffuse;

    const int hasDiffuse = int(UniformBufferData.ObjectLightInfo.y);
    switch (hasDiffuse) {
        case LIGHT_HAS_DIFFUSE_MAP:
        ambient = light.ambient * texture(diffuseSampler, inTexCoord);
        diffuse = light.diffuse * diff * texture(diffuseSampler, inTexCoord);
        break;

        case LIGHT_HAS_NO_DIFFUSE_MAP:
        ambient = light.ambient * inColor;
        diffuse = light.diffuse * diff * inColor;
        break;
    }

    vec4 specular;
    const int hasSpecular = int( UniformBufferData.ObjectLightInfo.z );
    const float energyConservation = ( 8.0 + shininess ) / ( 8.0 * C_PI );
    switch (hasSpecular) {
        case LIGHT_HAS_SPECULAR_MAP:
            specular = light.specular * spec * texture(specularSampler, inTexCoord);
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
    // Avoid warning meshes generally have this attribute but not used rn
    // as we pass the color via a uniform buffer
    inVertexColor;

    // [Constant properties]
    const vec3 norm = normalize(inNormals);
    const vec3 viewDir = normalize(vec3(UniformBufferData.ViewPosition) - fragmentPos);
    const float defaulAmbientIntensity = 0.4;

    // Final output result, acumulates the
    // contributions from lights.
    vec4 result;

    // [Compute base values, when there are no lights]
    const int hasDiffuse = int(UniformBufferData.ObjectLightInfo.y);
    switch (hasDiffuse) {
    case LIGHT_HAS_DIFFUSE_MAP:
        const vec4 resultTextureSampling = texture(diffuseSampler, inTexCoord);
        result = vec4(resultTextureSampling.rgb * defaulAmbientIntensity, resultTextureSampling.a);
        break;

    case LIGHT_HAS_NO_DIFFUSE_MAP:
        result = vec4(inColor.rgb * defaulAmbientIntensity, inColor.a);
        break;
    }


    // [1. Compute directional lights contribution]
    const int limitDirectionalLights = int(UniformBufferData.LightTypesCount.x);
    for (int index = 0; index < limitDirectionalLights; ++index) {
        result += CalcDirLight(UniformBufferData.DirectionalLights[index], norm, fragmentPos, viewDir);
    }

    // [2. Compute point lights]
    const int limitPointLights = int(UniformBufferData.LightTypesCount.y);

    for (int index = 0; index < limitPointLights; ++index) {
        result += CalcPointLight(UniformBufferData.PointLights[index], norm, fragmentPos, viewDir);
    }

    // [2. Compute spot lights contribution]
    const int limitSpotLights = int(UniformBufferData.LightTypesCount.z);

    for (int index = 0; index < limitSpotLights; ++index) {
        result += CalcSpotLight(UniformBufferData.SpotLights[index], norm, fragmentPos, viewDir);
    }

    // [Final pixel output color]
    outColor = result;
}