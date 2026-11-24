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

#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/ShadowMap_Helpers.slang"

// Tardif: camera-space visibility -> downsample -> horizontal/vertical Gaussian -> upsample.
// https://alextardif.com/shadowmapping.html
// Stable cascades and receiver-plane bias background, also linked from the article:
// https://therealmjp.github.io/posts/shadow-maps/
// https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
// Depth guidance is added to the filters to avoid mixing unrelated receivers at silhouettes.
enum class VisibilityStage : uint {
    eEvaluate,
    eDownsample,
    eBlurHorizontal,
    eBlurVertical,
    eUpsample,
    eCopyDepth
};

struct VisibilityParameters {
    uint64_t Camera;
    uint Records;
    uint Record;
    uint Depth;
    uint Input;
    uint Output;
    VisibilityStage Stage;
    float3 ToLight;
    uint RawVisibility;
};

MKT_DECLARE_PUSH_CONSTANT( VisibilityParameters, parameters );
MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float> ) images;

float3 VisibilityWorld( int2 pixel, uint2 size, CameraInfo camera ) {
    pixel = clamp( pixel, int2( 0 ), int2( size ) - 1 );
    float depth = textures[parameters.Depth].Load( int3( pixel, 0 ) ).r;
    float2 uv = ( float2( pixel ) + 0.5 ) / float2( size );
    float4 world = mul( camera.mInverseViewProjection, float4( uv * float2( 2, -2 ) + float2( -1, 1 ), depth, 1 ) );
    return world.xyz / world.w;
}

float VisibilityViewDepth( int2 pixel, uint2 size, CameraInfo camera ) {
    return -mul( camera.mViewMatrix, float4( VisibilityWorld( pixel, size, camera ), 1 ) ).z;
}

float VisibilityDepthWeight( float center, float neighbor ) {
    return exp( -abs( center - neighbor ) / max( 0.01, center * 0.005 ) );
}

float VisibilityCascade( ShadowMetadata record, uint face, float3 world, float3 normal ) {
    float4x4 matrix = buffers[parameters.Records].LoadByteOffset<float4x4>(
        parameters.Record * sizeof( ShadowRecord ) + face * sizeof( float4x4 ) );
    float4 projected = mul( matrix, float4( world, 1 ) );
    float3 ndc = projected.xyz / projected.w;
    float2 uv = ndc.xy * float2( 0.5, -0.5 ) + 0.5;
    if ( any( uv < 0 ) || any( uv >= 1 ) || ndc.z < 0 || ndc.z > 1 ) {
        return 1.0;
    }

    Texture2D<float4> shadow = textures[NonUniformResourceIndex( record.TextureID( face ) )];
    uint width, height;
    shadow.GetDimensions( width, height );
    int2 texel = clamp( int2( uv * float2( width, height ) ), int2( 0 ), int2( width, height ) - 1 );
    float2 texelUV = ( float2( texel ) + 0.5 ) / float2( width, height );
    float receiver = DirectionalShadowReceiverDepth( matrix, texelUV, ndc, normal );
    if ( receiver < 0 ) {
        return 1.0;
    }

    float bias = record.Bias.x + record.Bias.y * ( 1.0 - saturate( abs( dot( normal, parameters.ToLight ) ) ) );
    bias *= length( matrix[2].xyz );
    return receiver - bias <= shadow.Load( int3( texel, 0 ) ).r ? 1.0 : 0.0;
}

