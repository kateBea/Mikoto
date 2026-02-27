//    Copyright 2026 ケイト
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

#pragma once

#include "LightInfo.slang"

// ============================================================================
// CONSTANTS
// ============================================================================

static const uint MAX_LIGHT_CLUSTERS = 256;
static const uint MAX_LIGHTS = 50;

static const int LIGHT_TYPE_POINT = 1;
static const int LIGHT_TYPE_SPOT = 2;
static const int LIGHT_TYPE_DIRECTIONAL = 3;
static const int LIGHT_TYPE_INACTIVE = -1;

static const float LIGHT_MIN_INFLUENCE = 0.001f;

// ============================================================================
// STRUCTS 
// ============================================================================

struct ClusteredShadingParams {
    float4 GridSize;
    uint ShowHeatMap;
    uint ActiveLightCount;
};

struct Cluster {
    float4 Center;
    float4 ClosestPoint;
    float4 DistanceSquared;

    float4 MinPoint;
    float4 MaxPoint;

    // Active lights affecting this cluster
    uint Count;

    // Indices into the global lights list
    uint LightIndices[MAX_LIGHT_CLUSTERS];
};

// ============================================================================
// HELPERS
// ============================================================================

float GetPointLightRadius(float intensity) {
    // dist = sqrt(I / threshold)
    return sqrt(intensity / LIGHT_MIN_INFLUENCE);
}

float GetLightAttenuation(float dist) {
    return 1.0 / (dist * dist);
}

float3 GetHeatMapColor(uint count) {
    float intensity = clamp(float(count) / float(MAX_LIGHT_CLUSTERS), 0.0, 1.0);

    float3 color = float3(0.0);
    color.r = intensity;
    color.g = 1.0 - abs(intensity - 0.5) * 2.0;
    color.b = 1.0 - intensity;
    return color;
}

float3 LineIntersectionWithZPlane(float3 startPoint, float3 endPoint, float zDistance) {
    float3 direction = endPoint - startPoint;
    float3 normal = float3(0.0, 0.0, -1.0);

    float t = (zDistance - dot(normal, startPoint)) / dot(normal, direction);
    return startPoint + t * direction;
}

float3 ScreenToView(float2 screenCoord, float2 screenDimensions, float4x4 viewInverseProj) {
    float2 res = screenDimensions;

    float4 ndc = float4(screenCoord / res * 2.0 - 1.0, 0.0, 1.0);
    float4 v = mul(viewInverseProj, ndc);
    v /= v.w;

    return v.xyz;
}

bool SphereAABBIntersection(float3 center, float radius, float3 aabbMin, float3 aabbMax) {
    float3 closest = clamp(center, aabbMin, aabbMax);

    float3 diff = closest - center;
    float distSq = dot(diff, diff);

    return distSq <= (radius * radius);
}

bool TestSphereAABBPointLight(LightInfo light, Cluster cluster, float4x4 view) {
    float3 center = mul(view, float4(light.Position.xyz, 1.0)).xyz;

    float radius = GetPointLightRadius(light.Intensity);

    float3 aabbMin = cluster.MinPoint.xyz;
    float3 aabbMax = cluster.MaxPoint.xyz;

    return SphereAABBIntersection(center, radius, aabbMin, aabbMax);
}