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

// Build conservative empty-space information from cloud density. Later ray marching uses this
// field to skip empty regions without stepping through every density voxel.

#include "base/Clouds_Helpers.slang"

struct CloudDistanceParameters {
    uint64_t mFrame;
    uint64_t mInput;
    uint64_t mOutput;
    uint mVolume;
    uint mAxis;
    uint mModelOutput;
};

MKT_DECLARE_PUSH_CONSTANT( CloudDistanceParameters, parameters );
MKT_DECLARE_TEXTURE_UAV( RWTexture3D<float4> )
cloudModelOutputs;

groupshared float2 lineSource[kCloudDistanceLineCapacity];
groupshared float2 lineResult[kCloudDistanceLineCapacity];
groupshared int features[kCloudDistanceLineCapacity];
groupshared float boundaries[kCloudDistanceLineCapacity + 1];
groupshared uint featureCount;

// Each line uses the lower envelope of squared-distance parabolas. Computing
// both nearest occupied and nearest empty cells supplies a signed envelope,
// without a quadratic scan per voxel or an iterative jump-flood approximation.
[shader( "compute" )]
[numthreads( kCloudDistanceLineCapacity, 1, 1 )]
void main( uint3 dispatch : SV_DispatchThreadID, uint3 group : SV_GroupID, uint3 thread : SV_GroupThreadID ) {
    CloudsFrameData frame = LoadBufferValue<CloudsFrameData>( parameters.mFrame, 0 );
    uint3 size = frame.mResolution.xyz;
    uint count = size.x * size.y * size.z;
    CloudRecord cloud = LoadCloud( frame, parameters.mVolume );
    float3 metricExtent = CloudMetricExtent( cloud );
    float3 voxelSize = metricExtent / float3( size );

    // After the three axis transforms, convert exact squared distances into conservative byte-
    // sized skip bounds. Each invocation packs four voxels while the full-precision modeling
    // field keeps its signed distance.
    if ( parameters.mAxis == kCloudDistancePackAxis ) {
        uint index = dispatch.x + dispatch.y * kCloudDistancePackDispatchWidth;
        if ( index >= ( count + ( kCloudDistancePackedVoxels - 1 ) ) / kCloudDistancePackedVoxels ) {
            return;
        }
        float guard = 2 * length( voxelSize );
        float halfVoxel = .5 * min( voxelSize.x, min( voxelSize.y, voxelSize.z ) );
        uint packed = 0;
        for ( uint lane = 0; lane < kCloudDistancePackedVoxels; ++lane ) {
            uint offset = index * kCloudDistancePackedVoxels + lane;
            if ( offset >= count ) {
                continue;
            }
            uint3 voxel = uint3( offset % size.x, ( offset / size.x ) % size.y, offset / ( size.x * size.y ) );
            float2 squared = LoadBufferValue<float2>( parameters.mInput, offset * sizeof( float2 ) );
            float4 field = cloudModelOutputs[parameters.mModelOutput][voxel];
            float3 uv = ( float3( voxel ) + .5 ) / float3( size );
            float3 edges = min( uv, 1 - uv ) * metricExtent;
            float inside = min( max( sqrt( squared.y ) - halfVoxel, 0 ), min( edges.x, min( edges.y, edges.z ) ) );
            bool occupied = field.a > 0;
            field.r = occupied ? -inside : min( max( sqrt( squared.x ) - halfVoxel, 0 ), 4 );
            if ( ( cloud.mFlags.w & 4u ) == 0 && cloud.mFlags.y == 3 ) {
                field.a = occupied ? saturate( inside / max( cloud.mShape.w, .001 ) ) : 0;
            }
            cloudModelOutputs[parameters.mModelOutput][voxel] = field;

            // Procedural/imported profiles are independent of this acceleration field.
            // Only the positive, rounded-down bound guides skipping. The signed
            // field and profile keep full precision for density reconstruction.
            uint value = uint( floor( saturate( sqrt( squared.x ) - guard - 1e-6 ) * float( kCloudDistanceMaxCode ) ) );
            packed |= value << ( lane * kCloudDistanceBitsPerVoxel );
        }
        StoreBufferValue<uint>( parameters.mOutput, parameters.mVolume * count + index * sizeof( uint ), packed );
        return;
    }

    uint axis = parameters.mAxis;
    uint line = group.x + group.y * kCloudDistanceLineDispatchWidth;
    if ( line >= count / size[axis] ) {
        return;
    }
    uint lane = thread.x;
    uint3 voxel = axis == 0 ? uint3( lane, line % size.y, line / size.y ) : axis == 1 ? uint3( line % size.x, lane, line / size.x )
                                                                                      : uint3( line % size.x, line / size.x, lane );
    uint index = voxel.x + size.x * ( voxel.y + size.y * voxel.z );
    float2 source = 1e20;
    if ( lane < size[axis] ) {
        if ( axis == 0 ) {
            float4 field = cloudVolumes[NonUniformResourceIndex( cloud.mTextures.x )].Load( int4( voxel, 0 ) );
            source = field.a > 0 ? float2( 0, 1e20 ) : float2( 1e20, 0 );
        }
        else {
            source = LoadBufferValue<float2>( parameters.mInput, index * sizeof( float2 ) );
        }
    }

    // Stage one line in shared memory before building its distance envelope. All lanes must
    // reach the barriers even when a lane lies outside this volume dimension.
    lineSource[lane] = source;
    lineResult[lane] = 1e20;
    GroupMemoryBarrierWithGroupSync();

    float spacing = voxelSize[axis];
    float squaredSpacing = spacing * spacing;
    for ( uint channel = 0; channel < kCloudDistanceChannelCount; ++channel ) {
        // Lane zero constructs the lower envelope of distance parabolas for this channel.
        // Feature indices and crossing boundaries describe which source cell is nearest along
        // each interval.
        if ( lane == 0 ) {
            int last = -1;
            for ( uint sample = 0; sample < size[axis]; ++sample ) {
                float value = lineSource[sample][channel];
                if ( value >= 1e20 ) {
                    continue;
                }
                float crossing = -1e20;
                while ( last >= 0 ) {
                    int previous = features[last];
                    crossing = ( value - lineSource[previous][channel] + squaredSpacing * ( float( sample ) * sample - float( previous ) * previous ) ) /
                               ( 2 * squaredSpacing * ( float( sample ) - previous ) );
                    if ( crossing > boundaries[last] ) {
                        break;
                    }
                    --last;
                }
                features[++last] = int( sample );
                boundaries[last] = last == 0 ? -1e20 : crossing;
                boundaries[last + 1] = 1e20;
            }
            featureCount = uint( last + 1 );
        }
        GroupMemoryBarrierWithGroupSync();

        // Each lane locates its interval in the shared envelope, then evaluates the selected
        // parabola. Repeating this independently for occupied/empty channels produces both
        // sides of the signed field.
        if ( lane < size[axis] && featureCount > 0 ) {
            uint lo = 0;
            uint hi = featureCount - 1;
            while ( lo < hi ) {
                uint middle = ( lo + hi ) / 2;
                if ( boundaries[middle + 1] < float( lane ) ) {
                    lo = middle + 1;
                }
                else {
                    hi = middle;
                }
            }
            int nearest = features[lo];
            float delta = float( lane ) - nearest;
            lineResult[lane][channel] = lineSource[nearest][channel] + delta * delta * squaredSpacing;
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if ( lane < size[axis] ) {
        StoreBufferValue<float2>( parameters.mOutput, index * sizeof( float2 ), lineResult[lane] );
    }
}
