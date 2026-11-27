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

// Deform foliage from stable world-space wind and material controls. Position and normal
// helpers keep geometry and shading aligned across depth, shadow and color passes.

#pragma once

// Main bend, branch waves and leaf flutter share the same root and instance
// phase. This position-only path also serves depth, picking and shadow passes.
// References: Guerrilla, Between Tech and Art (GDC 2018); GPU Gems 3, chapter 6.
// https://media.gdcvault.com/gdc2018/presentations/gilbert_sanders_between_tech_and.pdf
// https://developer.nvidia.com/gpugems/gpugems3/part-i-geometry/chapter-6-gpu-generated-procedural-wind-animations-trees
float3 DeformFoliagePosition( float3 position, MeshGeometryInfo mesh ) {
    if ( mesh.mFoliageMotion.x <= 0 ) {
        return position;
    }
    float3 root = mul( mesh.mTransform, float4( 0, mesh.mFoliageShape.x, 0, 1 ) ).xyz;
    float3 axis = mul( mesh.mTransform, float4( 0, 1, 0, 0 ) ).xyz;
    float scale = max( length( axis ), 1e-6 );
    float3 up = axis / scale;
    float3 delta = position - root;
    float elevation = dot( delta, up );
    if ( elevation <= 0 ) {
        return position;
    }
    float3 wind = mesh.mFoliageWind.xyz - up * dot( mesh.mFoliageWind.xyz, up );
    float speed = length( wind );
    if ( speed < 1e-5 ) {
        return position;
    }
    float3 direction = wind / speed;
    float3 side = cross( up, direction );
    float height = mesh.mFoliageMotion.x * scale;
    float weight = saturate( elevation / height );
    float strength = saturate( speed / 12 );
    float time = mesh.mFoliageWind.w * mesh.mFoliageShape.y;
    float phase = dot( root, float3( .173, .137, .291 ) );
    float mainWave = 1 + .2 * sin( time * .73 + phase ) + .1 * sin( time * 1.31 + phase * 1.7 );
    float bend = height * mesh.mFoliageMotion.y * weight * weight * strength;
    float3 radial = delta - up * elevation;
    float branchWeight = weight * weight * saturate( length( radial ) / max( height * .2, .05 ) );
    float branchPhase = dot( radial, direction * .7 + side * .5 ) + phase;
    float branch = sin( time * 2.17 + branchPhase ) * mesh.mFoliageMotion.z * scale * branchWeight * strength;
    float flutter = ( sin( time * 7.3 + branchPhase * 2.1 ) + .35 * sin( time * 11.7 + branchPhase * 3.7 ) ) *
                    mesh.mFoliageMotion.w * scale * weight * strength;
    precise float3 result = position + direction * ( bend * mainWave + branch ) +
                            side * ( bend * .15 * sin( time * .91 + phase ) + flutter ) -
                            up * ( bend * bend / max( 2 * elevation, .01 ) );
    return result;
}

float3 DeformFoliageNormal( float3 rest, float3 deformed, float3 normal, MeshGeometryInfo mesh ) {
    float3 reference = abs( normal.y ) < .9 ? float3( 0, 1, 0 ) : float3( 1, 0, 0 );
    float3 tangent = normalize( cross( reference, normal ) );
    float3 bitangent = cross( normal, tangent );
    float epsilon = .01;
    float3 a = DeformFoliagePosition( rest + tangent * epsilon, mesh ) - deformed;
    float3 b = DeformFoliagePosition( rest + bitangent * epsilon, mesh ) - deformed;
    float3 result = cross( a, b );
    return dot( result, result ) > 1e-12 ? normalize( result ) : normal;
}
