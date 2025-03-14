/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

const float PI = 3.14159265359;

#define MAX_LIGHTS 50

#define MKT_SHADER_TRUE 1
#define MKT_SHADER_FALSE 0

#define DISPLAY_NORMAL 1
#define DISPLAY_COLOR 2
#define DISPLAY_METAL 3
#define DISPLAY_AO 4
#define DISPLAY_ROUGH 5

struct PointLight {
    vec4 Position;

    vec4 ambient;
    vec4 Diffuse;
    vec4 specular;

    vec4 AttenuationParams;

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
// x=cutOff, y=outerCutOff (both angles in radians),  z = intensity, w = radius
    vec4 CutOffValues;
};


// Input variables
layout (location = 0) in vec3 inFragmentPos;
layout (location = 1) in vec3 inNormals;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec2 inVertexColor;

// Output variables
layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D albedoSampler;
layout(set = 0, binding = 2) uniform sampler2D normalSampler;
layout(set = 0, binding = 3) uniform sampler2D metallicSampler;
layout(set = 0, binding = 4) uniform sampler2D roughnessSampler;
layout(set = 0, binding = 5) uniform sampler2D ambientOcclusionSampler;

layout(set = 0, binding = 6) uniform UniformBuffer {
    SpotLight SpotLights[MAX_LIGHTS];
    PointLight PointLights[MAX_LIGHTS];
    DirectionalLight DirectionalLights[MAX_LIGHTS];

    vec4 ViewPosition;

    vec4 Albedo;
    vec4 Factors;
// metallic, rough, ao

    int HasAlbedo;
    int HasNormal;
    int HasMetallic;
    int HasAmbientOcc;
    int HasRoughness;

    int DirectionalLightCount;
    int PointLightCount;
    int SpotLightCount;

    int DisplayMode;

    int Wireframe;

} BufferData;

// ----------------------------------------------------------------------------
vec3 GetNormalFromMap()
{
    vec3 tangentNormal = texture(normalSampler, inTexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(inFragmentPos);
    vec3 Q2  = dFdy(inFragmentPos);
    vec2 st1 = dFdx(inTexCoord);
    vec2 st2 = dFdy(inTexCoord);

    vec3 N   = normalize(inNormals);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ComputeAttenuation(float distance) {
    return 1.0 / (distance * distance);  // Physically correct attenuation
}

float ComputeAttenuation(float distance, float radius) {
    //return 1.0 / (distance * distance);  // Physically correct attenuation
    return pow(max(1.0 - pow(distance / radius, 4.0), 0.0), 2.0) / (distance * distance + 1.0);
}

// Custom attenuation
float ComputeAttenuation(vec4 components, float distance) {
    // x=constant, y=linear, z=quadratic, w=unused
    float constant = components.x;
    float linear = components.y;
    float quadratic = components.z;

    return 1.0 / (constant + linear * distance +
    quadratic * (distance * distance));
}

vec3 ComputePointLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo) {
    vec3 Lo = vec3(0.0);

    for(int i = 0; i < BufferData.PointLightCount; ++i)
    {
        // calculate per-light radiance
        vec3 L = normalize(BufferData.PointLights[i].Position.xyz - inFragmentPos);
        vec3 H = normalize(V + L);
        float distance = length(BufferData.PointLights[i].Position.xyz - inFragmentPos);
        float attenuation = ComputeAttenuation(distance, BufferData.PointLights[i].AttenuationParams.y);

        vec3 radiance = BufferData.PointLights[i].Diffuse.xyz * attenuation * BufferData.PointLights[i].AttenuationParams.x;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

        // kS is equal to Fresnel
        vec3 kS = F;

        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;

        // multiply kD by the inverse metalness such that only non-metals
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);

        // add to outgoing radiance Lo
        // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    return Lo;
}

vec3 ComputeDirectionalLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo) {
    vec3 Lo = vec3(0.0);

    for(int i = 0; i < BufferData.DirectionalLightCount; ++i)
    {
        // calculate per-light radiance
        vec3 L = normalize(-vec3(BufferData.DirectionalLights[i].Position.xyz));
        vec3 H = normalize(V + L);

        // This vec 3 should be the light color, we assume it is full white for now
        vec3 radiance = BufferData.DirectionalLights[i].Diffuse.xyz;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

        // kS is equal to Fresnel
        vec3 kS = F;

        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;

        // multiply kD by the inverse metalness such that only non-metals
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);

        // add to outgoing radiance Lo
        // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    return Lo;
}

