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

// Both BSDF and light sampling can generate the same path. The power heuristic
// combines their contributions while preserving each strategy's PDF.
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

// PBRT PowerLightSampler's discrete PMF is part of BOTH light proposals and MIS.
// Mikoto uses a binary-search CDF and a small uniform mixture rather than an alias table.
// https://pbr-book.org/4ed/Light_Sources/Light_Sampling#PowerLightSampler
float2 LightDistribution( uint index ) {
    return LoadBufferValue<float2>( Parameters.Data, Parameters.LightDistribution + index * sizeof( float2 ) );
}

float LightProbability( uint index, uint count ) {
    if ( count == 0 ) {
        return 0.0;
    }
    if ( Parameters.LightDistribution == 0 ) {
        return 1.0 / float( count );
    }
    // Renormalize the CDF over the chosen interval. Hybrid indirect omits analytical lights.
    uint first = count == LightCount() ? 0u : SceneData().LightCount;
    if ( index < first || index >= first + count ) {
        return 0.0;
    }
    float lower = first > 0 ? LightDistribution( first - 1 ).x : 0.0;
    float upper = LightDistribution( first + count - 1 ).x;
    return LightDistribution( index ).y / ( upper - lower );
}

uint SampleLight( float sample, bool analytical, out float probability ) {
    uint count = LightCount( analytical );
    uint first = analytical ? 0u : SceneData().LightCount;
    if ( Parameters.LightDistribution == 0 ) {
        probability = 1.0 / float( count );
        return first + min( uint( sample * count ), count - 1 );
    }
    float lower = first > 0 ? LightDistribution( first - 1 ).x : 0.0;
    float upper = LightDistribution( first + count - 1 ).x;
    float target = min( lerp( lower, upper, sample ), NextFloat( upper, false ) );
    uint low = first, high = first + count - 1;
    while ( low < high ) {
        uint middle = low + ( high - low ) / 2;
        if ( target >= LightDistribution( middle ).x ) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    probability = LightProbability( low, count );
    return low;
}

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
        EmitterRecord candidate = LoadBufferValue<EmitterRecord>( Parameters.Data, scene.EmitterOffset + middle * sizeof( EmitterRecord ) );
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

    EmitterRecord emitter = LoadBufferValue<EmitterRecord>( Parameters.Data, scene.EmitterOffset + low * sizeof( EmitterRecord ) );
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
    LightRecord light = LoadBufferValue<LightRecord>( Parameters.Data, scene.LightOffset + index * sizeof( LightRecord ) );
    float3 direction = light.PositionType.w == 0.0 ? normalize( -light.DirectionRadius.xyz ) : light.PositionType.xyz - s.Position;
    float distance = light.PositionType.w == 0.0 ? RAY_MAX_DISTANCE : length( direction );
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
        ? ShadowTransmittance( s, direction, RAY_MAX_DISTANCE, media, random )
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
            EmitterRecord candidate = LoadBufferValue<EmitterRecord>( Parameters.Data, scene.EmitterOffset + middle * sizeof( EmitterRecord ) );
            if ( choose >= candidate.CDF ) {
                low = middle + 1;
            }
            else {
                high = middle;
            }
        }

        EmitterRecord emitter = LoadBufferValue<EmitterRecord>( Parameters.Data, scene.EmitterOffset + low * sizeof( EmitterRecord ) );
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
    float3 transmittance = environment ? ShadowTransmittance( s, direction, RAY_MAX_DISTANCE, media, random )
        : ShadowTransmittanceTo( s, Offset( light, s.Position - light.Position ), media, random );
    return value * emission * transmittance * ScatteringCosine( s, direction ) * PowerWeight( lightPDF, bsdfPDF ) / lightPDF;
}
