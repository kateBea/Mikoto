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

#include "PathTracing_Common.slang"

// PBRT's error-bounded spawn points adapted to hardware traversal using NVIDIA's
// reconstruction and transform bounds. c1 is calibrated for NVIDIA RTX hardware.
// https://pbr-book.org/4ed/Shapes/Managing_Rounding_Error
// https://developer.nvidia.com/blog/solving-self-intersection-artifacts-in-directx-raytracing/
void ReconstructHit( float3 a, float3 b, float3 c, float2 barycentrics,
    float3x4 objectToWorld, float3x4 worldToObject,
    out float3 position, out float3 error, out float3 normal ) {
    precise float3 edge1 = b - a;
    precise float3 edge2 = c - a;
    precise float3 objectPosition = a + mad( barycentrics.x, edge1, barycentrics.y * edge2 );

    // Add translation last to keep its rounding error out of intermediate sums.
    precise float3 worldPosition;
    for ( uint axis = 0; axis < 3; ++axis ) {
        worldPosition[axis] = objectToWorld[axis][3] +
                              mad( objectToWorld[axis][0], objectPosition.x,
                                  mad( objectToWorld[axis][1], objectPosition.y, objectToWorld[axis][2] * objectPosition.z ) );
    }

    const float c0 = 5.9604644775390625e-8;
    const float c1 = 1.7881397695873602e-7;
    const float c2 = 1.1920931797249068e-7;
    float3 extents = abs( edge1 ) + abs( edge2 ) + abs( abs( edge1 ) - abs( edge2 ) );
    float extent = max( extents.x, max( extents.y, extents.z ) );
    float3 objectError = c0 * abs( a ) + c1 * extent;
    float3 translation = float3( objectToWorld[0][3], objectToWorld[1][3], objectToWorld[2][3] );
    float3 worldError = c1 * mul( abs( ( float3x3 )objectToWorld ), abs( objectPosition ) ) + c2 * abs( translation );
    objectError += c2 * mul( abs( worldToObject ), float4( abs( worldPosition ), 1.0 ) );

    position = worldPosition;
    error = worldError + mul( abs( ( float3x3 )objectToWorld ), objectError );
    float3 transformedNormal = mul( cross( edge1, edge2 ), ( float3x3 )worldToObject );
    normal = normalize( transformedNormal ) * ( determinant( ( float3x3 )objectToWorld ) < 0.0 ? -1.0 : 1.0 );
}

[shader( "closesthit" )]
void main( inout Hit payload, BuiltInTriangleIntersectionAttributes attributes ) {
    payload.Distance = RayTCurrent();

    // Each BLAS has local primitive IDs; the TLAS instance selects its index-buffer range.
    payload.Triangle = InstanceID() + PrimitiveIndex();
    payload.Barycentrics = attributes.barycentrics;

    uint3 indices = LoadBufferValue<uint3>( Parameters.Indices, payload.Triangle * 12 );
    float3 a = asfloat( LoadBufferValue<uint3>( Parameters.BuildPositions, indices.x * 12 ) );
    float3 b = asfloat( LoadBufferValue<uint3>( Parameters.BuildPositions, indices.y * 12 ) );
    float3 c = asfloat( LoadBufferValue<uint3>( Parameters.BuildPositions, indices.z * 12 ) );

    // Slang lowers these out arguments through inout temporaries. Reconstruct
    // locally so DXC never reads payload fields that closest hit only writes.
    // https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#payload-access-qualifiers
    float3 position = 0.0;
    float3 positionError = 0.0;
    float3 geometricNormal = 0.0;

    ReconstructHit( a, b, c, payload.Barycentrics, ObjectToWorld3x4(), WorldToObject3x4(),
        position, positionError, geometricNormal );

    payload.Position = position;
    payload.PositionError = positionError;
    payload.GeometricNormal = geometricNormal;
}
