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

// Cloud record layout, bounds, density and optical-depth queries. Local coordinates describe
// each cloud volume; extinction and integration distances are evaluated in world-space units.

#pragma once

#include "Memory_Helpers.slang"

// Local voxel fields and a separate light cache follow Nubis³'s data flow.
// https://www.guerrilla-games.com/read/nubis-cubed
// https://advances.realtimerendering.com/s2023/Nubis%20Cubed%20(Advances%202023).pdf
// These procedural authoring/noise functions are Mikoto's own implementation.
// Distance transforms store occupied/empty squared distances, then pack four conservative 8-bit
// skip bounds per uint. Dispatch widths match CloudsModule.
static const uint kCloudDistanceLineCapacity = 256;
static const uint kCloudDistanceChannelCount = 2;
static const uint kCloudDistancePackAxis = 3;
static const uint kCloudDistancePackedVoxels = 4;
static const uint kCloudDistanceBitsPerVoxel = 8;
static const uint kCloudDistanceMaxCode = 255;
static const uint kCloudDistanceLineDispatchWidth = 16384u;
static const uint kCloudDistancePackDispatchWidth = 4194304u;

struct CloudRecord {
    float4x4 mWorldToLocal;
    float4x4 mLocalToWorld;
    float4 mExtinction;
    float4 mScatteringPhase;
    float4 mDensityDetail;
    float4 mShape;
    float4 mWind;
    uint4 mFlags;
    uint4 mGrid;
    uint4 mTextures;
    float4 mWeather;
    float4 mFormation;
    float4 mProfile;
    float4 mLightningPositionRadius;
    float4 mLightningColorIntensity;
};

struct CloudsFrameData {
    float4x4 mInverseViewProjection;
    float4x4 mPreviousViewProjection;
    float4x4 mPreviousInverseViewProjection;
    float4 mCamera;
    float4 mPreviousCamera;
    float4 mBoundsMin;
    float4 mBoundsMax;
    float4 mSunDirection;
    float4 mSunColor;
    float4 mAmbientColor;
    float4 mScreen;
    float4 mMarch;
    float4 mClock;
    uint4 mControl;
    uint4 mResources;
    uint4 mResolution;
    uint4 mFiltering;
    uint4 mAtmosphereTextures;
    uint4 mFogTextures;
    uint4 mNoiseMips;
    float4 mDetail;
    uint4 mShapeResources;
    uint4 mLightingControl;
    uint4 mLocalTextures[2];
    uint64_t mInteractions;
    uint64_t mLocalLights;
    uint64_t mMeshShadows;
    uint64_t mBolts;
    uint64_t mModels;
    uint64_t mAtmosphere;
    uint64_t mFogFrame;
    uint64_t mDistanceFields;
    uint64_t mTerrainVertices;
    uint64_t mTerrainRecords;
    uint64_t mTerrainBounds;
};

MKT_DECLARE_TEXTURE_SRV( Texture3D<float4> )
cloudVolumes;
MKT_DECLARE_SAMPLERS cloudSamplers;
MKT_DECLARE_TEXTURE_SRV( Texture2D<float4> )
cloudWeatherMaps;

CloudRecord LoadCloud( CloudsFrameData frame, uint index ) {
    return LoadBufferValue<CloudRecord>( frame.mModels, 16 + index * sizeof( CloudRecord ) );
}

bool CloudBoxInterval( float3 origin, float3 direction, float3 minimum, float3 maximum, out float2 interval ) {
    float3 inverse = ( 2 * step( float3( 0 ), direction ) - 1 ) / max( abs( direction ), 1e-9 );
    float3 a = ( minimum - origin ) * inverse;
    float3 b = ( maximum - origin ) * inverse;
    float3 lo = min( a, b );
    float3 hi = max( a, b );
    interval = float2( max( lo.x, max( lo.y, lo.z ) ), min( hi.x, min( hi.y, hi.z ) ) );
    return interval.y >= max( interval.x, 0 );
}

// Intersect the world ray with a cloud-local unit box and return its world-ray interval.
// Restricting the march to occupied volume bounds avoids evaluating the whole sky.
bool CloudInterval( CloudRecord cloud, float3 origin, float3 direction, out float2 interval ) {
    return CloudBoxInterval( mul( cloud.mWorldToLocal, float4( origin, 1 ) ).xyz,
        mul( cloud.mWorldToLocal, float4( direction, 0 ) ).xyz, -0.5, 0.5, interval );
}

float CloudBoxDistance( float3 p ) {
    float3 q = abs( p ) - 0.5;
    return length( max( q, 0 ) ) + min( max( q.x, max( q.y, q.z ) ), 0 );
}

