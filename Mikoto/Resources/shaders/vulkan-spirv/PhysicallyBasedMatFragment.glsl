/**************************************************
    Shader for the PBR material. Using vec4s
    and mat4s for now for simplicity with uniform
    buffers aligment.

    Stage: Fragment
    Version: GLSL 4.5.0
**************************************************/

#version 450

/**  Constants */

#define MAX_LIGHTS 200

/** Albedo flags */
#define LIGHT_HAS_ALBEDO_MAP      1
#define LIGHT_HAS_NO_ALBEDO_MAP   0

/** Normal flags */
#define LIGHT_HAS_NORMAL_MAP       1
#define LIGHT_HAS_NO_NORMAL_MAP    0

/** Metallic flags */
#define LIGHT_HAS_METALLIC_MAP       1
#define LIGHT_HAS_NO_METALLIC_MAP    0

/** Roughness flags */
#define LIGHT_HAS_ROUGHNESS_MAP       1
#define LIGHT_HAS_NO_ROUGHNESS_MAP    0

/** AO flags */
#define LIGHT_HAS_AO_MAP       1
#define LIGHT_HAS_NO_AO_MAP    0

#define C_PI                        3.14159265359


/**  Structures */

struct PointLight {
    vec4 Position;

    vec4 Ambient;
    vec4 Diffuse;
    vec4 Specular;

    /**
     * x = constant value
     * y = linear value
     * z = quadratic value
     * w = radius
     * */
    vec4 Components;

    /**
     * x = ambient (rest unused for now)
     * */
    vec4 Intensity;
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

layout(set = 0, binding = 1) uniform UniformBufferObject {
    /** Lights information */
    SpotLight SpotLights[MAX_LIGHTS];
    PointLight PointLights[MAX_LIGHTS];
    DirectionalLight DirectionalLights[MAX_LIGHTS];

    /** represents the camera position */
    vec4 ViewPosition;

    /**
     * x = Has/hasn't albedo map
     * y = Has/hasn't normal map
     * z = Has/hasn't metallic map
     * w = Has/hasn't roughness map
     * */
    vec4 TextureMapInfo;

    /**
     * x = Has/hasn't ao map
     * */
    vec4 TextureMapInfo2;

    /**
     * Represents the count of lighst for each type
     *
     * x = Directional lights count
     * y = point lights count
     * z = spot lights count
     * */
    vec4 LightTypesCount;


    /** Material parameters */
    vec4 Albedo;

    /**
     * x = Metallic
     * y = Roughness
     * z = ao
     * w = unused
     * */
    vec4 MaterialParams;

} UniformBufferData;


// Shader input
layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inNormals;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec3 inWorldPosition;
layout (location = 4) in vec3 inAttributeColor;

// Samplers
layout(set = 0, binding = 2) uniform sampler2D albedoMap;
layout(set = 0, binding = 3) uniform sampler2D normalMap;
layout(set = 0, binding = 4) uniform sampler2D metallicMap;
layout(set = 0, binding = 5) uniform sampler2D roughnessMap;
layout(set = 0, binding = 6) uniform sampler2D ambientOcclusionMap;

// Output variables
layout (location = 0) out vec4 outColor;

/**
 * Easy trick to get tangent-normals to world-space to keep PBR code simplified.
 * Don't worry if you don't get what's going on; you generally want to do normal
 * mapping the usual way for performance anyways; I do plan make a note of this
 * technique somewhere later in the normal mapping tutorial.
 * */
vec3 GetNormalFromMap() {
    vec3 tangentNormal = texture(normalMap, inTexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(inWorldPosition);
    vec3 Q2  = dFdy(inWorldPosition);
    vec2 st1 = dFdx(inTexCoord);
    vec2 st2 = dFdy(inTexCoord);

    vec3 N   = normalize(inNormals);
    vec3 T  = normalize((Q1 * st2.t)- (Q2 * st1.t));
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

/**
 *
 *
 * */
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = C_PI * denom * denom;

    return nom / denom;
}

/**
 *
 *
 * */
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

/**
 *
 *
 * */
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

/**
 *
 *
 * */
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    const vec3  albedo     = pow(texture(albedoMap, inTexCoord).rgb, vec3(2.2));
    const float metallic  = texture(metallicMap, inTexCoord).r;
    const float roughness = texture(roughnessMap, inTexCoord).r;
    const float ao        = texture(ambientOcclusionMap, inTexCoord).r;

    vec3 N = GetNormalFromMap();
    vec3 V = normalize(UniformBufferData.ViewPosition.xyz - inWorldPosition);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);

    for(int index = 0; index < MAX_LIGHTS; ++index) {
        // calculate per-light radiance
        vec3 L = normalize(UniformBufferData.PointLights[index].Position.xyz - inWorldPosition);
        vec3 H = normalize(V + L);
        float distance = length(UniformBufferData.PointLights[index].Position.xyz - inWorldPosition);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = UniformBufferData.PointLights[index].Diffuse.xyz * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);

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
        Lo += (kD * albedo / C_PI + specular) * radiance * NdotL;
    }

    // ambient lighting (note that the next IBL tutorial will replace
    // this ambient lighting with environment lighting).
    vec3 ambient = vec3(0.03) * albedo * ao;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(1.0f, 1.0f, 1.0f, 1.0);
}