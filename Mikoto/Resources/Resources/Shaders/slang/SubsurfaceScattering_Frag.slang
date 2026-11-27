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

// Discrete artist-friendly Gaussian convolution, independently implemented from:
// https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/#2-scattering-the-diffuse-lighting
// https://github.com/bentoBAUX/Separable-Subsurface-Scattering-in-Unity-6
// Kernel weights are normalized on CPU; offsets are integer screen pixels.

// Blur the diffuse lighting using the configured subsurface profile. Depth-aware weights limit
// scattering across unrelated surfaces.

#include "base/Memory_Helpers.slang"
#include "base/Material_Helpers.slang"

// Matches the per-material kernel allocation and supported tap range in SubsurfaceScatteringModule.
static const uint kSubsurfaceMinTaps = 7u;
static const uint kSubsurfaceKernelCapacity = 65u;

struct Parameters {
    Pointer mMaterials;
    Pointer mKernels;
    uint mOriginal;
    uint mInput;
    uint mMetadata;
    uint mAxis;
    uint mWidth;
    uint mHeight;
};

MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );
MKT_DECLARE_TEXTURE_SRV( Texture2D ) images;

[shader( "fragment" )]
float4 main( float2 unusedUV : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    int2 pixel = int2( position.xy );
    float4 original = images[parameters.mOriginal].Load( int3( pixel, 0 ) );
    if ( original.a <= 0 ) {
        return float4( 0 );
    }
    uint index = uint( original.a ) - 1;
    MaterialParameters material = ( ( BufferReference<MaterialParameters> )parameters.mMaterials )[index];
    float3 center = images[parameters.mInput].Load( int3( pixel, 0 ) ).rgb;
    if ( material.mSubsurface.x <= 0 || material.mSubsurface.y <= 0 ) {
        return float4( center, 0 );
    }
    // Load one tap at a time rather than materializing a 65-float4 local array.
    // The uploaded RGB kernel stores scattering weights and an integer pixel offset. The
    // selected pass axis makes the profile separable into horizontal and vertical filtering.
    BufferReference<float4> kernels = ( BufferReference<float4> )parameters.mKernels;
    float4 metadata = images[parameters.mMetadata].Load( int3( pixel, 0 ) );
    int2 axis = parameters.mAxis == 0 ? int2( 1, 0 ) : int2( 0, 1 );
    float3 filtered = float3( 0 );
    uint count = clamp( uint( material.mSurfaceShading.z ), kSubsurfaceMinTaps, kSubsurfaceKernelCapacity );
    for ( uint tap = 0; tap < count; ++tap ) {
        float4 k = kernels[index * kSubsurfaceKernelCapacity + tap];
        int2 location = clamp( pixel + axis * int( k.w ), int2( 0 ), int2( parameters.mWidth, parameters.mHeight ) - 1 );
        float4 sampleMetadata = images[parameters.mMetadata].Load( int3( location, 0 ) );
        float3 value = center;
        // Extend center radiance at silhouettes/other instances. This retains DC
        // energy and avoids the reference's background bleed and brightness mask.
        if ( sampleMetadata.a == metadata.a ) {
            value = images[parameters.mInput].Load( int3( location, 0 ) ).rgb;
            float threshold = material.mSurfaceShading.y;
            if ( threshold > 0 ) {
                float relativeDepth = abs( sampleMetadata.x - metadata.x ) / max( abs( metadata.x ), 1e-4 );
                value = lerp( value, center, smoothstep( threshold, threshold * 2, relativeDepth ) );
            }
        }
        filtered += k.rgb * value;
    }
    return float4( filtered, 0 );
}
