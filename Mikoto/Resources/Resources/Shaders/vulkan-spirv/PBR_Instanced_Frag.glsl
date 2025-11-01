/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : require

const float PI = 3.14159265359;
#define INVALID_TEXTURE_INDEX -1
#define MAX_LIGHTS 50

#define DISPLAY_NORMAL 1
#define DISPLAY_COLOR 2
#define DISPLAY_METAL 3
#define DISPLAY_AO 4
#define DISPLAY_ROUGH 5

// --------------------------------------------------
// Structs
// --------------------------------------------------
struct PointLight {
    vec4 Position;
    vec4 ambient;
    vec4 Diffuse;
    vec4 AttenuationParams;
};

struct DirectionalLight {
    vec4 Direction;
    vec4 Ambient;
    vec4 Diffuse;
};

struct SpotLight {
    vec4 Position;
    vec4 Direction;
    vec4 Ambient;
    vec4 Diffuse;
// x=cutOff, y=outerCutOff, z=intensity, w=radius
    vec4 CutOffValues;
};

struct ShadingPassMeshBufferUBO {
    mat4 Transform;
    vec4 Albedo;
    vec4 Factors;
    int AlbedoIndex;
    int NormalIndex;
    int MetallicIndex;
    int RoughnessIndex;
    int AoIndex;
};

// --------------------------------------------------
// Inputs from vertex shader
// --------------------------------------------------
layout(location = 0) in vec3 inFragmentPos;
layout(location = 1) in vec3 inNormals;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inVertexColor;
layout(location = 4) in vec3 inCameraPos;
layout(location = 5) flat in uint inInstanceIndex;

// --------------------------------------------------
// Output
// --------------------------------------------------
layout(location = 0) out vec4 out_Color;

// --------------------------------------------------
// Lighting UBO
// --------------------------------------------------
layout(set = 0, binding = 1) uniform LightUniformBuffer {
    SpotLight SpotLights[MAX_LIGHTS];
    PointLight PointLights[MAX_LIGHTS];
    DirectionalLight DirectionalLights[MAX_LIGHTS];
    int DirectionalLightCount;
    int PointLightCount;
    int SpotLightCount;
    int DisplayMode;
    int Wireframe;
} LightsParams;

// --------------------------------------------------
// Global textures
// --------------------------------------------------
// If a binding uses VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, it must have the
// highest binding number in that descriptor set. All other bindings must have smaller binding numbers.
/**
Since g_Textures has variable size
layout(set = 0, binding = 0) uniform FrameUBO { ... };
layout(set = 0, binding = 1) uniform LightUniformBuffer { ... };
layout(set = 1, binding = 0) uniform sampler2D g_Textures[];

layout(set = 2, binding = 0) readonly buffer InstanceData { ... };
*/
layout(set = 1, binding = 0) uniform sampler2D g_Textures[];

// --------------------------------------------------
// Per-instance storage buffer (same as vertex)
// --------------------------------------------------
layout(std430, set = 2, binding = 0) readonly buffer InstanceData {
    ShadingPassMeshBufferUBO instances[];
};



vec3 GetNormalFromMap(sampler2D normalMap) {
    vec3 tangentNormal = texture(normalMap, inTexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx( inFragmentPos );
    vec3 Q2  = dFdy( inFragmentPos );
    vec2 st1 = dFdx( inTexCoord );
    vec2 st2 = dFdy( inTexCoord );

    vec3 N   = normalize(inNormals);
    vec3 T  = normalize( Q1 * st2.t - Q2 * st1.t);
    vec3 B  = -normalize(cross( N, T ));
    mat3 TBN = mat3( T, B, N );

    return normalize(TBN * tangentNormal);
}


float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot( N, H ), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = ( NdotH2 * ( a2 - 1.0 ) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}


float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = ( roughness + 1.0 );
    float k = ( r * r ) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * ( 1.0 - k ) + k;

    return nom / denom;
}


float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot( N, V ), 0.0);
    float NdotL = max(dot( N, L ), 0.0);
    float ggx2 = GeometrySchlickGGX( NdotV, roughness );
    float ggx1 = GeometrySchlickGGX( NdotL, roughness );

    return ggx1 * ggx2;
}


vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + ( 1.0 - F0 ) * pow( clamp( 1.0 - cosTheta, 0.0, 1.0 ), 5.0 );
}

float ComputeAttenuation(float distance) {
    // Physically correct attenuation
    return 1.0 / ( distance * distance );
}

float ComputeAttenuation(float distance, float radius) {
    return pow( max( 1.0 - pow( distance / radius, 4.0 ), 0.0 ), 2.0 ) / ( distance * distance + 1.0 );
}

// Custom attenuation
float ComputeAttenuation(vec4 components, float distance) {
    // In components x=constant, y=linear, z=quadratic, w=unused
    float constant = components.x;
    float linear = components.y;
    float quadratic = components.z;

    return 1.0 / (constant + linear * distance + quadratic * (distance * distance));
}

