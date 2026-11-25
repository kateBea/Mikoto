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

#include "base/SSR_Common.slang"

struct Parameters {
    Pointer Frame;
    Pointer Camera;
    uint DepthMips[16];
    uint MipCount;
    uint Position;
    uint Normal;
    uint Surface;
    uint HitOutput;
    uint DirectionPdfOutput;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) images;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

float3 SampleDirection( uint2 pixel, float3 normal, float3 view, float roughness, SsrFrame frame, out float pdf ) {
    float3 tangent = normalize( cross( abs( normal.z ) < 0.999 ? float3( 0, 0, 1 ) : float3( 1, 0, 0 ), normal ) );
    float3 bitangent = cross( normal, tangent );
    float alpha = max( roughness * roughness, 0.002 );
    float3 light = reflect( -view, normal );
    pdf = 0;

    for ( uint attempt = 0; attempt < 4; ++attempt ) {
        float2 u = SsrSequence( pixel, frame.Control.x, attempt );
        // Truncate the GGX tail in its inverse CDF. The constant PDF scaling
        // cancels in the normalized BRDF/PDF reconstruction (Frostbite slide 68).
        u.y *= 1 - frame.Trace.w;
        float cosine = sqrt( ( 1 - u.y ) / max( 1 + ( alpha * alpha - 1 ) * u.y, 1e-6 ) );
        float sine = sqrt( max( 1 - cosine * cosine, 0 ) );
        float phi = u.x * 6.28318530718;
        float3 halfVector = normalize( tangent * ( cos( phi ) * sine ) + bitangent * ( sin( phi ) * sine ) + normal * cosine );
        light = reflect( -view, halfVector );

        if ( dot( light, normal ) > 0.001 && dot( view, halfVector ) > 0.001 ) {
            pdf = SsrDistribution( roughness, saturate( dot( normal, halfVector ) ) ) *
                  saturate( dot( normal, halfVector ) ) / ( 4 * dot( view, halfVector ) );
            break;
        }
    }

    return normalize( light );
}