// Use the shortest physical extent as the profile's reference length. Otherwise
// a wide, thin sky layer acquires shallow profiles from its horizontal scaling.
// Account for the volume transform when converting local field scale to a world-space distance.
// Nonuniform scaling changes the conservative step size.
float3 CloudMetricExtent( CloudRecord cloud ) {
    float3 extent = float3( length( mul( cloud.mLocalToWorld, float4( 1, 0, 0, 0 ) ).xyz ),
        length( mul( cloud.mLocalToWorld, float4( 0, 1, 0, 0 ) ).xyz ),
        length( mul( cloud.mLocalToWorld, float4( 0, 0, 1, 0 ) ).xyz ) );
    return extent / max( min( extent.x, min( extent.y, extent.z ) ), 1e-6 );
}

float4 CloudField( CloudsFrameData frame, CloudRecord cloud, float3 position ) {
    float3 p = mul( cloud.mWorldToLocal, float4( position, 1 ) ).xyz;
    if ( any( abs( p ) > 0.5 ) ) {
        float distance = CloudBoxDistance( p ) * cloud.mShape.z;
        return float4( distance, 0, 0, 0 );
    }
    if ( cloud.mTextures.w != 0 && frame.mDistanceFields != 0 ) {
        uint3 voxel = min( uint3( ( p + .5 ) * float3( frame.mResolution.xyz ) ), frame.mResolution.xyz - 1 );
        uint offset = cloud.mTextures.z + voxel.x + frame.mResolution.x * ( voxel.y + frame.mResolution.y * voxel.z );
        uint packed = LoadBufferValue<uint>( frame.mDistanceFields, ( offset / kCloudDistancePackedVoxels ) * sizeof( uint ) );
        float distance = float( ( packed >> ( ( offset % kCloudDistancePackedVoxels ) * kCloudDistanceBitsPerVoxel ) ) & kCloudDistanceMaxCode ) / float( kCloudDistanceMaxCode ) * cloud.mShape.z;
        if ( distance > 0 ) {
            return float4( distance, 0, 0, 0 );
        }
    }
    float4 field = cloudVolumes[NonUniformResourceIndex( cloud.mTextures.x )].SampleLevel(
        cloudSamplers[frame.mResources.y], p + 0.5, 0 );
    // A filtered coarse distance can overestimate the true distance by a cell
    // diagonal. Subtract it before skipping; micro-erosion never expands support.
    float guard = length( CloudMetricExtent( cloud ) / float3( frame.mResolution.xyz ) );
    field.r = ( field.r - guard ) * cloud.mShape.z;
    return field;
}

// Shape is baked in metres in the cloud's own frame. The cached weather and
// height profile create separate banks; detail noise then breaks up their bodies
// and silhouettes instead of merely trimming a few ellipsoid rims.
float3 CloudMaterialPosition( CloudRecord cloud, float3 local ) {
    float3 extent = float3( length( mul( cloud.mLocalToWorld, float4( 1, 0, 0, 0 ) ).xyz ),
        length( mul( cloud.mLocalToWorld, float4( 0, 1, 0, 0 ) ).xyz ),
        length( mul( cloud.mLocalToWorld, float4( 0, 0, 1, 0 ) ).xyz ) );
    return local * extent;
}

float4 CloudDetailNoise( CloudsFrameData frame, float3 uv, float lod ) {
    lod = clamp( lod, 0, 4 );
    uint low = uint( floor( lod ) );
    uint high = min( low + 1, 4u );
    uint a = low == 0 ? frame.mResources.x : frame.mNoiseMips[max( low, 1u ) - 1];
    uint b = high == 0 ? frame.mResources.x : frame.mNoiseMips[max( high, 1u ) - 1];
    float4 first = cloudVolumes[NonUniformResourceIndex( a )].SampleLevel( cloudSamplers[frame.mResources.z], uv, 0 );
    float4 second = cloudVolumes[NonUniformResourceIndex( b )].SampleLevel( cloudSamplers[frame.mResources.z], uv, 0 );
    return lerp( first, second, frac( lod ) );
}

float4 CloudDetailNoise( CloudsFrameData frame, float3 uv ) {
    return cloudVolumes[frame.mResources.x].SampleLevel( cloudSamplers[frame.mResources.z], uv, 0 );
}

float4 CloudWeather( CloudsFrameData frame, CloudRecord cloud, float3 materialPosition ) {
    float seed = float( cloud.mFlags.z % 4096u ) * .0137;
    float2 uv = materialPosition.xz * cloud.mWeather.x + float2( seed, seed * .713 );
    return cloudWeatherMaps[frame.mShapeResources.y].SampleLevel( cloudSamplers[frame.mResources.z], uv, 0 );
}