vec3 ComputePointLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo) {
    vec3 Lo = vec3(0.0);

    for(int i = 0; i < LightsParams.PointLightCount; ++i) {
        // calculate per-light radiance
        vec3 L = normalize( LightsParams.PointLights[i].Position.xyz - inFragmentPos);
        vec3 H = normalize( V + L );
        float distance = length( LightsParams.PointLights[i].Position.xyz - inFragmentPos );
        float attenuation = ComputeAttenuation( distance, LightsParams.PointLights[i].AttenuationParams.y );

        vec3 radiance = LightsParams.PointLights[i].Diffuse.xyz * attenuation * LightsParams.PointLights[i].AttenuationParams.x;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX( N, H, roughness );
        float G   = GeometrySmith( N, V, L, roughness );
        vec3 F    = FresnelSchlick( clamp( dot( H, V ), 0.0, 1.0), F0 );

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max( dot( N, V ), 0.0) * max( dot( N, L ), 0.0 ) + 0.0001; // + 0.0001 to prevent divide by zero
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

    for(int i = 0; i < LightsParams.DirectionalLightCount; ++i) {
        // calculate per-light radiance
        vec3 L = normalize( -vec3(LightsParams.DirectionalLights[i].Direction.xyz ) );
        vec3 H = normalize( V + L );

        // This vec 3 should be the light color, we assume it is full white for now
        vec3 radiance = LightsParams.DirectionalLights[i].Diffuse.xyz;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX( N, H, roughness );
        float G   = GeometrySmith( N, V, L, roughness );
        vec3 F    = FresnelSchlick( clamp( dot( H, V ), 0.0, 1.0 ), F0 );

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max( dot( N, V ), 0.0 ) * max( dot( N, L ), 0.0 ) + 0.0001; // + 0.0001 to prevent divide by zero
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

    for(int i = 0; i < LightsParams.SpotLightCount; ++i) {
        // calculate per-light radiance
        vec3 L = normalize(LightsParams.SpotLights[i].Position.xyz - inFragmentPos);
        float distance = length(LightsParams.SpotLights[i].Position.xyz - inFragmentPos);
        float attenuation = ComputeAttenuation(distance, LightsParams.SpotLights[i].CutOffValues.w);
        // This vec 3 should be the light color, we assume it is full white for now
        vec3 radiance = LightsParams.SpotLights[i].Diffuse.xyz * attenuation;

        // Spotlight intensity based on angle
        float theta = dot(L, normalize(-vec3(LightsParams.SpotLights[i].Direction.xyz)));
        float epsilon = LightsParams.SpotLights[i].CutOffValues.x - LightsParams.SpotLights[i].CutOffValues.y;
        float intensity = clamp((theta - LightsParams.SpotLights[i].CutOffValues.y) / epsilon, 0.0, 1.0) * LightsParams.SpotLights[i].CutOffValues.z;
        radiance *= intensity;

        vec3 H = normalize(V + L);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = FresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

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

vec4 DetermineOutFragmentColor(vec3 N, vec3 color, float metallic, float roughness, float ao) {
    vec4 result = vec4(0.0);

    if (LightsParams.Wireframe != 1) {
        switch (LightsParams.DisplayMode) {
            case DISPLAY_COLOR:
            result = vec4(color , 1.0);
            break;

            case DISPLAY_NORMAL:
            result = vec4(N , 1.0);
            break;

            case DISPLAY_METAL:
            result = vec4(metallic, metallic, metallic , 1.0);
            break;

            case DISPLAY_AO:
            result = vec4(ao, ao, ao , 1.0);
            break;

            case DISPLAY_ROUGH:
            result = vec4(roughness, roughness, roughness , 1.0);
            break;
        }
    } else {
        result = vec4(0.0f, 0.0f, 0.0f , 1.0);
    }

    return result;
}

// --------------------------------------------------
// Main
// --------------------------------------------------
void main() {

    ShadingPassMeshBufferUBO materialParams = instances[inInstanceIndex];

    vec3 albedo     = materialParams.AlbedoIndex != INVALID_TEXTURE_INDEX ?
    pow(texture(g_Textures[materialParams.AlbedoIndex], inTexCoord).rgb, vec3(2.2))
    : materialParams.Albedo.xyz;

    float metallic  = materialParams.MetallicIndex  != INVALID_TEXTURE_INDEX ?
    texture(g_Textures[materialParams.MetallicIndex], inTexCoord).r
    : materialParams.Factors.x;

    float roughness = materialParams.RoughnessIndex != INVALID_TEXTURE_INDEX ?
    texture(g_Textures[materialParams.RoughnessIndex], inTexCoord).r
    : materialParams.Factors.y;

    float ao        = materialParams.AoIndex != INVALID_TEXTURE_INDEX ?
    texture(g_Textures[materialParams.AoIndex], inTexCoord).r
    : materialParams.Factors.z;

    vec3 N = materialParams.NormalIndex != INVALID_TEXTURE_INDEX
    ? GetNormalFromMap(g_Textures[materialParams.NormalIndex])
    : normalize(inNormals);

    vec3 V = normalize(inCameraPos - inFragmentPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    Lo += ComputeDirectionalLightContribution(N, V, F0, roughness, metallic, albedo);
    Lo += ComputePointLightContribution(N, V, F0, roughness, metallic, albedo);
    Lo += ComputeSpotLightContribution(N, V, F0, roughness, metallic, albedo);

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    out_Color = DetermineOutFragmentColor(N, color, metallic, roughness, ao);
    out_Color = vec4(color , 1.0);
}