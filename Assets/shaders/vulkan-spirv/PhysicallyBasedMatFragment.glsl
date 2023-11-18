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


void main() {

}