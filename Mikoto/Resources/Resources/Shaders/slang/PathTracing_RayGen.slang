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

#include "base/HeightMapping_Helpers.slang"
#include "PathTracing_Common.slang"
#include "base/RayTracing_Bindings.slang"

// Transport and sampling are adapted from PBRT v4, in RGB for Mikoto's glTF materials.
// PBRT-derived portions: Copyright (c) 1998-2020 Matt Pharr, Wenzel Jakob, Greg Humphreys.
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/cpu/integrators.cpp
// Atmosphere LUTs, material decoding, hybrid outputs and editor overlays are engine adapters.

// Keep transport helpers inlinable. Forced noinline calls with large Surface and
// medium records made even ordinary PBR scenes much slower on the GPU.
// https://developer.nvidia.com/blog/best-practices-for-using-nvidia-rtx-ray-tracing-updated/

// Same shared-table slots as ResourceBindings.hh. Ray pipelines bind that
// layout at set 0; D3D12 accesses the device's shared descriptor heaps directly.
#if defined( MKT_SUPPORT_D3D12 )
ResourceArray<Texture2D<float4>> RayTextures;
ResourceArray<RWTexture2D<float4>> RayImages;
SamplerArray RaySamplers;
#else
[[vk::binding( 3, 0 )]] Texture2D<float4> RayTextures[];
[[vk::binding( 4, 0 )]] RWTexture2D<float4> RayImages[];
[[vk::binding( 2, 0 )]] SamplerState RaySamplers[];
#endif

// Materials already contain the graph's descriptor indices.
Texture2D<float4> MaterialMap( uint index ) {
    return RayTextures[NonUniformResourceIndex( index )];
}
Texture2D<float4> AtmosphereMap( uint index ) {
    return RayTextures[NonUniformResourceIndex( Parameters.AtmosphereTextures[index] )];
}
#define Output            RayImages[Parameters.Output]
#define History           RayTextures[Parameters.History]
#define Auxiliary         RayImages[Parameters.Auxiliary]
#define OcclusionOutput   RayImages[Parameters.OcclusionOutput]
#define OcclusionHistory  RayTextures[Parameters.OcclusionHistory]
#define MaterialSampler   RaySamplers[Parameters.MaterialSampler]
#define AtmosphereSampler RaySamplers[Parameters.AtmosphereSampler]

#define MKT_ATMOSPHERE_RAY_TRACING
#include "Atmosphere_Common.slang"

struct ViewEffectsRecord {
    AtmosphereData Atmosphere;
    float4 Control, Background;
    float4 LensRight, LensUp, LensForward;
};

ViewEffectsRecord Effects() {
    return LoadBufferValue<ViewEffectsRecord>( Parameters.ViewEffects, 0 );
}

static const float PI = 3.141592653589793;
static const float INV_PI = 1.0 / PI;
static const float INV_FOUR_PI = 1.0 / ( 4.0 * PI );
static const uint DIFFUSE = 1u;
static const uint SPECULAR = 2u;
static const uint ALL_LOBES = DIFFUSE | SPECULAR;
// Serialized ShadingModel IDs; preserve the original material enum values.
static const uint MATERIAL_SUBSURFACE = 5u;
static const uint MATERIAL_DIELECTRIC = 6u;
static const uint MATERIAL_CONDUCTOR = 7u;
static const uint MATERIAL_UNLIT = 8u;
static const uint MATERIAL_MEDIUM = 0xfffffffeu; // An interaction inside a volume, not a material model.

// Matches RayTracingModule::Settings::kMaxPathSamplesPerFrame.
static const uint SAMPLE_STREAMS_PER_FRAME = 1024u;

struct Surface {
    float3 Position, PositionError;
    float3 Normal, GeometricNormal;
    float3 Base, Emission, F0;
    float Roughness, Metallic, Alpha, Cutoff;
    uint AlphaMode;
    bool DoubleSided, AreaLight, EmissionTwoSided, EmissionOnly;
    uint Model, Material, Profile;
    float IOR, Transmission, Thickness, Anisotropy;
    float3 Absorption, Scattering;
    bool FrontFace, ThinWalled, SubsurfaceExit;
};

struct BSDFSample {
    float3 Direction, Value;
    float PDF;
    float Eta;
    bool Delta, Transmitted;
};

struct PreviousVertex {
    float3 Position;
    float BSDFPDF;
    uint LightCount;
};

struct MediumStack {
    uint Materials[8];
    uint Count;
};

struct MediumState {
    float3 Absorption, Scattering;
    float IOR, Anisotropy;
};

SceneRecord SceneData() {
    return LoadBufferValue<SceneRecord>( Parameters.Data, 0 );
}

float MaxComponent( float3 value ) {
    return max( value.x, max( value.y, value.z ) );
}

// PCG's GPU permutation; each pixel sample starts its own deterministic stream.
// https://www.jcgt.org/published/0009/03/02/
// https://pbr-book.org/4ed/Sampling_and_Reconstruction/Independent_Sampler
float Random( inout uint state ) {
    state = state * 747796405u + 2891336453u;
    uint word = ( ( state >> ( ( state >> 28u ) + 4u ) ) ^ state ) * 277803737u;
    word = ( word >> 22u ) ^ word;
    return float( word >> 8u ) * ( 1.0 / 16777216.0 );
}

float2 Random2D( inout uint state ) {
    float x = Random( state );
    float y = Random( state );
    return float2( x, y );
}

// PBRT's concentric mapping preserves uniform aperture area.
// https://pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions
float2 SampleLensDisk( float2 sample ) {
    float2 offset = 2.0 * sample - 1.0;
    if ( all( offset == 0.0 ) ) {
        return float2( 0.0 );
    }

    float radius, angle;
    if ( abs( offset.x ) > abs( offset.y ) ) {
        radius = offset.x;
        angle = ( PI * 0.25 ) * ( offset.y / offset.x );
    }
    else {
        radius = offset.y;
        angle = ( PI * 0.5 ) - ( PI * 0.25 ) * ( offset.x / offset.y );
    }
    return radius * float2( cos( angle ), sin( angle ) );
}

