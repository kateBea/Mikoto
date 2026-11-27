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

// Trace short directional-light rays through visible grass depth. Separate ground and grass
// receiver depths preserve each layer; RGBA stores opaque depth, ground visibility, grass
// visibility and grass depth.

#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct GrassContactParameters {
    uint64_t mCamera;
    uint mSceneDepth;
    uint mGrassDepth;
    uint mCoverage;
    uint mOutput;
    uint mActive;
    float3 mToLight;
};
MKT_DECLARE_PUSH_CONSTANT( GrassContactParameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) depths;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) receivers;

static const uint kGrassContactSteps = 32;
static const float kGrassContactLength = 2;
static const float kGrassContactPixelLimit = 64;
static const float kGrassContactViewStart = 35;
static const float kGrassContactViewEnd = 50;
static const float kGrassContactMinimumTravel = .02;
static const float kGrassContactDepthBias = .01;
static const float kGrassContactThickness = .04;

float3 ContactWorld( float2 uv, float depth, CameraInfo camera ) {
    float4 world = mul( camera.mInverseViewProjection,
        float4( uv * float2( 2, -2 ) + float2( -1, 1 ), depth, 1 ) );
    return world.xyz / world.w;
}

// March only a short segment toward this directional light. The coverage image
// distinguishes visible grass from copied opaque depth, avoiding false grass
// blockers on walls, the sky and hidden blades behind solid geometry.
float TraceGrassContact( int2 pixel, uint2 size, float depth, CameraInfo camera ) {
    if ( depth >= 1 ) {
        return 1;
    }
    float2 uv = ( float2( pixel ) + .5 ) / float2( size );
    float3 world = ContactWorld( uv, depth, camera );
    float viewDepth = -mul( camera.mViewMatrix, float4( world, 1 ) ).z;
    if ( viewDepth <= 0 || viewDepth >= kGrassContactViewEnd ) {
        return 1;
    }
    float3 direction = normalize( parameters.mToLight );
    float4 endClip = CameraClipPosition( float4( world + direction * kGrassContactLength, 1 ), camera );
    if ( endClip.w <= 0 ) {
        return 1;
    }
    float2 endUV = endClip.xy / endClip.w * float2( .5, -.5 ) + .5;
    float projectedLength = length( ( endUV - uv ) * float2( size ) );
    float rayLength = kGrassContactLength * min( 1, kGrassContactPixelLimit / max( projectedLength, 1 ) );
    float occlusion = 0;
    for ( uint step = 0; step < kGrassContactSteps; ++step ) {
        float travel = lerp( kGrassContactMinimumTravel, rayLength, ( float( step ) + .5 ) / kGrassContactSteps );
        float3 point = world + direction * travel;
        float4 clipPosition = CameraClipPosition( float4( point, 1 ), camera );
        if ( clipPosition.w <= 0 ) {
            break;
        }
        float2 sampleUV = clipPosition.xy / clipPosition.w * float2( .5, -.5 ) + .5;
        if ( any( sampleUV < 0 ) || any( sampleUV >= 1 ) ) {
            break;
        }
        int2 samplePixel = int2( sampleUV * float2( size ) );
        if ( all( abs( samplePixel - pixel ) <= 1 ) ) {
            continue;
        }
        float opacity = depths[parameters.mCoverage].Load( int3( samplePixel, 0 ) ).r;
        if ( opacity == 0 ) {
            continue;
        }
        float blockerDepth = depths[parameters.mGrassDepth].Load( int3( samplePixel, 0 ) ).r;
        float3 blocker = ContactWorld( ( float2( samplePixel ) + .5 ) / float2( size ), blockerDepth, camera );
        float blockerViewDepth = -mul( camera.mViewMatrix, float4( blocker, 1 ) ).z;
        float pointViewDepth = -mul( camera.mViewMatrix, float4( point, 1 ) ).z;
        float delta = pointViewDepth - blockerViewDepth;
        float thickness = max( kGrassContactThickness, blockerViewDepth * .002 );
        if ( delta > kGrassContactDepthBias && delta < thickness ) {
            occlusion = max( occlusion, opacity * saturate( 1 - delta / thickness ) );
        }
    }
    float fade = 1 - smoothstep( kGrassContactViewStart, kGrassContactViewEnd, viewDepth );
    return 1 - occlusion * fade;
}

// Opaque receivers and grass receivers occupy different depths in one pixel.
// Store both visibilities and their exact hardware depths. Shading accepts only
// a matching receiver, so transparency/MSAA layers cannot inherit another layer.
[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint width;
    uint height;
    receivers[parameters.mOutput].GetDimensions( width, height );
    if ( any( thread.xy >= uint2( width, height ) ) ) {
        return;
    }
    int2 pixel = int2( thread.xy );
    float sceneDepth = depths[parameters.mSceneDepth].Load( int3( pixel, 0 ) ).r;
    float grassDepth = depths[parameters.mGrassDepth].Load( int3( pixel, 0 ) ).r;
    float sceneVisibility = 1;
    float grassVisibility = 1;
    if ( parameters.mActive != 0 ) {
        CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.mCamera, 0 );
        sceneVisibility = TraceGrassContact( pixel, uint2( width, height ), sceneDepth, camera );
        if ( depths[parameters.mCoverage].Load( int3( pixel, 0 ) ).r > 0 ) {
            grassVisibility = TraceGrassContact( pixel, uint2( width, height ), grassDepth, camera );
        }
    }
    receivers[parameters.mOutput][pixel] = float4( sceneDepth, sceneVisibility, grassVisibility, grassDepth );
}

