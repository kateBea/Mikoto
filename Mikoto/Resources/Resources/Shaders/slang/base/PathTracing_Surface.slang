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

// Decode the glTF surface, then filter hardware intersections for visibility and shadows.

float3 Linear( float3 value ) {
    return select( value <= 0.04045, value / 12.92, pow( ( value + 0.055 ) / 1.055, 2.4 ) );
}

float4 Map( int index, float2 uv ) {
    if ( index < 0 ) {
        return float4( 1.0 );
    }
    return MaterialMap( uint( index ) ).SampleLevel( MaterialSampler(), uv, 0.0 );
}

float2 UV( float4 uv, int set ) {
    return set == 0 ? uv.xy : uv.zw;
}

Surface Evaluate( uint triangle, float2 barycentrics, bool visibilityOnly = false, float3 toEye = float3( 0 ) ) {
    uint3 indices = LoadTriangleIndices( triangle );
    VertexRecord a = LoadBufferValue<VertexRecord>( Parameters.Vertices, indices.x * sizeof( VertexRecord ) );
    VertexRecord b = LoadBufferValue<VertexRecord>( Parameters.Vertices, indices.y * sizeof( VertexRecord ) );
    VertexRecord c = LoadBufferValue<VertexRecord>( Parameters.Vertices, indices.z * sizeof( VertexRecord ) );
    MaterialRecord material = LoadMaterialRecord( uint( a.Position.w ) );
    float3 weights = float3( 1.0 - barycentrics.x - barycentrics.y, barycentrics );

    Surface s = ( Surface )0;
    s.Position = a.Position.xyz * weights.x + b.Position.xyz * weights.y + c.Position.xyz * weights.z;
    // PBRT's gamma(7) bound for barycentric interpolation of sampled triangles.
    // Hardware hits replace this with the reconstruction bounds from closest hit.
    // https://pbr-book.org/4ed/Shapes/Triangle_Meshes
    s.PositionError = 4.172327e-7 * ( abs( weights.x * a.Position.xyz ) +
                                        abs( weights.y * b.Position.xyz ) + abs( weights.z * c.Position.xyz ) );
    s.PositionError += dot( abs( weights ), float3( a.Normal.w, b.Normal.w, c.Normal.w ) );
    s.GeometricNormal = normalize( cross( b.Position.xyz - a.Position.xyz, c.Position.xyz - a.Position.xyz ) );
    s.Alpha = 1.0;
    s.AlphaMode = uint( material.Values.z );
    s.Cutoff = material.Values.w;
    s.DoubleSided = material.Extra.y != 0.0;
    s.Model = material.Properties.x;
    s.Material = uint( a.Position.w );
    s.Profile = material.Properties.z;
    s.ThinWalled = material.Properties.y != 0;
    s.IOR = material.Optical.x;
    s.Transmission = material.Optical.y;
    s.Thickness = material.Optical.z;
    s.Absorption = material.Absorption.xyz;
    s.Scattering = material.Scattering.xyz;
    s.Anisotropy = material.Absorption.w;
    s.DoubleSided = s.DoubleSided || s.Model == MATERIAL_SUBSURFACE || s.Model == MATERIAL_DIELECTRIC;
    s.AreaLight = material.Emission.w != 0.0;
    s.EmissionTwoSided = s.AreaLight ? material.Emission.w > 1.0 : s.DoubleSided;
    s.EmissionOnly = material.Extra.z != 0.0;

    // Opaque visibility needs sidedness only; alpha coverage still needs textures.
    if ( visibilityOnly && s.AlphaMode == 0 ) {
        return s;
    }

    float4 uvA = a.UV * material.UVTransform.xyxy + material.UVTransform.zwzw;
    float4 uvB = b.UV * material.UVTransform.xyxy + material.UVTransform.zwzw;
    float4 uvC = c.UV * material.UVTransform.xyxy + material.UVTransform.zwzw;
    float4 uv = uvA * weights.x + uvB * weights.y + uvC * weights.z;

    // Use the height map's UV set to build its tangent frame. Parallax shifts
    // the UVs used for shading while the geometric intersection stays fixed.
    float3 heightDu = 0.0;
    float3 heightDv = 0.0;
    bool heightFrame = HeightTangents( b.Position.xyz - a.Position.xyz, c.Position.xyz - a.Position.xyz,
        UV( uvB - uvA, material.HeightInfo.y ), UV( uvC - uvA, material.HeightInfo.y ), heightDu, heightDv );
    float2 heightUV = UV( uv, material.HeightInfo.y );

    static const uint parallaxMode = 2u;
    if ( !visibilityOnly && material.HeightInfo.x >= 0 && uint( material.HeightMapping.z ) == parallaxMode && heightFrame && dot( toEye, toEye ) > 0.0 ) {
        heightUV = HeightParallax( MaterialMap( uint( material.HeightInfo.x ) ), MaterialSampler(), heightUV,
            normalize( toEye ), s.GeometricNormal, heightDu, heightDv, material.HeightMapping.x,
            material.HeightMapping.y, uint( material.HeightMapping.w ) );

        if ( material.HeightInfo.y == 0 ) {
            uv.xy = heightUV;
        }
        else {
            uv.zw = heightUV;
        }
    }

    float4 color = a.Color * weights.x + b.Color * weights.y + c.Color * weights.z;
    float4 texel = Map( material.Maps.x, UV( uv, material.UVSets.x ) );
    float4 base = material.Base * float4( Linear( texel.rgb ), texel.a ) * color;
    s.Alpha = saturate( base.a );
    if ( visibilityOnly ) {
        return s;
    }

    float3 normal = a.Normal.xyz * weights.x + b.Normal.xyz * weights.y + c.Normal.xyz * weights.z;
    s.Normal = dot( normal, normal ) > 1e-12 ? normalize( normal ) : s.GeometricNormal;
    if ( dot( s.Normal, s.GeometricNormal ) < 0.0 ) {
        s.Normal = -s.Normal;
    }

    if ( material.HeightInfo.x >= 0 && material.HeightMapping.z != 0.0 && heightFrame ) {
        s.Normal = HeightBumpNormal( MaterialMap( uint( material.HeightInfo.x ) ), MaterialSampler(),
            heightUV, s.Normal, heightDu, heightDv, material.HeightMapping.x );
    }

    // Reconstruct the texture's tangent frame while rejecting degenerate UVs.
    if ( material.Maps.w >= 0 ) {
        float2 d1 = UV( uvB - uvA, material.UVSets.w );
        float2 d2 = UV( uvC - uvA, material.UVSets.w );
        float determinant = d1.x * d2.y - d1.y * d2.x;
        if ( abs( determinant ) > 1e-10 ) {
            float3 tangent = ( ( b.Position.xyz - a.Position.xyz ) * d2.y -
                                 ( c.Position.xyz - a.Position.xyz ) * d1.y ) /
                             determinant;
            tangent -= s.Normal * dot( s.Normal, tangent );
            if ( dot( tangent, tangent ) > 1e-12 ) {
                tangent = normalize( tangent );
                float3 bitangent = cross( s.Normal, tangent ) * sign( determinant );
                float3 mapped = Map( material.Maps.w, UV( uv, material.UVSets.w ) ).xyz * 2.0 - 1.0;
                mapped.xy *= material.Extra.x;
                normal = tangent * mapped.x + bitangent * mapped.y + s.Normal * mapped.z;
                if ( dot( normal, normal ) > 1e-12 ) {
                    s.Normal = normalize( normal );
                }
            }
        }
    }

    // Preserve glTF's RGB metallic/roughness and legacy specular/glossiness inputs.
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
    float4 mr = Map( material.Maps.y, UV( uv, material.UVSets.y ) );
    s.Base = max( base.rgb, 0.0 );
    s.Emission = max( material.Emission.rgb * Linear( Map( material.Maps.z, UV( uv, material.UVSets.z ) ).rgb ), 0.0 );
    s.Metallic = saturate( material.Values.x * mr.b );
    s.Roughness = clamp( material.Values.y * mr.g, 0.0, 1.0 );
    s.F0 = lerp( float3( 0.04 ), s.Base, s.Metallic );
    if ( material.Extra.w > 0.0 ) {
        s.F0 = material.Specular.rgb * Linear( mr.rgb );
        s.Roughness = clamp( 1.0 - ( 1.0 - material.Values.y ) * mr.a, 0.025, 1.0 );
    }
    if ( s.Model == MATERIAL_CONDUCTOR ) {
        s.Metallic = 1.0;
    }
    if ( s.Model == MATERIAL_UNLIT ) {
        s.Emission += s.Base;
        s.EmissionOnly = true;
    }
    return s;
}

