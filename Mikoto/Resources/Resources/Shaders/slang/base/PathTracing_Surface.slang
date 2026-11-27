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

// Reconstruct and evaluate hit materials, then launch visibility/intersection queries. Alpha
// coverage and ray-origin offsets connect triangle hits to surface transport.

#pragma once

#include "Checkerboard_Helpers.slang"

// Decode the glTF surface, then filter hardware intersections for visibility and shadows.

float3 Linear( float3 value ) {
    return select( value <= 0.04045, value / 12.92, pow( ( value + 0.055 ) / 1.055, 2.4 ) );
}

float4 Map( int index, float2 uv, float2 dx = float2( 0 ), float2 dy = float2( 0 ) ) {
    if ( index < 0 ) {
        return float4( 1.0 );
    }
    // Ray stages have no implicit raster derivatives. Supply the projected
    // footprint explicitly so both mip selection and anisotropy can operate.
    return MaterialMap( uint( index ) ).SampleGrad( MaterialSampler(), uv, dx, dy );
}

float2 UV( float4 uv, int set ) {
    return set == 0 ? uv.xy : uv.zw;
}

// Resolve hit geometry and material textures into transport coefficients. Texture coordinates,
// normals and material model are evaluated once before sampling the next event.
Surface Evaluate( uint triangle, float2 barycentrics, bool visibilityOnly = false, float3 toEye = float3( 0 ) ) {
    uint3 indices = LoadTriangleIndices( triangle );
    VertexRecord a = LoadBufferValue<VertexRecord>( Parameters.mVertices, indices.x * sizeof( VertexRecord ) );
    VertexRecord b = LoadBufferValue<VertexRecord>( Parameters.mVertices, indices.y * sizeof( VertexRecord ) );
    VertexRecord c = LoadBufferValue<VertexRecord>( Parameters.mVertices, indices.z * sizeof( VertexRecord ) );
    MaterialRecord material = LoadMaterialRecord( uint( a.mPosition.w ) );
    float3 weights = float3( 1.0 - barycentrics.x - barycentrics.y, barycentrics );

    Surface s = ( Surface )0;
    s.mPosition = a.mPosition.xyz * weights.x + b.mPosition.xyz * weights.y + c.mPosition.xyz * weights.z;
    // PBRT's gamma(7) bound for barycentric interpolation of sampled triangles.
    // Hardware hits replace this with the reconstruction bounds from closest hit.
    // https://pbr-book.org/4ed/Shapes/Triangle_Meshes
    s.mPositionError = 4.172327e-7 * ( abs( weights.x * a.mPosition.xyz ) +
                                        abs( weights.y * b.mPosition.xyz ) + abs( weights.z * c.mPosition.xyz ) );
    s.mPositionError += dot( abs( weights ), float3( a.mNormal.w, b.mNormal.w, c.mNormal.w ) );
    s.mGeometricNormal = normalize( cross( b.mPosition.xyz - a.mPosition.xyz, c.mPosition.xyz - a.mPosition.xyz ) );
    s.mAlpha = 1.0;
    s.mAlphaMode = uint( material.mValues.z );
    s.mCutoff = material.mValues.w;
    s.mDoubleSided = material.mExtra.y != 0.0;
    s.mModel = material.mProperties.x;
    s.mMaterial = uint( a.mPosition.w );
    s.mProfile = material.mProperties.z;
    s.mThinWalled = material.mProperties.y != 0;
    s.mIOR = material.mOptical.x;
    s.mTransmission = material.mOptical.y;
    s.mThickness = material.mOptical.z;
    s.mAbsorption = material.mAbsorption.xyz;
    s.mScattering = material.mScattering.xyz;
    s.mAnisotropy = material.mAbsorption.w;
    s.mDoubleSided = s.mDoubleSided || s.mModel == MATERIAL_SUBSURFACE || s.mModel == MATERIAL_DIELECTRIC;
    s.mAreaLight = material.mEmission.w != 0.0;
    s.mEmissionTwoSided = s.mAreaLight ? material.mEmission.w > 1.0 : s.mDoubleSided;
    s.mEmissionOnly = material.mExtra.z != 0.0;

    // Opaque visibility needs sidedness only; alpha coverage still needs textures.
    if ( visibilityOnly && s.mAlphaMode == 0 ) {
        return s;
    }

    float4 uvA = a.mUV * material.mUVTransform.xyxy + material.mUVTransform.zwzw;
    float4 uvB = b.mUV * material.mUVTransform.xyxy + material.mUVTransform.zwzw;
    float4 uvC = c.mUV * material.mUVTransform.xyxy + material.mUVTransform.zwzw;
    float4 uv = uvA * weights.x + uvB * weights.y + uvC * weights.z;

    float3 worldDx = 0;
    float3 worldDy = 0;
    float4 uvDx = 0;
    float4 uvDy = 0;
    if ( any( material.mMaps >= 0 ) || material.mCheckerboardColorA.w != 0 ) {
        float3 cameraOrigin;
        float3 cameraDirection;
        float3 originX;
        float3 directionX;
        float3 originY;
        float3 directionY;
        uint2 pixel = DispatchRaysIndex().xy;
        GenerateCameraRay( pixel, float2( 0.5 ), cameraOrigin, cameraDirection );
        GenerateCameraRay( pixel, float2( 1.5, 0.5 ), originX, directionX );
        GenerateCameraRay( pixel, float2( 0.5, 1.5 ), originY, directionY );
        float3 incoming = dot( toEye, toEye ) > 0 ? -normalize( toEye ) : cameraDirection;
        float distance = length( s.mPosition - cameraOrigin );
        worldDx = originX - cameraOrigin + distance * ( directionX - cameraDirection );
        worldDy = originY - cameraOrigin + distance * ( directionY - cameraDirection );
        float cosine = dot( incoming, s.mGeometricNormal );
        cosine = ( cosine < 0 ? -1 : 1 ) * max( abs( cosine ), 1e-3 );
        worldDx -= incoming * dot( worldDx, s.mGeometricNormal ) / cosine;
        worldDy -= incoming * dot( worldDy, s.mGeometricNormal ) / cosine;

        // Project the pixel's world-space footprint into both authored UV sets.
        // At secondary hits this is a conservative camera-cone approximation.
        float3 edge1 = b.mPosition.xyz - a.mPosition.xyz;
        float3 edge2 = c.mPosition.xyz - a.mPosition.xyz;
        float area = max( length( cross( edge1, edge2 ) ), 1e-12 );
        float3 reciprocal1 = cross( edge2, s.mGeometricNormal ) / area;
        float3 reciprocal2 = cross( s.mGeometricNormal, edge1 ) / area;
        uvDx = ( uvB - uvA ) * dot( reciprocal1, worldDx ) + ( uvC - uvA ) * dot( reciprocal2, worldDx );
        uvDy = ( uvB - uvA ) * dot( reciprocal1, worldDy ) + ( uvC - uvA ) * dot( reciprocal2, worldDy );
    }


    // Use the height map's UV set to build its tangent frame. Parallax shifts
    // the UVs used for shading while the geometric intersection stays fixed.
    float3 heightDu = 0.0;
    float3 heightDv = 0.0;
    bool heightFrame = HeightTangents( b.mPosition.xyz - a.mPosition.xyz, c.mPosition.xyz - a.mPosition.xyz,
        UV( uvB - uvA, material.mHeightInfo.y ), UV( uvC - uvA, material.mHeightInfo.y ), heightDu, heightDv );
    float2 heightUV = UV( uv, material.mHeightInfo.y );

    static const uint parallaxMode = 2u;
    if ( !visibilityOnly && material.mHeightInfo.x >= 0 && uint( material.mHeightMapping.z ) == parallaxMode && heightFrame && dot( toEye, toEye ) > 0.0 ) {
        heightUV = HeightParallax( MaterialMap( uint( material.mHeightInfo.x ) ), MaterialSampler(), heightUV,
            normalize( toEye ), s.mGeometricNormal, heightDu, heightDv, material.mHeightMapping.x,
            material.mHeightMapping.y, uint( material.mHeightMapping.w ) );

        if ( material.mHeightInfo.y == 0 ) {
            uv.xy = heightUV;
        }
        else {
            uv.zw = heightUV;
        }
    }

    float4 color = a.mColor * weights.x + b.mColor * weights.y + c.mColor * weights.z;
    float4 texel = Map( material.mMaps.x, UV( uv, material.mUVSets.x ), UV( uvDx, material.mUVSets.x ), UV( uvDy, material.mUVSets.x ) );
    float4 base = material.mBase * float4( Linear( texel.rgb ), texel.a ) * color;

    if ( material.mCheckerboardColorA.w != 0.0 ) {
        // Integrate cells over the pixel instead of relying on a few random
        // point samples to resolve an arbitrarily high-frequency checkerboard.
        base.rgb *= CheckerboardColor( s.mPosition, abs( worldDx ) + abs( worldDy ), s.mGeometricNormal, material.mCheckerboardColorA.rgb,
            material.mCheckerboardColorB, material.mCheckerboardOffset.xyz );
    }
    s.mAlpha = saturate( base.a );
    if ( visibilityOnly ) {
        return s;
    }

    float3 normal = a.mNormal.xyz * weights.x + b.mNormal.xyz * weights.y + c.mNormal.xyz * weights.z;
    s.mNormal = dot( normal, normal ) > 1e-12 ? normalize( normal ) : s.mGeometricNormal;
    if ( dot( s.mNormal, s.mGeometricNormal ) < 0.0 ) {
        s.mNormal = -s.mNormal;
    }

    if ( material.mHeightInfo.x >= 0 && material.mHeightMapping.z != 0.0 && heightFrame ) {
        s.mNormal = HeightBumpNormal( MaterialMap( uint( material.mHeightInfo.x ) ), MaterialSampler(),
            heightUV, s.mNormal, heightDu, heightDv, material.mHeightMapping.x );
    }

    // Reconstruct the texture's tangent frame while rejecting degenerate UVs.
    if ( material.mMaps.w >= 0 ) {
        float2 d1 = UV( uvB - uvA, material.mUVSets.w );
        float2 d2 = UV( uvC - uvA, material.mUVSets.w );
        float determinant = d1.x * d2.y - d1.y * d2.x;
        if ( abs( determinant ) > 1e-10 ) {
            float3 tangent = ( ( b.mPosition.xyz - a.mPosition.xyz ) * d2.y -
                                 ( c.mPosition.xyz - a.mPosition.xyz ) * d1.y ) /
                             determinant;
            tangent -= s.mNormal * dot( s.mNormal, tangent );
            if ( dot( tangent, tangent ) > 1e-12 ) {
                tangent = normalize( tangent );
                float3 bitangent = cross( s.mNormal, tangent ) * sign( determinant );
                float3 mapped = Map( material.mMaps.w, UV( uv, material.mUVSets.w ), UV( uvDx, material.mUVSets.w ), UV( uvDy, material.mUVSets.w ) ).xyz * 2.0 - 1.0;
                mapped.xy *= material.mExtra.x;
                normal = tangent * mapped.x + bitangent * mapped.y + s.mNormal * mapped.z;
                if ( dot( normal, normal ) > 1e-12 ) {
                    s.mNormal = normalize( normal );
                }
            }
        }
    }

    // Preserve glTF's RGB metallic/roughness and legacy specular/glossiness inputs.
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
    float4 mr = Map( material.mMaps.y, UV( uv, material.mUVSets.y ), UV( uvDx, material.mUVSets.y ), UV( uvDy, material.mUVSets.y ) );
    s.mBase = max( base.rgb, 0.0 );
    s.mEmission = max( material.mEmission.rgb * Linear( Map( material.mMaps.z, UV( uv, material.mUVSets.z ), UV( uvDx, material.mUVSets.z ), UV( uvDy, material.mUVSets.z ) ).rgb ), 0.0 );
    s.mMetallic = saturate( material.mValues.x * mr.b );
    s.mRoughness = clamp( material.mValues.y * mr.g, 0.0, 1.0 );
    s.mF0 = lerp( float3( 0.04 ), s.mBase, s.mMetallic );
    if ( material.mExtra.w > 0.0 ) {
        s.mF0 = material.mSpecular.rgb * Linear( mr.rgb );
        s.mRoughness = clamp( 1.0 - ( 1.0 - material.mValues.y ) * mr.a, 0.025, 1.0 );
    }
    if ( s.mModel == MATERIAL_CONDUCTOR ) {
        s.mMetallic = 1.0;
    }
    if ( s.mModel == MATERIAL_UNLIT ) {
        s.mEmission += s.mBase;
        s.mEmissionOnly = true;
    }
    return s;
}