vec3 ComputeSpotLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo) {
    vec3 Lo = vec3(0.0);

    for(int i = 0; i < BufferData.SpotLightCount; ++i)
    {
        // calculate per-light radiance
        vec3 L = normalize(BufferData.SpotLights[i].Position.xyz - inFragmentPos);
        float distance = length(BufferData.SpotLights[i].Position.xyz - inFragmentPos);
        float attenuation = ComputeAttenuation(distance, BufferData.SpotLights[i].CutOffValues.w);
        // This vec 3 should be the light color, we assume it is full white for now
        vec3 radiance = BufferData.SpotLights[i].Diffuse.xyz * attenuation;

        // Spotlight intensity based on angle
        float theta = dot(L, normalize(-vec3(BufferData.SpotLights[i].Direction.xyz)));
        float epsilon = BufferData.SpotLights[i].CutOffValues.x - BufferData.SpotLights[i].CutOffValues.y;
        float intensity = clamp((theta - BufferData.SpotLights[i].CutOffValues.y) / epsilon, 0.0, 1.0) * BufferData.SpotLights[i].CutOffValues.z;
        radiance *= intensity;

        vec3 H = normalize(V + L);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

        // kS is equal to Fresnel
        vec3 kS = F;

        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;

        // multiply kD by the inverse metalness such that only non-metals
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);

        // add to outgoing radiance Lo
        // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    return Lo;
}

void main() {

    vec3 albedo     = BufferData.HasAlbedo == 1 ? pow(texture(albedoSampler, inTexCoord).rgb, vec3(2.2)) : BufferData.Albedo.xyz;
    float metallic  = BufferData.HasMetallic  == 1 ? texture(metallicSampler, inTexCoord).r : BufferData.Factors.x;
    float roughness = BufferData.HasRoughness == 1 ? texture(roughnessSampler, inTexCoord).r : BufferData.Factors.y;
    float ao        = BufferData.HasAmbientOcc == 1 ? texture(ambientOcclusionSampler, inTexCoord).r : BufferData.Factors.z;

    vec3 N = BufferData.HasNormal == 1 ? GetNormalFromMap() : normalize(inNormals);

    vec3 V = normalize(BufferData.ViewPosition.xyz - inFragmentPos);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);

    Lo += ComputeDirectionalLightContribution(N, V, F0, roughness, metallic, albedo);
    Lo += ComputePointLightContribution(N, V, F0, roughness, metallic, albedo);
    Lo += ComputeSpotLightContribution(N, V, F0, roughness, metallic, albedo);

    // ambient lighting (note that the next IBL tutorial will replace
    // this ambient lighting with environment lighting).
    vec3 ambient = vec3(0.03) * albedo * ao;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));

    // gamma correct
    color = pow(color, vec3(1.0/2.2));


    if (BufferData.Wireframe != MKT_SHADER_TRUE) {
        switch (BufferData.DisplayMode) {
            case DISPLAY_COLOR:
            outColor = vec4(color , 1.0);
            break;

            case DISPLAY_NORMAL:
            outColor = vec4(N , 1.0);
            break;

            case DISPLAY_METAL:
            outColor = vec4(metallic, metallic, metallic , 1.0);
            break;

            case DISPLAY_AO:
            outColor = vec4(ao, ao, ao , 1.0);
            break;

            case DISPLAY_ROUGH:
            outColor = vec4(roughness, roughness, roughness , 1.0);
            break;
        }
    } else {
        outColor = vec4(0.0f, 0.0f, 0.0f , 1.0);
    }
}