void ApplyDepthOfField( float4 right, float4 up, float3 forward, float2 sample,
    inout float3 origin, inout float3 direction ) {
    // Intersect the pinhole ray with a plane perpendicular to camera forward.
    // https://pbr-book.org/4ed/Cameras_and_Film/Projective_Camera_Models
    float3 focus = origin + direction * ( up.w / max( dot( direction, forward ), 1e-6 ) );
    float2 lens = right.w * SampleLensDisk( sample );
    origin += right.xyz * lens.x + up.xyz * lens.y;
    direction = normalize( focus - origin );
}

float3 Linear( float3 value ) {
    return select( value <= 0.04045, value / 12.92, pow( ( value + 0.055 ) / 1.055, 2.4 ) );
}

float4 Map( int index, float2 uv ) {
    if ( index < 0 ) {
        return float4( 1.0 );
    }
    return MaterialMap( uint( index ) ).SampleLevel( MaterialSampler, uv, 0.0 );
}

float2 UV( float4 uv, int set ) {
    return set == 0 ? uv.xy : uv.zw;
}

Surface Evaluate( uint triangle, float2 barycentrics, bool visibilityOnly = false, float3 toEye = float3(0) ) {
    uint3 indices = LoadBufferValue<uint3>( Parameters.Indices, triangle * 12 );
    VertexRecord a = LoadBufferValue<VertexRecord>( Parameters.Vertices, indices.x * 64 );
    VertexRecord b = LoadBufferValue<VertexRecord>( Parameters.Vertices, indices.y * 64 );
    VertexRecord c = LoadBufferValue<VertexRecord>( Parameters.Vertices, indices.z * 64 );
    MaterialRecord material = LoadBufferValue<MaterialRecord>( Parameters.Data, 32 + uint( a.Position.w ) * sizeof(MaterialRecord) );
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
    float3 heightDu=0,heightDv=0;
    bool heightFrame = HeightTangents(b.Position.xyz-a.Position.xyz,c.Position.xyz-a.Position.xyz,
        UV(uvB-uvA,material.HeightInfo.y),UV(uvC-uvA,material.HeightInfo.y),heightDu,heightDv);
    float2 heightUV = UV(uv,material.HeightInfo.y);
    if ( !visibilityOnly && material.HeightInfo.x>=0 && uint(material.HeightMapping.z)==2 && heightFrame && dot(toEye,toEye)>0 ) {
        heightUV = HeightParallax(RayTextures[NonUniformResourceIndex(material.HeightInfo.x)],MaterialSampler,
            heightUV,normalize(toEye),s.GeometricNormal,heightDu,heightDv,material.HeightMapping.x,material.HeightMapping.y,uint(material.HeightMapping.w));
        if ( material.HeightInfo.y==0 ) { uv.xy=heightUV; } else { uv.zw=heightUV; }
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

    if ( material.HeightInfo.x>=0 && material.HeightMapping.z!=0 && heightFrame ) {
        s.Normal = HeightBumpNormal(RayTextures[NonUniformResourceIndex(material.HeightInfo.x)],MaterialSampler,
            heightUV,s.Normal,heightDu,heightDv,material.HeightMapping.x);
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
    for ( uint layer = 0; layer < 128; ++layer ) {
        hit.Distance = -1.0;
        TraceRay( LoadRayScene( Parameters.SceneIndex ), RAY_FLAG_NONE, 0xff, 0, 1, 0, ray, hit );
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

bool HasAtmosphere() {
    return Effects().Control.x != 0.0;
}
float3 AtmosWorldOrigin( AtmosphereData a, float3 world ) {
    return AtmosClampGround( a, AtmosPlanetPosition( a, world ) );
}
float3 Environment( float3 direction, bool primary = false ) {
    if ( HasAtmosphere() ) {
        AtmosphereData a = Atmosphere();
        if ( primary && a.mSunIlluminance.w < 0.5 ) {
            return float3( 0 );
        }
        // Sky radiance is HDR and already includes the configured sun intensity.
        float3 observer = AtmosWorldOrigin( a, Parameters.Camera.xyz );
        float3 sky = AtmosRead( 2, AtmosSkyUv( a, observer, direction ) ).rgb;
        // Scene directional lights are sampled explicitly. Keep their sun disk
        // out of secondary misses to avoid counting the same direct light twice.
        if ( primary ) {
            float angle = acos( clamp( dot( direction, a.mSunDirection.xyz ), -1, 1 ) );
            float disk = 1 - smoothstep( a.mSunDirection.w - 1e-5, a.mSunDirection.w + 1e-5, angle );
            float solidAngle = AtmosPi * pow( sin( a.mSunDirection.w ), 2 );
            sky += disk * a.mSunIlluminance.xyz / max( solidAngle, 1e-6 ) *
                   AtmosSunTransmittance( a, observer, direction );
        }
        return min( sky, float3( 60000 ) );
    }
    if ( SceneData().HasFaces != 0 ) {
        // Inverse of the cubemap face directions used by raster sky lighting.
        float3 axis = abs( direction );
        uint face;
        float2 uv;
        if ( axis.x >= axis.y && axis.x >= axis.z ) {
            face = direction.x >= 0.0 ? 0u : 1u;
            uv = float2( direction.x >= 0.0 ? -direction.z : direction.z, -direction.y ) / axis.x;
        }
        else if ( axis.y >= axis.z ) {
            face = direction.y >= 0.0 ? 2u : 3u;
            uv = float2( direction.x, direction.y >= 0.0 ? direction.z : -direction.z ) / axis.y;
        }
        else {
            face = direction.z >= 0.0 ? 4u : 5u;
            uv = float2( direction.z >= 0.0 ? direction.x : -direction.x, -direction.y ) / axis.z;
        }
        return max( MaterialMap( LoadBufferValue<uint>( Parameters.Data, SceneData().FaceOffset + face * 4 ) )
                        .SampleLevel( AtmosphereSampler, uv * 0.5 + 0.5, 0 )
                        .rgb,
                   0.0 ) *
               Effects().Background.w;
    }
    int index = SceneData().Environment;
    if ( index < 0 ) {
        // Hybrid GI must not invent environment illumination when raster IBL
        // has no map. Preserve the standalone path tracer's fallback sky.
        if ( Parameters.Frame.w == 0 ) {
            return float3( 0.0 );
        }
        return lerp( float3( 0.04, 0.05, 0.08 ), float3( 0.35, 0.5, 0.8 ), saturate( direction.y * 0.5 + 0.5 ) ) *
               Effects().Background.w;
    }
    float2 uv = float2( atan2( direction.z, direction.x ) / ( 2.0 * PI ) + 0.5,
        acos( clamp( direction.y, -1.0, 1.0 ) ) / PI );
    return max( Map( index, uv ).rgb, 0.0 ) * Effects().Background.w;
}
// PBRT's shading frame keeps BSDF evaluation and sampling in the same coordinates.
// https://pbr-book.org/4ed/Reflection_Models/BSDF_Representation
float3 Tangent( float3 normal ) {
    return normalize( cross( abs( normal.z ) < 0.99999 ? float3( 0, 0, 1 ) : float3( 0, 1, 0 ), normal ) );
}

float3 ToWorld( float3 local, float3 normal ) {
    float3 tangent = Tangent( normal );
    return tangent * local.x + cross( normal, tangent ) * local.y + normal * local.z;
}

float3 ToLocal( float3 direction, float3 normal ) {
    float3 tangent = Tangent( normal );
    return float3( dot( direction, tangent ), dot( direction, cross( normal, tangent ) ), dot( direction, normal ) );
}

float3 Hemisphere( float3 normal, float cosine, float phi ) {
    float sine = sqrt( max( 0.0, 1.0 - cosine * cosine ) );
    return ToWorld( float3( sine * cos( phi ), sine * sin( phi ), cosine ), normal );
}

#include "base/PathTracing_Media.slang"

float Distribution( float cosine, float alpha ) {
    float cosine2 = cosine * cosine;
    float denominator = max( 1.0 - cosine2, 0.0 ) + cosine2 * alpha * alpha;
    return alpha * alpha / ( PI * denominator * denominator );
}

float Lambda( float cosine, float alpha ) {
    float tangent2 = max( 1.0 - cosine * cosine, 0.0 ) / ( cosine * cosine );
    return ( sqrt( 1.0 + alpha * alpha * tangent2 ) - 1.0 ) * 0.5;
}

float3 Fresnel( float3 f0, float cosine ) {
    // glTF retains Schlick's RGB Fresnel approximation rather than spectral IORs.
    return f0 + ( 1.0 - f0 ) * pow( 1.0 - cosine, 5.0 );
}

float SpecularProbability( Surface s, float3 view, uint lobes ) {
    if ( ( lobes & DIFFUSE ) == 0 || s.Metallic >= 1.0 || !any( s.Base > 0.0 ) ) {
        return 1.0;
    }
    if ( ( lobes & SPECULAR ) == 0 ) {
        return 0.0;
    }

    // The mixture is an engine material adapter; both SampleBSDF and its PDF use it.
    float specular = saturate( MaxComponent( Fresnel( s.F0, saturate( dot( s.Normal, view ) ) ) ) );
    float diffuse = MaxComponent( s.Base ) * ( 1.0 - s.Metallic ) * ( 1.0 - specular );
    return clamp( specular / max( specular + diffuse, 1e-8 ), 0.05, 0.95 );
}

// PBRT's Trowbridge-Reitz VNDF sampler and height-correlated masking-shadowing.
// https://pbr-book.org/4ed/Reflection_Models/Roughness_Using_Microfacet_Theory
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/util/scattering.h
float3 SampleVisibleNormal( float3 view, float alpha, float2 sample ) {
    float3 stretched = normalize( float3( alpha * view.xy, view.z ) );
    float3 tangent = stretched.z < 0.99999
                         ? normalize( cross( float3( 0, 0, 1 ), stretched ) )
                         : float3( 1, 0, 0 );
    float3 bitangent = cross( stretched, tangent );

    float radius = sqrt( sample.x );
    float phi = 2.0 * PI * sample.y;
    float2 disk = radius * float2( cos( phi ), sin( phi ) );
    float height = sqrt( max( 0.0, 1.0 - disk.x * disk.x ) );
    disk.y = lerp( height, disk.y, ( 1.0 + stretched.z ) * 0.5 );

    float z = sqrt( max( 0.0, 1.0 - dot( disk, disk ) ) );
    float3 normal = disk.x * tangent + disk.y * bitangent + z * stretched;
    return normalize( float3( alpha * normal.xy, max( normal.z, 1e-6 ) ) );
}

#include "base/PathTracing_Optics.slang"

float3 EvaluateBSDF( Surface s, float3 view, float3 light, out float pdf, uint lobes = ALL_LOBES ) {
    pdf = 0.0;
    if ( s.Model == MATERIAL_MEDIUM ) {
        pdf = PhaseHG( dot( view, light ), s.Anisotropy );
        return float3( pdf );
    }
    if ( s.SubsurfaceExit ) {
        float cosine = dot( s.Normal, light );
        if ( cosine <= 0.0 || dot( s.GeometricNormal, light ) <= 0.0 ) {
            return float3( 0 );
        }
        pdf = cosine * INV_PI;
        return float3( NormalizedFresnel( s.IOR, cosine ) );
    }
    if ( s.Model == MATERIAL_SUBSURFACE || s.Model == MATERIAL_DIELECTRIC ) {
        return EvaluateDielectric( s, view, light, pdf );
    }
    if ( s.Model == MATERIAL_UNLIT || ( s.Model == MATERIAL_CONDUCTOR && s.Roughness <= 0.01 ) ) {
        return float3( 0 );
    }
    float nv = dot( s.Normal, view );
    float nl = dot( s.Normal, light );
    if ( nv <= 0.0 || nl <= 0.0 || dot( s.GeometricNormal, view ) <= 0.0 ||
         dot( s.GeometricNormal, light ) <= 0.0 || dot( view + light, view + light ) <= 1e-12 ) {
        return float3( 0.0 );
    }

    float3 halfVector = normalize( view + light );
    float nh = saturate( dot( s.Normal, halfVector ) );
    float vh = saturate( dot( view, halfVector ) );
    float alpha = max( s.Roughness * s.Roughness, 0.000625 );
    float distribution = Distribution( nh, alpha );
    float lambdaV = Lambda( nv, alpha );
    float masking = 1.0 / ( 1.0 + lambdaV + Lambda( nl, alpha ) );
    float probability = SpecularProbability( s, view, lobes );

    // Visible-normal density converted to the sampled reflection direction.
    float specularPDF = distribution / ( 4.0 * nv * ( 1.0 + lambdaV ) );
    pdf = probability * specularPDF + ( 1.0 - probability ) * nl * INV_PI;
    float3 fresnel = s.Model == MATERIAL_CONDUCTOR ? ConductorFresnel( vh, s.Absorption, s.Scattering ) * s.Base : Fresnel( s.F0, vh );
    float3 value = float3( 0.0 );
    if ( ( lobes & SPECULAR ) != 0 ) {
        value += fresnel * distribution * masking / ( 4.0 * nv * nl );
    }
    if ( ( lobes & DIFFUSE ) != 0 ) {
        value += ( 1.0 - fresnel ) * ( 1.0 - s.Metallic ) * s.Base * INV_PI;
    }
    return value;
}

BSDFSample SampleBSDF( Surface s, float3 view, inout uint random, uint lobes = ALL_LOBES ) {
    BSDFSample result = ( BSDFSample )0;
    if ( s.Model == MATERIAL_MEDIUM ) {
        float2 sample = Random2D( random );
        result.Direction = SamplePhaseHG( view, s.Anisotropy, sample );
        result.PDF = PhaseHG( dot( view, result.Direction ), s.Anisotropy );
        result.Value = float3( result.PDF );
        return result;
    }
    if ( dot( s.Normal, view ) <= 0.0 || dot( s.GeometricNormal, view ) <= 0.0 ) {
        return result;
    }

    if ( !s.SubsurfaceExit && ( s.Model == MATERIAL_SUBSURFACE || s.Model == MATERIAL_DIELECTRIC ) ) {
        float choose = Random( random );
        float2 sample = Random2D( random );
        return SampleDielectric( s, view, choose, sample );
    }
    if ( s.Model == MATERIAL_CONDUCTOR && s.Roughness <= 0.01 ) {
        result.Direction = reflect( -view, s.Normal );
        result.Value = ConductorFresnel( dot( s.Normal, view ), s.Absorption, s.Scattering ) * s.Base /
                       abs( dot( s.Normal, result.Direction ) );
        result.PDF = 1.0;
        result.Delta = true;
        return result;
    }

    float choose = Random( random );
    float2 sample = Random2D( random );
    if ( !s.SubsurfaceExit && choose < SpecularProbability( s, view, lobes ) ) {
        float3 normal = ToWorld( SampleVisibleNormal( ToLocal( view, s.Normal ), max( s.Roughness * s.Roughness, 0.000625 ), sample ), s.Normal );
        result.Direction = reflect( -view, normal );
    }
    else {
        // Cosine-weighted diffuse sampling, as in PBRT's DiffuseBxDF.
        // https://pbr-book.org/4ed/Reflection_Models/Diffuse_Reflection
        float2 disk = SampleLensDisk( sample );
        result.Direction = ToWorld( float3( disk, sqrt( max( 0.0, 1.0 - dot( disk, disk ) ) ) ), s.Normal );
    }
    result.Value = EvaluateBSDF( s, view, result.Direction, result.PDF, lobes );
    return result;
}

float PowerWeight( float a, float b ) {
    // Scaling first avoids overflowing when a small emitter has a very large PDF.
    float scale = max( a, b );
    if ( scale <= 0.0 ) {
        return 0.0;
    }
    a /= scale;
    b /= scale;
    return a * a / ( a * a + b * b );
}

bool HasEnvironment() {
    SceneRecord scene = SceneData();
    return HasAtmosphere() || ( Effects().Background.w > 0.0 &&
                                  ( scene.HasFaces != 0 || scene.Environment >= 0 || Parameters.Frame.w != 0 ) );
}

uint LightCount( bool analytical = true ) {
    SceneRecord scene = SceneData();
    return ( analytical ? scene.LightCount : 0u ) + ( scene.EmitterCount > 0 ? 1u : 0u ) + ( HasEnvironment() ? 1u : 0u );
}

#include "base/PathTracing_Lights.slang"

float EnvironmentPDF( uint count ) {
    return count > 0 && HasEnvironment() ? INV_FOUR_PI * LightProbability(
        SceneData().LightCount + ( SceneData().EmitterCount > 0 ? 1u : 0u ), count ) : 0.0;
}

float EmitterPDF( uint triangle, Surface light, float3 previous, uint count ) {
    SceneRecord scene = SceneData();
    if ( count == 0 ) {
        return 0.0;
    }

    // Triangle order allows logarithmic lookup when a BSDF ray reaches an emitter.
    uint low = 0, high = scene.EmitterCount;
    while ( low < high ) {
        uint middle = low + ( high - low ) / 2;
        EmitterRecord candidate = LoadBufferValue<EmitterRecord>( Parameters.Data, scene.EmitterOffset + middle * 16 );
        if ( candidate.Triangle < triangle ) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    if ( low == scene.EmitterCount ) {
        return 0.0;
    }

    EmitterRecord emitter = LoadBufferValue<EmitterRecord>( Parameters.Data, scene.EmitterOffset + low * 16 );
    float3 delta = light.Position - previous;
    float distance2 = dot( delta, delta );
    float cosine = distance2 > 0.0 ? abs( dot( light.GeometricNormal, -delta / sqrt( distance2 ) ) ) : 0.0;
    if ( emitter.Triangle != triangle || cosine <= 0.0 || emitter.Area <= 0.0 ) {
        return 0.0;
    }
    return LightProbability( scene.LightCount, count ) * emitter.Probability * distance2 / ( emitter.Area * cosine );
}

float3 AnalyticalLight( Surface s, float3 view, uint index, uint lobes, inout uint random, bool shadows,
    MediumStack media = ( MediumStack )0 ) {
    SceneRecord scene = SceneData();
    LightRecord light = LoadBufferValue<LightRecord>( Parameters.Data, scene.LightOffset + index * 64 );
    float3 direction = light.PositionType.w == 0.0 ? normalize( -light.DirectionRadius.xyz ) : light.PositionType.xyz - s.Position;
    float distance = light.PositionType.w == 0.0 ? 1e30 : length( direction );
    if ( distance <= 0.0 ) {
        return float3( 0.0 );
    }
    direction = normalize( direction );

    float3 target = light.PositionType.xyz;
    float sourceWeight = 1.0;
    if ( Parameters.Frame.w != 0 && light.Spot.z > 0.0 ) {
        float2 sample = Random2D( random );
        if ( light.PositionType.w == 0.0 ) {
            // Uniform solar-disk solid angle, normalized to preserve normal-incidence irradiance.
            // https://pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions
            float oneMinusCosMax = 2.0 * pow( sin( 0.5 * light.Spot.z ), 2.0 );
            float oneMinusCos = sample.x * oneMinusCosMax;
            float sine = sqrt( max( 0.0, oneMinusCos * ( 2.0 - oneMinusCos ) ) );
            float phi = 2.0 * PI * sample.y;
            direction = ToWorld( float3( sine * cos( phi ), sine * sin( phi ), 1.0 - oneMinusCos ), direction );
            sourceWeight = 2.0 / ( 2.0 - oneMinusCosMax );
        }
        else if ( light.PositionType.w == 1.0 ) {
            // PBRT Sphere::Sample: sample the visible spherical emitter uniformly in solid angle.
            // I/(pi*r^2) radiance and the cone PDF preserve the point source's far-field intensity.
            // https://pbr-book.org/4ed/Shapes/Spheres
            float radius = light.Spot.z;
            if ( distance <= radius ) {
                return float3( 0.0 ); // The back of a one-sided emitting sphere does not light its interior.
            }
            float sinMax = radius / distance;
            float sin2Max = sinMax * sinMax;
            float cosMax = sqrt( max( 0.0, 1.0 - sin2Max ) );
            float oneMinusCosMax = sin2Max / ( 1.0 + cosMax );
            float cosTheta = 1.0 - sample.x * oneMinusCosMax;
            // Cancel sin^2(thetaMax) algebraically to keep the point-source limit finite.
            float sin2ThetaRatio = sample.x * ( 2.0 - sample.x * oneMinusCosMax ) / ( 1.0 + cosMax );
            float cosAlpha = sinMax * sin2ThetaRatio + cosTheta * sqrt( max( 0.0, 1.0 - sin2ThetaRatio ) );
            float sinAlpha = sqrt( max( 0.0, 1.0 - cosAlpha * cosAlpha ) );
            float phi = 2.0 * PI * sample.y;
            float3 normal = ToWorld( -float3( sinAlpha * cos( phi ), sinAlpha * sin( phi ), cosAlpha ), direction );
            target += radius * normal;
            direction = normalize( target - s.Position );
            sourceWeight = 2.0 / ( 1.0 + cosMax );
        }
    }

    // Range and cone attenuation preserve the engine's punctual-light convention.
    float attenuation = 1.0;
    if ( light.PositionType.w != 0.0 ) {
        float falloff = saturate( 1.0 - pow( distance / max( light.DirectionRadius.w, 0.0001 ), 4.0 ) );
        attenuation = falloff * falloff / max( distance * distance, 0.0001 );
    }
    if ( light.PositionType.w == 2.0 ) {
        attenuation *= saturate( ( dot( -direction, normalize( light.DirectionRadius.xyz ) ) - light.Spot.y ) /
                                 max( light.Spot.x - light.Spot.y, 1e-6 ) );
    }

    float3 illumination = light.ColorIntensity.rgb * light.ColorIntensity.w;
    if ( light.PositionType.w == 0 && HasAtmosphere() ) {
        AtmosphereData atmosphere = Atmosphere();
        if ( dot( normalize( -light.DirectionRadius.xyz ), atmosphere.mSunDirection.xyz ) > 0.9999 ) {
            // The explicit sun and atmospheric sky must use the same irradiance.
            // Its air transmittance is separate from the cloud/fog shadow connection.
            illumination = atmosphere.mSunIlluminance.xyz * AtmosSunTransmittance( atmosphere,
                AtmosWorldOrigin( atmosphere, s.Position ), direction );
        }
    }
    float pdf;
    float3 contribution = EvaluateBSDF( s, view, direction, pdf, lobes ) * ScatteringCosine( s, direction ) *
                          illumination * attenuation * sourceWeight;
    if ( !any( contribution > 0.0 ) ) {
        return float3( 0.0 );
    }
    float3 transmittance = !shadows ? float3( 1 ) : light.PositionType.w == 0.0
        ? ShadowTransmittance( s, direction, 1e30, media, random )
        : ShadowTransmittanceTo( s, target, media, random );
    return contribution * transmittance;
}

float3 Direct( Surface s, float3 view, inout uint random, bool shadows ) {
    // Hybrid raster replacement evaluates all punctual lights without sampling noise.
    float3 result = float3( 0.0 );
    for ( uint index = 0; index < SceneData().LightCount; ++index ) {
        result += AnalyticalLight( s, view, index, ALL_LOBES, random, shadows );
    }
    return result;
}

// PBRT SampleLd: choose a light, sample it, then compare its full PDF with the BSDF.
// The emissive group retains its power/area CDF; other entries use uniform selection.
// https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer
float3 SampleDirect( Surface s, float3 view, inout uint random, uint lobes = ALL_LOBES, bool analytical = true,
    MediumStack media = ( MediumStack )0 ) {
    SceneRecord scene = SceneData();
    uint count = LightCount( analytical );
    if ( count == 0 ) {
        return float3( 0.0 );
    }

    float probability;
    uint selected = SampleLight( Random( random ), analytical, probability );
    if ( selected < scene.LightCount ) {
        // Analytical sources use light sampling only. Secondary misses exclude their sun disk,
        // and point-source spheres are not scene geometry, so this proposal has MIS weight one.
        return AnalyticalLight( s, view, selected, lobes, random, true, media ) / probability;
    }

    float3 direction, emission;
    float lightPDF;
    Surface light = ( Surface )0;
    bool environment = selected >= scene.LightCount + ( scene.EmitterCount > 0 ? 1u : 0u );
    if ( environment ) {
        // PBRT UniformInfiniteLight proposal, evaluated against our HDR sky/map.
        // Uniform sphere sampling stays unbiased for cubemaps, panoramas and LUTs.
        // https://pbr-book.org/4ed/Light_Sources/Infinite_Area_Lights
        float2 sample = Random2D( random );
        float cosine = 1.0 - 2.0 * sample.x;
        direction = Hemisphere( float3( 0, 1, 0 ), cosine, 2.0 * PI * sample.y );
        emission = Environment( direction );
        lightPDF = EnvironmentPDF( count );
    }
    else {
        float choose = Random( random );
        uint low = 0, high = scene.EmitterCount - 1;
        while ( low < high ) {
            uint middle = low + ( high - low ) / 2;
            EmitterRecord candidate = LoadBufferValue<EmitterRecord>( Parameters.Data, scene.EmitterOffset + middle * 16 );
            if ( choose >= candidate.CDF ) {
                low = middle + 1;
            }
            else {
                high = middle;
            }
        }

        EmitterRecord emitter = LoadBufferValue<EmitterRecord>( Parameters.Data, scene.EmitterOffset + low * 16 );
        float2 sample = Random2D( random );
        float u = sqrt( sample.x );
        light = Evaluate( emitter.Triangle, float2( u * ( 1.0 - sample.y ), u * sample.y ) );
        float3 delta = light.Position - s.Position;
        float distance2 = dot( delta, delta );
        if ( distance2 <= 0.0 ) {
            return float3( 0.0 );
        }
        direction = delta / sqrt( distance2 );
        float cosine = dot( light.GeometricNormal, -direction );
        if ( ( !light.EmissionTwoSided && cosine <= 0.0 ) || abs( cosine ) <= 0.0 ||
             ( light.AlphaMode == 1 && light.Alpha < light.Cutoff ) ) {
            return float3( 0.0 );
        }
        emission = light.Emission * ( light.AlphaMode == 2 ? light.Alpha : 1.0 );
        lightPDF = probability * emitter.Probability * distance2 / ( emitter.Area * abs( cosine ) );
    }

    float bsdfPDF;
    float3 value = EvaluateBSDF( s, view, direction, bsdfPDF, lobes );
    if ( lightPDF <= 0.0 || bsdfPDF <= 0.0 || !any( emission > 0.0 ) ) {
        return float3( 0.0 );
    }
    float3 transmittance = environment ? ShadowTransmittance( s, direction, 1e30, media, random )
        : ShadowTransmittanceTo( s, Offset( light, s.Position - light.Position ), media, random );
    return value * emission * transmittance * ScatteringCosine( s, direction ) * PowerWeight( lightPDF, bsdfPDF ) / lightPDF;
}

float3 CameraBackground( float3 direction, inout uint random ) {
    float3 background = Effects().Background.xyz;
    if ( background.z != 0.0 ) {
        return float3( 0.0 );
    }
    if ( background.x > 0.0 ) {
        // Filter radiance directions only after a camera miss: silhouettes and
        // secondary-ray illumination must not inherit this artistic blur.
        // Progressive accumulation averages the cone samples without allocating
        // raster IBL targets. Prefilter mode uses a roughness-like angular width;
        // ordinary sky mode approximates a cubemap mip's growing footprint.
        float angle = background.y != 0.0
                          ? pow( saturate( background.x / 8.0 ), 2.0 ) * ( PI * 0.5 )
                          : min( ( exp2( background.x ) - 1.0 ) / 512.0, PI * 0.5 );
        float cosine = lerp( 1.0, cos( angle ), Random( random ) );
        direction = Hemisphere( direction, cosine, Random( random ) * 2.0 * PI );
    }
    return Environment( direction, true );
}

// PBRT's PathIntegrator::Li, adapted to RGB and hardware TraceRay intersections.
// Keep the preceding vertex's sampling context for BOTH emitter hits and sky misses.
// https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer
#include "base/PathTracing_Bssrdf.slang"

bool ContinuePath( inout float3 throughput, float etaScale, uint depth, inout uint random ) {
    float survive = MaxComponent( throughput * etaScale );
    if ( survive <= 0.0 ) {
        return false;
    }
    if ( depth > 2 && survive < 1.0 ) {
        if ( Random( random ) >= survive ) {
            return false;
        }
        throughput /= survive;
    }
    return true;
}

float3 Integrate( float3 origin, float3 direction, inout uint random, uint maxDepth,
    bool cameraPath, PreviousVertex previous ) {
    float3 radiance = float3( 0.0 );
    float3 throughput = float3( 1.0 );
    float etaScale = 1.0;
    MediumStack media = ( MediumStack )0;
    uint depth = 0, nullCrossings = 0;
    int fogChannel = -1;

    while ( true ) {
        Surface s;
        Hit hit;
        bool found = Intersect( origin, direction, 1e30, random, s, hit );
        MediumState medium = ReadMedium( media, media.Count );
        float distance;
        float3 mediumWeight;
        uint fogMedium = 0xffffffffu;
        bool scattered;
        if ( media.Count == 0 && VolumeCount( Parameters.Volumes ) != 0 ) {
            scattered = SampleSceneFog( origin, direction, found ? hit.Distance : 1e30, random,
                fogChannel, distance, mediumWeight, fogMedium );
        }
        else {
            float2 mediumSample = Random2D( random );
            scattered = SampleMedium( medium, found ? hit.Distance : 1e30, mediumSample, distance, mediumWeight );
        }
        throughput *= mediumWeight;
        if ( !any( throughput > 0.0 ) ) {
            break;
        }

        // Do not run the existing planetary-air integral through glass interiors.
        if ( HasAtmosphere() && media.Count == 0 && ( found || scattered ) ) {
            AtmosphereData atmosphere = Atmosphere();
            if ( atmosphere.mView.y > 0.5 ) {
                float3 planetOrigin = AtmosWorldOrigin( atmosphere, origin );
                float entry = max( 0, AtmosEntryDistance( atmosphere, planetOrigin, direction ) );
                float range = min( distance * atmosphere.mRadii.z, entry + atmosphere.mGround.w );
                AtmosIntegration air = AtmosIntegrate( atmosphere, planetOrigin, direction,
                    atmosphere.mSunDirection.xyz, range, 16, true, false, false );
                radiance += throughput * air.mRadiance * atmosphere.mSunIlluminance.xyz;
                throughput *= air.mTransmittance;
            }
        }

        if ( scattered ) {
            if ( depth >= maxDepth ) {
                break;
            }
            Surface event = ( Surface )0;
            event.Model = MATERIAL_MEDIUM;
            event.Position = origin + distance * direction;
            event.Anisotropy = medium.Anisotropy;
            bool receiveShadows = true;
            if ( fogMedium != 0xffffffffu ) {
                VolumeRecord fog = LoadVolume( Parameters.Volumes, fogMedium );
                event.Anisotropy = fog.ScatteringPhase.w;
                receiveShadows = fog.Flags.w != 0;
            }
            radiance += throughput * SampleDirect( event, -direction, random, ALL_LOBES, receiveShadows, media );
            BSDFSample phase = SampleBSDF( event, -direction, random );
            if ( phase.PDF <= 0.0 ) {
                break;
            }
            throughput *= phase.Value / phase.PDF;
            previous.Position = event.Position;
            previous.BSDFPDF = phase.PDF;
            previous.LightCount = LightCount();
            origin = event.Position;
            direction = phase.Direction;
            if ( !ContinuePath( throughput, etaScale, ++depth, random ) ) {
                break;
            }
            continue;
        }

        if ( !found ) {
            bool background = cameraPath && depth == 0;
            float weight = previous.BSDFPDF > 0.0 && previous.LightCount > 0
                               ? PowerWeight( previous.BSDFPDF, EnvironmentPDF( previous.LightCount ) )
                               : 1.0;
            radiance += throughput * weight * ( background ? CameraBackground( direction, random ) : Environment( direction ) );
            break;
        }

        // An IOR-one boundary only changes the medium; it consumes no scattering depth.
        if ( NullMediumBoundary( s, media ) ) {
            if ( ++nullCrossings > 256 || !CrossMediumBoundary( media, s ) ) {
                break;
            }
            origin = Offset( s, direction );
            continue;
        }

        if ( any( s.Emission > 0.0 ) ) {
            float weight = previous.BSDFPDF > 0.0 && previous.LightCount > 0
                ? PowerWeight( previous.BSDFPDF, EmitterPDF( hit.Triangle, s, previous.Position, previous.LightCount ) ) : 1.0;
            radiance += throughput * s.Emission * weight;
        }
        // A terminal ray still sees emission, but does not create another scattering event.
        if ( depth >= maxDepth || s.EmissionOnly ) {
            break;
        }

        float materialIOR = s.IOR;
        if ( s.Model == MATERIAL_DIELECTRIC && !s.ThinWalled ) {
            float outside = ReadMedium( media, s.FrontFace ? media.Count : ( media.Count > 1 ? media.Count - 1 : 0 ) ).IOR;
            s.IOR = materialIOR / outside;
        }
        radiance += throughput * SampleDirect( s, -direction, random, ALL_LOBES, true, media );
        BSDFSample sample = SampleBSDF( s, -direction, random );
        if ( sample.PDF <= 0.0 || !any( sample.Value > 0.0 ) ) {
            break;
        }
        throughput *= sample.Value * abs( dot( s.Normal, sample.Direction ) ) / sample.PDF;
        if ( sample.Transmitted ) {
            etaScale *= sample.Eta * sample.Eta;
            if ( s.Model == MATERIAL_SUBSURFACE ) {
                Surface exit;
                float3 weight;
                if ( !SampleBssrdf( s, random, exit, weight ) ) {
                    break;
                }
                throughput *= weight;
                s = exit;
                radiance += throughput * SampleDirect( s, s.Normal, random, ALL_LOBES, true, media );
                sample = SampleBSDF( s, s.Normal, random );
                if ( sample.PDF <= 0.0 ) {
                    break;
                }
                throughput *= sample.Value * abs( dot( s.Normal, sample.Direction ) ) / sample.PDF;
                etaScale /= s.IOR * s.IOR;
            }
            else if ( s.ThinWalled ) {
                // ThinDielectric has no interior segment; thickness supplies optional absorption.
                throughput *= exp( -s.Absorption * s.Thickness / max( abs( dot( s.Normal, sample.Direction ) ), 1e-6 ) );
            }
            else if ( s.Model == MATERIAL_DIELECTRIC ) {
                if ( !CrossMediumBoundary( media, s ) ) {
                    break;
                }
            }
        }

        // Russian roulette preserves expected radiance by compensating survivors.
        if ( !ContinuePath( throughput, etaScale, ++depth, random ) ) {
            break;
        }

        previous.Position = s.Position;
        previous.BSDFPDF = sample.Delta ? 0.0 : sample.PDF;
        previous.LightCount = LightCount();
        origin = Offset( s, sample.Direction );
        direction = sample.Direction;
    }
    return radiance;
}

float3 Indirect( Surface s, float3 view, inout uint random, uint lobes, uint maxDepth ) {
    // Raster already supplies punctual direct lighting. Sample only emitters/sky
    // here, then hand the exact first-lobe PDF to the shared transport loop.
    if ( lobes == DIFFUSE && ( s.Metallic >= 1.0 || !any( s.Base > 0.0 ) ) ) {
        return float3( 0.0 );
    }

    float3 radiance = SampleDirect( s, view, random, lobes, false );
    BSDFSample sample = SampleBSDF( s, view, random, lobes );
    if ( sample.PDF > 0.0 && any( sample.Value > 0.0 ) ) {
        PreviousVertex previous;
        previous.Position = s.Position;
        previous.BSDFPDF = sample.PDF;
        previous.LightCount = LightCount( false );
        radiance += Integrate( Offset( s, sample.Direction ), sample.Direction, random, maxDepth, false, previous ) *
                    sample.Value * saturate( dot( s.Normal, sample.Direction ) ) / sample.PDF;
    }
    return radiance;
}

// PBRT AOIntegrator::Li: geometric-normal hemisphere visibility, normalized so open space is one.
// https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/cpu/integrators.cpp#L1281
// https://pbrt.org/fileformat-v4#integrators (ambientocclusion: cossample, maxdistance)
float AmbientOcclusion( Surface surface, float3 view, float2 sample, bool cosineSampling,
    float maxDistance, inout uint random ) {
    float3 normal = dot( surface.GeometricNormal, view ) >= 0.0 ? surface.GeometricNormal : -surface.GeometricNormal;
    float3 local;
    float pdf;
    if ( cosineSampling ) {
        float2 disk = SampleLensDisk( sample );
        local = float3( disk, sqrt( max( 0.0, 1.0 - dot( disk, disk ) ) ) );
        pdf = local.z * INV_PI;
    }
    else {
        float sine = sqrt( max( 0.0, 1.0 - sample.x * sample.x ) );
        float phi = 2.0 * PI * sample.y;
        local = float3( sine * cos( phi ), sine * sin( phi ), sample.x );
        pdf = 1.0 / ( 2.0 * PI );
    }
    if ( pdf <= 0.0 ) {
        return 0.0;
    }

    float3 direction = ToWorld( local, normal );
    return Visible( surface, direction, maxDistance, random ) ? max( 0.0, dot( direction, normal ) ) / ( PI * pdf ) : 0.0;
}

void GenerateCameraRay( uint2 pixel, float2 jitter, out float3 origin, out float3 direction ) {
    float2 ndc = ( float2( pixel ) + jitter ) / float2( DispatchRaysDimensions().xy ) * 2.0 - 1.0;
    float4 nearPoint = mul( Parameters.InverseViewProjection, float4( ndc.x, -ndc.y, 0.0, 1.0 ) );
    float4 farPoint = mul( Parameters.InverseViewProjection, float4( ndc.x, -ndc.y, 0.5, 1.0 ) );
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    origin = Parameters.Camera.w != 0.0 ? nearPoint.xyz : Parameters.Camera.xyz;
    direction = normalize( farPoint.xyz - origin );
}

void RenderAmbientOcclusion( uint2 pixel ) {
    if ( Parameters.Occlusion.x == 0 ) {
        return;
    }

    uint previous = Parameters.Occlusion.z;
    uint count = min( Parameters.Occlusion.y, 4096u - min( previous, 4096u ) );
    if ( count == 0 ) {
        return;
    }
    // AO owns its sample stream/history and never perturbs the transport estimator.
    uint random = ( pixel.x + pixel.y * DispatchRaysDimensions().x + 1u ) ^
        ( Parameters.Frame.x * SAMPLE_STREAMS_PER_FRAME * 0x9e3779b9u ) ^ 0x85ebca6bu;
    float3 origin, direction;
    GenerateCameraRay( pixel, float2( 0.5 ), origin, direction );
    Surface surface;
    Hit hit;
    float sum = 0.0;
    if ( Intersect( origin, direction, 1e30, random, surface, hit, true ) ) {
        for ( uint index = 0; index < count; ++index ) {
            float2 sample = Random2D( random );
            sum += AmbientOcclusion( surface, -direction, sample, Parameters.Occlusion.w != 0,
                Parameters.OcclusionMaxDistance, random );
        }
    }
    float history = previous > 0 ? OcclusionHistory.Load( int3( pixel, 0 ) ).r : 0.0;
    float average = ( history * float( previous ) + sum ) / float( previous + count );
    OcclusionOutput[pixel] = float4( average, average, average, 1.0 );
}

[shader( "raygeneration" )] 
void main() {
    uint2 pixel = DispatchRaysIndex().xy;
    RenderAmbientOcclusion( pixel );
    bool path = Parameters.Frame.w != 0;
    bool gi = !path && ( Parameters.Options.z & 2u ) != 0;
    bool reflections = !path && ( Parameters.Options.z & 1u ) != 0;
    uint samples =
        path ? min( Parameters.Frame.y, Parameters.Options.w - min( Parameters.Options.x, Parameters.Options.w ) )
             : 1;

    float3 sum = float3( 0.0 );
    for ( uint sample = 0; sample < samples; ++sample ) {
        // Camera edits clear history, but must not restart the screen-space noise.
        // Reserve disjoint sample blocks even when SPP changes between frames.
        // https://pbr-book.org/4ed/Sampling_and_Reconstruction/Independent_Sampler
        uint sampleIndex = Parameters.Frame.x * SAMPLE_STREAMS_PER_FRAME + sample;
        uint random = ( pixel.x + pixel.y * DispatchRaysDimensions().x + 1u ) ^ ( sampleIndex * 0x9e3779b9u );

        float2 jitter = path ? Random2D( random ) : float2( 0.5 );
        float3 origin, direction;
        GenerateCameraRay( pixel, jitter, origin, direction );
        if ( path ) {
            float4 lensRight = Effects().LensRight;
            if ( lensRight.w > 0.0 ) {
                float4 lensUp = Effects().LensUp;
                float3 lensForward = Effects().LensForward.xyz;
                float2 lensSample = Random2D( random );
                ApplyDepthOfField( lensRight, lensUp, lensForward, lensSample, origin, direction );
            }

            sum += Integrate( origin, direction, random, Parameters.Frame.z, true, ( PreviousVertex )0 );
        }
        else {
            Surface s;
            Hit hit;
            float3 indirect = float3( 0.0 );
            uint indirectSamples = gi ? Parameters.Frame.y : 1u;

            // Hybrid outputs are consumed by opaque raster surfaces; blended
            // geometry retains raster lighting and must not replace this hit.
            if ( Intersect( origin, direction, 1e30, random, s, hit, true ) ) {
                if ( Parameters.Options.y != 0 ) {
                    sum = Direct( s, -direction, random, true );
                }

                for ( uint indirectSample = 0; indirectSample < indirectSamples; ++indirectSample ) {
                    if ( gi ) {
                        indirect += Indirect( s, -direction, random, DIFFUSE, Parameters.Frame.z );
                    }

                    if ( reflections ) {
                        indirect += Indirect( s, -direction, random, SPECULAR, Parameters.Options.w );
                    }
                }
            }

            if ( gi || reflections ) {
                uint previous = Parameters.Options.x;
                float3 history = previous > 0 ? History.Load( int3( pixel, 0 ) ).rgb : float3( 0.0 );
                indirect = ( history * float( previous ) + indirect ) / float( previous + indirectSamples );
            }
            else {
                indirect /= float( indirectSamples );
            }

            Auxiliary[pixel] = float4( indirect, 1.0 );
        }
    }

    if ( path ) {
        uint previous = Parameters.Options.x;
        float3 history = previous > 0 ? History.Load( int3( pixel, 0 ) ).rgb : float3( 0.0 );
        float3 average = ( history * float( previous ) + sum ) / float( max( previous + samples, 1u ) );
        Auxiliary[pixel] = float4( average, 1.0 );
        // Display effects consume this HDR image after accumulation. Never store bloom,
        // tone mapping, the editor grid or UI in the radiance history.
        Output[pixel] = float4( average, 1.0 );
    }
    else {
        Output[pixel] = float4( sum, 1.0 );
    }
}