float3 Offset( Surface s, float3 direction ) {
    return OffsetRayOrigin( s.Position, s.PositionError, s.GeometricNormal, direction );
}

bool Intersect( float3 origin, float3 direction, float maximum, inout uint random, out Surface surface, out Hit hit,
    bool opaqueOnly = false, bool visibilityOnly = false ) {
    RayDesc ray = { origin, 0.0, direction, maximum };
    surface = ( Surface )0;
    hit = ( Hit )0;

    // Engine adapter: retry nearest hits to implement glTF sidedness and coverage
    // on opaque BLASes. Advancing one representable t avoids skipping thin layers.
    static const uint maxCoverageLayers = 128u;
    for ( uint layer = 0; layer < maxCoverageLayers; ++layer ) {
        hit.Distance = RAY_MISS_DISTANCE;
        TraceRay( LoadRayScene( Parameters.SceneIndex ), RAY_FLAG_NONE, RAY_ALL_INSTANCES, 0, 1, 0, ray, hit );
        if ( hit.Distance < 0.0 ) {
            return false;
        }

        surface = Evaluate( hit.Triangle, hit.Barycentrics, visibilityOnly, -direction );
        surface.Position = hit.Position;
        surface.PositionError = hit.PositionError;
        surface.GeometricNormal = hit.GeometricNormal;
        bool back = dot( surface.GeometricNormal, direction ) > 0.0;
        surface.FrontFace = !back;
        bool translucent = surface.AlphaMode == 2 && surface.Alpha < 1.0 - 1e-6;
        bool reject = ( !surface.DoubleSided && !surface.AreaLight && back ) || ( opaqueOnly && translucent ) ||
                      ( surface.AlphaMode == 1 && surface.Alpha < surface.Cutoff ) ||
                      ( translucent && Random( random ) >= surface.Alpha );
        if ( !reject ) {
            // PBRT DiffuseAreaLight::L: the unlit back remains an opaque surface.
            // https://pbr-book.org/4ed/Light_Sources/Area_Lights
            if ( surface.AreaLight && !surface.EmissionTwoSided && back ) {
                surface.Emission = float3( 0.0 );
            }

            if ( dot( surface.Normal, surface.GeometricNormal ) < 0.0 ) {
                surface.Normal = -surface.Normal;
            }
            if ( back ) {
                surface.Normal = -surface.Normal;
                surface.GeometricNormal = -surface.GeometricNormal;
            }
            return true;
        }

        ray.TMin = NextFloat( hit.Distance, true );
        if ( ray.TMin >= maximum ) {
            return false;
        }
    }
    return false;
}

bool Visible( Surface s, float3 direction, float maximum, inout uint random ) {
    Surface blocker;
    Hit hit;
    return !Intersect( Offset( s, direction ), direction, maximum, random, blocker, hit, false, true );
}

bool VisibleTo( Surface s, float3 target, inout uint random ) {
    float3 origin = Offset( s, target - s.Position );
    float3 delta = target - origin;
    float distance = length( delta );
    if ( distance <= 0.0 ) {
        return false;
    }

    Surface blocker;
    Hit hit;
    return !Intersect( origin, delta / distance, NextFloat( distance, false ), random, blocker, hit, false, true );
}

bool VisibleTo( Surface s, Surface light, inout uint random ) {
    // PBRT's SpawnRayTo offsets both ends before constructing a connection ray.
    // https://pbr-book.org/4ed/Shapes/Managing_Rounding_Error
    return VisibleTo( s, Offset( light, s.Position - light.Position ), random );
}
