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


void main() {

    outColor = vec4(1.0f, 1.0f, 1.0f , 1.0);
}