/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : require

#define PI 3.14159265359
#define INVALID_TEXTURE_INDEX -1
#define MAX_LIGHTS 50

#define DISPLAY_NORMAL 1
#define DISPLAY_COLOR 2
#define DISPLAY_METAL 3
#define DISPLAY_AO 4
#define DISPLAY_ROUGH 5

#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2
#define LIGHT_TYPE_DIRECTIONAL 3
#define LIGHT_TYPE_INACTIVE -1

// --------------------------------------------------
// Structs
// --------------------------------------------------
struct LightInfo {
    vec4 Position;
    vec4 Direction;
    vec4 CutOffValues;

    vec4 Diffuse;

    // x=cutOff, y=outerCutOff, z=intensity, w=radius
    vec4 AttenuationParams;

    int ActiveLightType;
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
// Interpolated input (with flat for instance data)
// --------------------------------------------------
layout(location = 0) in vec3 in_FragmentPos;
layout(location = 1) in vec3 in_Normals;
layout(location = 2) in vec2 in_TexCoord;
layout(location = 3) in vec3 in_VertexColor;
layout(location = 4) in vec3 in_CameraPos;

layout(location = 5) flat in int in_AlbedoIndex;
layout(location = 6) flat in int in_NormalIndex;
layout(location = 7) flat in int in_MetallicIndex;
layout(location = 8) flat in int in_RoughnessIndex;
layout(location = 9) flat in int in_AoIndex;
layout(location = 10) flat in vec4 in_Albedo;
layout(location = 11) flat in vec4 in_Factors;

layout(location = 0) out vec4 out_Color;

layout(set = 0, binding = 1) uniform LightUniformBuffer {
    LightInfo Lights[MAX_LIGHTS];
    int ActiveLightsCount;
    int DisplayMode;

} Lighting;

layout(set = 1, binding = 0) uniform sampler2D g_BindlessTextures[];

vec3 GetNormalFromMap(sampler2D normalMap) {
    vec3 tangentNormal = texture(normalMap, in_TexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(in_FragmentPos);
    vec3 Q2  = dFdy(in_FragmentPos);
    vec2 st1 = dFdx(in_TexCoord);
    vec2 st2 = dFdy(in_TexCoord);

    vec3 N   = normalize(in_Normals);
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

vec3 ComputePointLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo, int lightIndex) {
    vec3 Lo = vec3(0.0);

    vec3 L = normalize(Lighting.Lights[lightIndex].Position.xyz - in_FragmentPos);
    vec3 H = normalize( V + L );
    float distance = length(Lighting.Lights[lightIndex].Position.xyz - in_FragmentPos);
    float attenuation = ComputeAttenuation(distance );

    vec3 radiance = Lighting.Lights[lightIndex].Diffuse.xyz * attenuation * Lighting.Lights[lightIndex].AttenuationParams.x;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX( N, H, roughness );
    float G   = GeometrySmith( N, V, L, roughness );
    vec3 F    = FresnelSchlick( max(dot(H, V), 0.0), F0 );

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

    return Lo;
}

vec3 ComputeSpotLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo, int lightIndex) {
    vec3 Lo = vec3(0.0);

    // calculate per-light radiance
    vec3 L = normalize(Lighting.Lights[lightIndex].Position.xyz - in_FragmentPos);
    float distance = length(Lighting.Lights[lightIndex].Position.xyz - in_FragmentPos);
    float attenuation = ComputeAttenuation(distance, Lighting.Lights[lightIndex].CutOffValues.w);
    // This vec 3 should be the light color, we assume it is full white for now
    vec3 radiance = Lighting.Lights[lightIndex].Diffuse.xyz * attenuation;

    // Spotlight intensity based on angle
    float theta = dot(L, normalize(-vec3(Lighting.Lights[lightIndex].Direction.xyz)));
    float epsilon = Lighting.Lights[lightIndex].CutOffValues.x - Lighting.Lights[lightIndex].CutOffValues.y;
    float intensity = clamp((theta - Lighting.Lights[lightIndex].CutOffValues.y) / epsilon, 0.0, 1.0) * Lighting.Lights[lightIndex].CutOffValues.z;
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

    return Lo;
}

vec3 ComputeDirectionalLightContribution(vec3 N, vec3 V, vec3 F0, float roughness, float metallic, vec3 albedo, int lightIndex) {
    vec3 Lo = vec3(0.0);

    vec3 L = normalize( -vec3(Lighting.Lights[lightIndex].Direction.xyz ) );
    vec3 H = normalize( V + L );

    // This vec 3 should be the light color, we assume it is full white for now
    vec3 radiance = Lighting.Lights[lightIndex].Diffuse.xyz;

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

    return Lo;
}

vec4 DetermineOutFragmentColor(vec3 N, vec3 color, float metallic, float roughness, float ao) {
    vec4 result = vec4(0.0);

    switch (Lighting.DisplayMode) {
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

    return result;
}

// --------------------------------------------------
// Main
// --------------------------------------------------
void main() {

    vec3 albedo     = in_AlbedoIndex != INVALID_TEXTURE_INDEX ?
        pow(texture(g_BindlessTextures[in_AlbedoIndex], in_TexCoord).rgb, vec3(2.2)) :
        in_Albedo.xyz;

    float metallic  = in_MetallicIndex != INVALID_TEXTURE_INDEX ?
        texture(g_BindlessTextures[in_MetallicIndex], in_TexCoord).r :
        in_Factors.x;

    float roughness = in_RoughnessIndex != INVALID_TEXTURE_INDEX ?
        texture(g_BindlessTextures[in_RoughnessIndex], in_TexCoord).r :
        in_Factors.y;

    float ao        = in_AoIndex != INVALID_TEXTURE_INDEX ?
        texture(g_BindlessTextures[in_AoIndex], in_TexCoord).r :
        in_Factors.z;

    vec3 N = in_NormalIndex != INVALID_TEXTURE_INDEX ?
        GetNormalFromMap(g_BindlessTextures[in_NormalIndex]) :
        normalize(in_Normals);

    vec3 V = normalize(in_CameraPos - in_FragmentPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < Lighting.ActiveLightsCount; ++i)  {
        switch (Lighting.Lights[i].ActiveLightType) {
            case LIGHT_TYPE_POINT:
                Lo += ComputePointLightContribution(N, V, F0, roughness, metallic, albedo, i);
                break;
            case LIGHT_TYPE_SPOT:
                Lo += ComputeSpotLightContribution(N, V, F0, roughness, metallic, albedo, i);
                break;
            case LIGHT_TYPE_DIRECTIONAL:
                Lo += ComputeDirectionalLightContribution(N, V, F0, roughness, metallic, albedo, i);
                break;
            default:
                break;
        }
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    // Tonemap + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    out_Color = DetermineOutFragmentColor(N, color, metallic, roughness, ao);
}