bool Trace( float3 originView, float3 directionView, float3 directionWorld, CameraInfo camera, SsrFrame frame,
            out float2 hitUv, out float hitLength, out float confidence ) {
    hitUv = 0;
    hitLength = 0;
    confidence = 0;
    float distance = frame.Trace.x;

    // Clip camera-facing rays before the projection's W changes sign.
    if ( directionView.z > 0 ) {
        distance = min( distance, ( -camera.mPlanes.x * 1.01 - originView.z ) / directionView.z );
    }
    if ( distance <= 0.01 ) {
        return false;
    }

    float3 origin = SsrProject( originView, camera, frame );
    float3 end = SsrProject( originView + directionView * distance, camera, frame );
    float3 direction = end - origin;
    if ( dot( direction.xy, direction.xy ) < 1e-12 ) {
        return false;
    }

    float current = 0;
    uint mip = 0;
    float2 sign = float2( direction.x >= 0 ? 1 : -1, direction.y >= 0 ? 1 : -1 );
    float2 inverse = float2( abs( direction.x ) > 1e-8 ? 1 / direction.x : 1e20,
                             abs( direction.y ) > 1e-8 ? 1 / direction.y : 1e20 );

    // Stackless min-Z traversal. Unlike AMD/Wicked's reversed device-depth
    // variants, our hierarchy normalizes both camera conventions to near=0.
    // https://github.com/GPUOpen-Effects/FidelityFX-SSSR/blob/master/ffx-sssr/ffx_sssr.h
    for ( uint iteration = 0; iteration < frame.Control.w; ++iteration ) {
        float3 position = origin + direction * current;
        if ( current > 1 || !SsrInside( position.xy ) || position.z < 0 || position.z > 1 ) {
            break;
        }

        uint width, height;
        uint texture = parameters.DepthMips[mip];
        textures[NonUniformResourceIndex( texture )].GetDimensions( width, height );
        float2 dimensions = float2( width, height );
        int2 cell = clamp( int2( position.xy * dimensions ), 0, int2( width, height ) - 1 );
        float surface = textures[NonUniformResourceIndex( texture )].Load( int3( cell, 0 ) ).r;
        float2 boundary = ( float2( cell ) + float2( direction.x >= 0 ? 1 : 0, direction.y >= 0 ? 1 : 0 ) ) / dimensions;
        boundary += sign * ( 0.005 / frame.Screen.xy );
        float2 crossings = ( boundary - origin.xy ) * inverse;
        float leave = max( min( crossings.x, crossings.y ), current + 1e-7 );
        float zCross = abs( direction.z ) > 1e-12 ? ( surface - origin.z ) / direction.z : 1e20;
        bool crossesPlane = zCross >= current && zCross <= leave;
        bool behind = position.z >= surface;

        if ( surface < 1 && ( behind || crossesPlane ) ) {
            if ( mip > 0 ) {
                --mip;
                continue;
            }

            float candidateTime = crossesPlane ? zCross : current;
            float3 candidate = origin + direction * candidateTime;
            int2 pixel = clamp( int2( candidate.xy * frame.Screen.xy ), 0, int2( frame.Screen.xy ) - 1 );
            float4 scenePosition = textures[parameters.Position].Load( int3( pixel, 0 ) );
            float difference = abs( SsrViewDepth( candidate, camera, frame ) - scenePosition.w );
            float3 hitNormal = normalize( textures[parameters.Normal].Load( int3( pixel, 0 ) ).xyz * 2 - 1 );
            float lengthToHit = length( scenePosition.xyz - originView );

            if ( scenePosition.w > 0 && difference <= frame.Trace.y && lengthToHit > max( 0.05, frame.Trace.y * 0.5 ) &&
                 dot( hitNormal, -directionWorld ) > 0.01 ) {
                hitUv = candidate.xy;
                hitLength = lengthToHit;
                float edge = min( min( hitUv.x, hitUv.y ), min( 1 - hitUv.x, 1 - hitUv.y ) );
                confidence = smoothstep( 0, 0.06, edge ) * ( 1 - smoothstep( 0.8, 1, lengthToHit / frame.Trace.x ) );
                return true;
            }
        }

        current = leave;
        mip = min( mip + 1, parameters.MipCount - 1 );
    }
    return false;
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    SsrFrame frame = ( ( BufferReference<SsrFrame> )parameters.Frame )[0];
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.Screen.zw ) ) ) {
        return;
    }

    images[parameters.HitOutput][pixel] = 0;
    images[parameters.DirectionPdfOutput][pixel] = 0;
    uint2 fullPixel = pixel;
    if ( any( frame.Screen.zw != frame.Screen.xy ) ) {
        uint2 phase = uint2( frame.Control.x & 1u, ( frame.Control.x >> 1u ) & 1u );
        fullPixel = min( pixel * 2 + phase, uint2( frame.Screen.xy ) - 1 );
    }

    float4 position = textures[parameters.Position].Load( int3( fullPixel, 0 ) );
    float4 surface = textures[parameters.Surface].Load( int3( fullPixel, 0 ) );
    if ( position.w <= 0 || surface.w > frame.Trace.z || !any( surface.rgb > 0 ) ) {
        return;
    }

    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.Camera )[0];
    float3 normal = normalize( textures[parameters.Normal].Load( int3( fullPixel, 0 ) ).xyz * 2 - 1 );
    float3 world = SsrWorldPosition( position.xyz, frame );
    float3 view = camera.mProjection[3][3] != 0 ? normalize( mul( frame.InverseView, float4( 0, 0, 1, 0 ) ).xyz )
                                               : normalize( camera.mViewPosition.xyz - world );
    float pdf;
    float3 direction = SampleDirection( fullPixel, normal, view, surface.w, frame, pdf );
    if ( pdf <= 0 ) {
        return;
    }

    images[parameters.DirectionPdfOutput][pixel] = float4( direction, pdf );
    float3 viewNormal = normalize( mul( camera.mViewMatrix, float4( normal, 0 ) ).xyz );
    float3 viewDirection = normalize( mul( camera.mViewMatrix, float4( direction, 0 ) ).xyz );
    float3 origin = position.xyz + viewNormal * max( 0.015, frame.Trace.y * 0.15 );
    float2 hit;
    float distance;
    float confidence;
    if ( Trace( origin, viewDirection, direction, camera, frame, hit, distance, confidence ) ) {
        images[parameters.HitOutput][pixel] = float4( hit, distance, confidence );
    }
}