float EvaluateVisibility( int2 pixel, uint2 size, CameraInfo camera ) {
    float3 world = VisibilityWorld( pixel, size, camera );
    float depth = -mul( camera.mViewMatrix, float4( world, 1 ) ).z;
    ShadowMetadata record = buffers[parameters.Records].LoadByteOffset<ShadowMetadata>(
        parameters.Record * sizeof( ShadowRecord ) + ShadowMetadataOffset );
    if ( record.FaceCount == 0 || depth <= 0 || depth > record.CascadeSplits.w ) {
        return 1.0;
    }

    // Choose the nearer finite difference at discontinuities, rather than using a background edge as the receiver plane.
    float3 left = world - VisibilityWorld( pixel - int2( 1, 0 ), size, camera );
    float3 right = VisibilityWorld( pixel + int2( 1, 0 ), size, camera ) - world;
    float3 up = world - VisibilityWorld( pixel - int2( 0, 1 ), size, camera );
    float3 down = VisibilityWorld( pixel + int2( 0, 1 ), size, camera ) - world;
    float3 dx = dot( left, left ) > 1e-12 && ( dot( right, right ) <= 1e-12 || dot( left, left ) < dot( right, right ) ) ? left : right;
    float3 dy = dot( up, up ) > 1e-12 && ( dot( down, down ) <= 1e-12 || dot( up, up ) < dot( down, down ) ) ? up : down;
    float3 normal = cross( dx, dy );
    normal *= rsqrt( max( dot( normal, normal ), 1e-12 ) );

    uint cascade = 0;
    while ( cascade + 1 < record.FaceCount && depth > record.CascadeSplits[cascade] ) {
        ++cascade;
    }

    float visibility = VisibilityCascade( record, cascade, world, normal );
    float start = cascade == 0 ? record.PositionRange.w : record.CascadeSplits[cascade - 1];
    float span = max( record.CascadeSplits[cascade] - start, 1e-4 );
    float blend = saturate( ( depth - ( record.CascadeSplits[cascade] - 0.1 * span ) ) / ( 0.1 * span ) );
    if ( blend > 0 ) {
        float next = cascade + 1 < record.FaceCount ? VisibilityCascade( record, cascade + 1, world, normal ) : 1.0;
        visibility = lerp( visibility, next, blend );
    }
    return visibility;
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 dispatch : SV_DispatchThreadID ) {
    uint width, height;
    images[parameters.Output].GetDimensions( width, height );
    int2 pixel = int2( dispatch.xy );
    if ( any( dispatch.xy >= uint2( width, height ) ) ) {
        return;
    }

    if ( parameters.Stage == VisibilityStage::eCopyDepth ) {
        images[parameters.Output][pixel] = textures[parameters.Depth].Load( int3( pixel, 0 ) ).r;
        return;
    }

    CameraInfo camera = LoadBufferValue<CameraInfo>( parameters.Camera, 0 );
    uint2 depthSize;
    textures[parameters.Depth].GetDimensions( depthSize.x, depthSize.y );
    float2 uv = ( float2( pixel ) + 0.5 ) / float2( width, height );
    int2 guidePixel = clamp( int2( uv * float2( depthSize ) ), int2( 0 ), int2( depthSize ) - 1 );
    float receiverDepth = textures[parameters.Depth].Load( int3( guidePixel, 0 ) ).r;
    if ( receiverDepth >= 1.0 ) {
        images[parameters.Output][pixel] = 1.0;
        return;
    }

    if ( parameters.Stage == VisibilityStage::eEvaluate ) {
        images[parameters.Output][pixel] = EvaluateVisibility( pixel, depthSize, camera );
        return;
    }

    uint2 inputSize;
    textures[parameters.Input].GetDimensions( inputSize.x, inputSize.y );
    float centerDepth = VisibilityViewDepth( guidePixel, depthSize, camera );
    float sum = 0.0;
    float weights = 0.0;

    if ( parameters.Stage == VisibilityStage::eBlurHorizontal || parameters.Stage == VisibilityStage::eBlurVertical ) {
        int2 axis = parameters.Stage == VisibilityStage::eBlurHorizontal ? int2( 1, 0 ) : int2( 0, 1 );
        for ( int tap = -3; tap <= 3; ++tap ) {
            int2 neighbor = clamp( pixel + axis * tap, int2( 0 ), int2( inputSize ) - 1 );
            int2 depthPixel = int2( ( float2( neighbor ) + 0.5 ) / float2( inputSize ) * float2( depthSize ) );
            float weight = exp( -float( tap * tap ) / 4.5 ) * VisibilityDepthWeight( centerDepth, VisibilityViewDepth( depthPixel, depthSize, camera ) );
            sum += textures[parameters.Input].Load( int3( neighbor, 0 ) ).r * weight;
            weights += weight;
        }
    }
    else {
        float2 source = uv * float2( inputSize ) - 0.5;
        int2 basePixel = int2( floor( source ) );
        float2 fraction = frac( source );
        for ( int y = 0; y < 2; ++y ) {
            for ( int x = 0; x < 2; ++x ) {
                int2 neighbor = clamp( basePixel + int2( x, y ), int2( 0 ), int2( inputSize ) - 1 );
                int2 depthPixel = int2( ( float2( neighbor ) + 0.5 ) / float2( inputSize ) * float2( depthSize ) );
                float spatial = ( x == 0 ? 1.0 - fraction.x : fraction.x ) * ( y == 0 ? 1.0 - fraction.y : fraction.y );
                float weight = spatial * VisibilityDepthWeight( centerDepth, VisibilityViewDepth( depthPixel, depthSize, camera ) );
                sum += textures[parameters.Input].Load( int3( neighbor, 0 ) ).r * weight;
                weights += weight;
            }
        }
    }

    // At a thin silhouette, half-resolution taps may all belong to another surface.
    // Fall back to the original visibility instead of inventing an unshadowed receiver.
    float fallback = parameters.Stage == VisibilityStage::eUpsample ?
        textures[parameters.RawVisibility].Load( int3( pixel, 0 ) ).r : textures[parameters.Input].Load( int3( clamp( int2( uv * float2( inputSize ) ), int2( 0 ), int2( inputSize ) - 1 ), 0 ) ).r;
    images[parameters.Output][pixel] = saturate( weights > 1e-5 ? sum / weights : fallback );
}