#include "CloudModeling_Helpers.slang"

// Evaluate the coarse cloud profile before detail erosion. Authored density and seeded
// procedural formations meet at this shared density interface.
float CloudBaseDensity( CloudsFrameData frame, CloudRecord cloud, float3 local ) {
    // Authoring-space wind shear changes the shape without making it camera-dependent.
    float2 wind = cloud.mProfile.zw;
    wind = dot( wind, wind ) > 1e-6 ? normalize( wind ) : float2( 1, 0 );
    float3 worldExtent = float3( length( mul( cloud.mLocalToWorld, float4( 1, 0, 0, 0 ) ).xyz ),
        length( mul( cloud.mLocalToWorld, float4( 0, 1, 0, 0 ) ).xyz ), length( mul( cloud.mLocalToWorld, float4( 0, 0, 1, 0 ) ).xyz ) );
    float maximumShear = min( worldExtent.x, worldExtent.z ) * .25;
    local.xz -= wind * clamp( cloud.mProfile.y, -maximumShear, maximumShear ) * pow( saturate( local.y + .5 ), 2.0 ) / max( worldExtent.xz, 1e-5 );
    return CloudSeededProfile( frame, cloud, local );
}

struct CloudInteractionRecord {
    float4x4 mWorldToLocal;
    float4 mParameters;
};

// Evaluate local cloud interaction controls in the density domain. The result modulates the
// formation without changing the outer ray/box intersection.
float CloudInteractionMask( CloudsFrameData frame, float3 position ) {
    float mask = 1;
    for ( uint index = 0; index < frame.mLightingControl.w; ++index ) {
        CloudInteractionRecord interaction = LoadBufferValue<CloudInteractionRecord>( frame.mInteractions, index * sizeof( CloudInteractionRecord ) );
        float3 p = mul( interaction.mWorldToLocal, float4( position, 1 ) ).xyz;
        if ( any( abs( p ) > .5 ) ) {
            continue;
        }
        float distance;
        if ( interaction.mParameters.x < .5 ) {
            distance = length( p ) - .5;
        }
        else if ( interaction.mParameters.x < 1.5 ) {
            distance = CloudBoxDistance( p );
        }
        else {
            p.y -= clamp( p.y, -.25, .25 );
            distance = length( p ) - .25;
        }
        float feather = max( interaction.mParameters.z * .5, .00001 );
        float influence = 1 - smoothstep( -feather, 0, distance );
        mask *= 1 - influence * interaction.mParameters.y;
        if ( mask <= 0 ) {
            break;
        }
    }
    return mask;
}

// Combine coarse density, weather and detailed erosion at the sample point. Keeping this query
// shared aligns the camera march with light transmittance evaluation.
float CloudDensity( CloudsFrameData frame, CloudRecord cloud, float3 position, float4 field ) {
    if ( field.a <= 0 || field.b <= 0 ) {
        return 0;
    }

    float density = field.a;

    if ( cloud.mFlags.y == 2 || cloud.mFlags.y == 3 ) {
        float3 local = mul( cloud.mWorldToLocal, float4( position, 1 ) ).xyz;
        float3 materialPosition = CloudMaterialPosition( cloud, local );
        float distance = length( position - frame.mCamera.xyz );
        float footprint = distance * cloud.mDensityDetail.z * 128 / max( frame.mScreen.y, 1 );
        float lod = max( log2( max( footprint, 1 ) ) + frame.mDetail.z, 0 );
        float4 noise = CloudDetailNoise( frame, materialPosition * cloud.mDensityDetail.z, lod );
        float profile = saturate( field.a );
        // Nubis3 slides 101..122: profile-driven RG/BA composites, folded
        // near-camera detail, then erosion and independent density scaling.
        float wispy = lerp( noise.r, noise.g, profile );
        float billowy = lerp( noise.b, noise.a, pow( profile, .25 ) ) * .3;
        float erosion = lerp( wispy, billowy, field.g );
        float foldedWisps = 1 - pow( abs( abs( noise.g * 2 - 1 ) * 2 - 1 ), 4.0 );
        float foldedBillows = pow( abs( abs( noise.a * 2 - 1 ) * 2 - 1 ), 2.0 );
        float nearDetail = ( 1 - smoothstep( 50, 150, distance ) ) * .1 * saturate( 1 - lod );
        erosion = lerp( erosion, lerp( foldedWisps, foldedBillows, field.g ), nearDetail );
        float amount = saturate( cloud.mDensityDetail.w * frame.mClock.z );
        float cut = erosion * amount;
        density = saturate( ( profile - cut ) / max( 1 - cut, .001 ) );
        // Authored density is a separate field; scale before sharpening as in
        // the published up-rez function, while zero erosion preserves the medium.
        float densityScale = pow( saturate( field.b ), 4.0 );
        float sharpened = pow( max( density * densityScale, 0.0 ), lerp( .3, .6, densityScale ) );
        float close = 1 - smoothstep( 50, 150, distance );
        sharpened = pow( max( sharpened, 0.0 ), lerp( 1, .5, close ) ) * lerp( 1, .666, close );
        return lerp( density * field.b, sharpened, amount ) * max( cloud.mDensityDetail.x, 0 ) * CloudInteractionMask( frame, position );
    }

    return density * field.b * max( cloud.mDensityDetail.x, 0 ) * CloudInteractionMask( frame, position );
}

