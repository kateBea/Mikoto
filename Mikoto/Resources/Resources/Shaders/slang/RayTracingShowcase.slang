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

#define MKT_RAY_SCENE_SET 1
#include "base/RayTracing_Bindings.slang"

struct CameraParameters {
    float4 Origin;
    float4 Forward;
    float4 Right;
    float4 Up;
    float3 Settings;
    uint SceneIndex;
};

[[vk::binding( 1, 0 )]] RWTexture2D<float4> Output : register( u0 );
[[vk::binding( 2, 0 )]] ByteAddressBuffer Vertices : register( t0 );
[[vk::binding( 3, 0 )]] ByteAddressBuffer Indices : register( t1 );
#if defined( MKT_SUPPORT_D3D12 )
ConstantBuffer<CameraParameters> Camera : register( b0 );
#else
[[vk::push_constant]] ConstantBuffer<CameraParameters> Camera;
#endif

[raypayload] struct Payload {
    float3 Color : read( caller, closesthit ) : write( caller, closesthit, miss );
    uint Depth : read( closesthit ) : write( caller );
    uint Shadow : read( closesthit, miss ) : write( caller );
};

float3 Sky( float3 direction ) {
    return lerp( float3( 0.35, 0.42, 0.55 ), float3( 0.08, 0.19, 0.42 ), saturate( direction.y ) );
}

[shader( "raygeneration" )]
void RayGeneration() {
    uint2 pixel = DispatchRaysIndex().xy;
    float2 uv = ( float2( pixel ) + 0.5 ) / float2( DispatchRaysDimensions().xy ) * 2.0 - 1.0;
    RayDesc ray;
    ray.Origin = Camera.Origin.xyz;
    ray.Direction = normalize( Camera.Forward.xyz + Camera.Right.xyz * uv.x * Camera.Right.w * Camera.Origin.w -
                               Camera.Up.xyz * uv.y * Camera.Origin.w );
    ray.TMin = 0.001;
    ray.TMax = 1000.0;
    Payload payload = { float3( 0.0 ), 0, 0 };
    TraceRay( LoadRayScene( Camera.SceneIndex ), RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload );
    float3 color = max( payload.Color * Camera.Forward.w, 0.0 );
    color = color / ( 1.0 + color );
    Output[pixel] = float4( pow( color, 1.0 / 2.2 ), 1.0 );
}

        [shader( "miss" )] void Miss( inout Payload payload ) {
    payload.Color = payload.Shadow != 0 ? float3( 1.0 ) : Sky( WorldRayDirection() );
}

[shader( "closesthit" )] void ClosestHit( inout Payload payload, BuiltInTriangleIntersectionAttributes attributes ) {
    if ( payload.Shadow != 0 ) {
        payload.Color = float3( 0.0 );
        return;
    }
    uint3 indices = Indices.Load3( PrimitiveIndex() * 12 );
    float4 a = asfloat( Vertices.Load4( indices.x * 16 ) );
    float3 b = asfloat( Vertices.Load3( indices.y * 16 ) );
    float3 c = asfloat( Vertices.Load3( indices.z * 16 ) );
    float3 normal = normalize( mul( cross( b - a.xyz, c - a.xyz ), ( float3x3 )WorldToObject3x4() ) );
    if ( dot( normal, WorldRayDirection() ) > 0.0 ) {
        normal = -normal;
    }
    float3 point = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3 color = a.w < 0.5   ? float3( 0.48, 0.52, 0.58 )
                   : a.w < 1.5 ? float3( 0.82, 0.24, 0.10 )
                               : float3( 0.12, 0.45, 0.78 );
    if ( a.w < 0.5 ) {
        color *= ( ( int( floor( point.x ) ) + int( floor( point.z ) ) ) & 1 ) != 0 ? 0.5 : 1.0;
    }
    float3 light = normalize( float3( -0.5, 0.9, 0.3 ) );
    float visibility = 1.0;
    if ( Camera.Up.w > 0.5 && dot( normal, light ) > 0.0 ) {
        RayDesc shadowRay;
        shadowRay.Origin = point + normal * 0.003;
        shadowRay.Direction = light;
        shadowRay.TMin = 0.001;
        shadowRay.TMax = 1000.0;
        Payload shadow = { float3( 0.0 ), payload.Depth, 1 };
        // Skip closest-hit execution; an occluder leaves Color at zero, a miss sets it to one.
        TraceRay( LoadRayScene( Camera.SceneIndex ), RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 0, 1, 0,
                  shadowRay, shadow );
        visibility = shadow.Color.x;
    }
    payload.Color = color * ( 0.15 + 2.4 * saturate( dot( normal, light ) ) * visibility );
    if ( payload.Depth < uint( Camera.Settings.x ) ) {
        RayDesc reflection;
        reflection.Origin = point + normal * 0.003;
        reflection.Direction = reflect( WorldRayDirection(), normal );
        reflection.TMin = 0.001;
        reflection.TMax = 1000.0;
        Payload reflected = { float3( 0.0 ), payload.Depth + 1, 0 };
        TraceRay( LoadRayScene( Camera.SceneIndex ), RAY_FLAG_NONE, 0xFF, 0, 1, 0, reflection, reflected );
        float reflectivity = a.w < 0.5 ? 0.28 : 0.6;
        payload.Color = lerp( payload.Color, reflected.Color, reflectivity );
    }
}
