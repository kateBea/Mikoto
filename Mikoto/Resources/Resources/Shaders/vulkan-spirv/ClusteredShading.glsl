//    Copyright 2025 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//@ref https://github.com/RavEngine/RavEngine/blob/master/shaders/cluster_shared.glsl


#define MAX_LIGHT_CLUSTERS 256

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

#define LIGHT_MIN_INFLUENCE 0.001f

// Definitions
struct LightInfo {
    vec4 Position;
    vec4 Direction;
    vec3 Diffuse;

    float CutOff;
    float OuterCutOff;
    float Intensity;
    float Radius;

    int  ActiveLightType;
};

struct Cluster  {
    // For debug
    vec4 Center;
    vec4 ClosestPoint;
    vec4 DistanceSquared;

    vec4 MinPoint;
    vec4 MaxPoint;
    uint Count; // Active lights affecting this cluster
    uint LightIndices[MAX_LIGHT_CLUSTERS];
};

float GetPointLightRadius(float intensity){
    // for quadratic falloff (influence = intensity / dist^2 ), the radius is equal to sqrt(intensity / min_influence)
    return sqrt(intensity / LIGHT_MIN_INFLUENCE);
}

float GetLightAttenuation(float dist){
    return 1 / (dist * dist);
}

vec3 GetHeatMapColor(uint count) {
    // Define density levels (adjust based on expected max light per cluster)
    float intensity = clamp(float(count) / MAX_LIGHT_CLUSTERS, 0.0, 1.0);

    // Simple Blue -> Green -> Red gradient
    vec3 color = vec3(0.0);
    color.r = intensity;
    color.g = 1.0 - abs(intensity - 0.5) * 2.0;
    color.b = 1.0 - intensity;
    return color;
}