float3 CloudExtinction( CloudsFrameData frame, float3 position, bool shadowOnly ) {
    float3 extinction = 0;
    for ( uint index = 0; index < frame.mControl.x; ++index ) {
        CloudRecord cloud = LoadCloud( frame, index );
        if ( shadowOnly && ( cloud.mFlags.w & 1u ) == 0 ) {
            continue;
        }
        float4 field = CloudField( frame, cloud, position );
        extinction += CloudDensity( frame, cloud, position, field ) * cloud.mExtinction.rgb;
    }
    return extinction;
}

// Evaluate the configured angular scattering response between incoming light and the camera
// ray. This controls forward/backward brightening independently of extinction.
float CloudPhase( float cosine, float g ) {
    g = clamp( g, -.95, .95 );
    return ( 1 - g * g ) / ( 12.56637061435917 * pow( max( 1 + g * g - 2 * g * cosine, 1e-4 ), 1.5 ) );
}

float3 CloudSurfaceShadow( uint64_t address, float3 position, float3 lightDirection ) {
    if ( address == 0 ) {
        return 1;
    }
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( address, 0 );
    if ( frame.mResources.w == 0 || frame.mControl.x == 0 || dot( lightDirection, frame.mSunDirection.xyz ) < .999 ) {
        return 1;
    }
    float entry = 1e30;
    uint selected = 0;
    for ( uint index = 0; index < frame.mControl.x; ++index ) {
        CloudRecord cloud = LoadCloud( frame, index );
        float2 interval;
        if ( ( cloud.mFlags.w & 1u ) != 0 && CloudInterval( cloud, position, frame.mSunDirection.xyz, interval ) && max( interval.x, 0 ) < entry ) {
            entry = max( interval.x, 0 );
            selected = index;
        }
    }
    if ( entry == 1e30 ) {
        return 1;
    }
    CloudRecord cloud = LoadCloud( frame, selected );
    float3 sample = position + frame.mSunDirection.xyz * ( entry + .01 );
    float3 local = mul( cloud.mWorldToLocal, float4( sample, 1 ) ).xyz + .5;
    float3 opticalDepth = cloudVolumes[NonUniformResourceIndex( cloud.mTextures.y )].SampleLevel(
                                                                                       cloudSamplers[frame.mResources.y], saturate( local ), 0 )
                              .rgb;
    return exp( -max( opticalDepth, 0.0 ) );
}

// Empty-space skipping pays for both sun and upward-sky cache rays. The same
// conservative envelope guards used by view rays keep thin formations intact.
// Sample the low-frequency light-path extinction cache. The cached quantity is optical depth,
// which becomes transmittance after exponentiation.
float3 CloudCachedOpticalDepth( CloudsFrameData frame, float3 origin, float3 direction, float distance, bool shadowOnly ) {
    float3 result = 0;
    float travelled = 0;
    uint budget = max( frame.mResolution.w, 1u );
    for ( uint stepIndex = 0; stepIndex < budget && travelled < distance; ++stepIndex ) {
        float spacing = ( distance - travelled ) / max( float( budget - stepIndex ), 1.0 );
        float3 position = origin + direction * ( travelled + spacing * .5 );
        float3 sigma = 0;
        float nearest = 1e20;
        for ( uint i = 0; i < frame.mControl.x; ++i ) {
            CloudRecord cloud = LoadCloud( frame, i );
            if ( shadowOnly && ( cloud.mFlags.w & 1u ) == 0 ) {
                continue;
            }
            float4 field = CloudField( frame, cloud, position );
            nearest = min( nearest, field.r );
            sigma += CloudDensity( frame, cloud, position, field ) * cloud.mExtinction.rgb;
        }
        result += sigma * spacing;
        travelled += spacing;
        if ( nearest > spacing * 2 ) {
            travelled += min( max( nearest * .8 - spacing * .5, 0.0 ), distance - travelled );
        }
        if ( all( result > 12 ) ) {
            break;
        }
    }
    return min( result, 80.0 );
}