// Move a secondary-ray origin to the side of the geometric surface selected by its outgoing
// direction. This reduces self-intersection without changing the shading normal.
float3 Offset( Surface s, float3 direction ) {
    return OffsetRayOrigin( s.mPosition, s.mPositionError, s.mGeometricNormal, direction );
}

// Trace and evaluate a usable surface, applying the material coverage rules while traversing
// candidate hits.
bool Intersect( float3 origin, float3 direction, float maximum, inout uint random, out Surface surface, out Hit hit,
    bool opaqueOnly = false, bool visibilityOnly = false ) {
    RayDesc ray = { origin, 0.0, direction, maximum };
    surface = ( Surface )0;
    hit = ( Hit )0;

    // Engine adapter: retry nearest hits to implement glTF sidedness and coverage
    // on opaque BLASes. Advancing one representable t avoids skipping thin layers.
    static const uint maxCoverageLayers = 128u;
    for ( uint layer = 0; layer < maxCoverageLayers; ++layer ) {
        hit.mDistance = RAY_MISS_DISTANCE;
        TraceRay( LoadRayScene( Parameters.mSceneIndex ), RAY_FLAG_NONE, RAY_ALL_INSTANCES, 0, 1, 0, ray, hit );
        if ( hit.mDistance < 0.0 ) {
            return false;
        }

        surface = Evaluate( hit.mTriangle, hit.mBarycentrics, visibilityOnly, -direction );
        surface.mPosition = hit.mPosition;
        surface.mPositionError = hit.mPositionError;
        surface.mGeometricNormal = hit.mGeometricNormal;
        bool back = dot( surface.mGeometricNormal, direction ) > 0.0;
        surface.mFrontFace = !back;
        bool translucent = surface.mAlphaMode == 2 && surface.mAlpha < 1.0 - 1e-6;
        bool reject = ( !surface.mDoubleSided && !surface.mAreaLight && back ) || ( opaqueOnly && translucent ) ||
                      ( surface.mAlphaMode == 1 && surface.mAlpha < surface.mCutoff ) ||
                      ( translucent && Random( random ) >= surface.mAlpha );
        if ( !reject ) {
            // PBRT DiffuseAreaLight::L: the unlit back remains an opaque surface.
            // https://pbr-book.org/4ed/Light_Sources/Area_Lights
            if ( surface.mAreaLight && !surface.mEmissionTwoSided && back ) {
                surface.mEmission = float3( 0.0 );
            }

            if ( dot( surface.mNormal, surface.mGeometricNormal ) < 0.0 ) {
                surface.mNormal = -surface.mNormal;
            }
            if ( back ) {
                surface.mNormal = -surface.mNormal;
                surface.mGeometricNormal = -surface.mGeometricNormal;
            }
            return true;
        }

        ray.TMin = NextFloat( hit.mDistance, true );
        if ( ray.TMin >= maximum ) {
            return false;
        }
    }
    return false;
}

// Check whether a finite surface connection is unobstructed. The endpoints and origin offset
// use the same geometry convention as transport continuation.
bool Visible( Surface s, float3 direction, float maximum, inout uint random ) {
    Surface blocker;
    Hit hit;
    return !Intersect( Offset( s, direction ), direction, maximum, random, blocker, hit, false, true );
}

bool VisibleTo( Surface s, float3 target, inout uint random ) {
    float3 origin = Offset( s, target - s.mPosition );
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
    return VisibleTo( s, Offset( light, s.mPosition - light.mPosition ), random );
}
