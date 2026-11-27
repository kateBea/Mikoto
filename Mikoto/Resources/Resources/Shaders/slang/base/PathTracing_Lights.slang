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

// Sample analytical lights, emissive geometry and the environment. Multiple-importance weights
// combine light sampling with paths that encounter emitters through BSDF sampling.

#pragma once

// Both BSDF and light sampling can generate the same path. The power heuristic
// combines their contributions while preserving each strategy's PDF.
// Balance two sampling strategies with the squared-PDF power heuristic. A strategy with a much
// larger PDF receives most of the contribution weight.
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
    return HasAtmosphere() || ( Effects().mBackground.w > 0.0 &&
                                  ( scene.mHasFaces != 0 || scene.mEnvironment >= 0 || Parameters.mFrame.w != 0 ) );
}

uint LightCount( bool analytical = true ) {
    SceneRecord scene = SceneData();
    return ( analytical ? scene.mLightCount : 0u ) + ( scene.mEmitterCount > 0 ? 1u : 0u ) + ( HasEnvironment() ? 1u : 0u );
}

// PBRT PowerLightSampler's discrete PMF is part of BOTH light proposals and MIS.
// Mikoto uses a binary-search CDF and a small uniform mixture rather than an alias table.
// https://pbr-book.org/4ed/Light_Sources/Light_Sampling#PowerLightSampler
// Build the discrete light-selection distribution used for direct sampling. Selection
// probabilities must also be included when evaluating the reverse emitter PDF.
float2 LightDistribution( uint index ) {
    return LoadBufferValue<float2>( Parameters.mData, Parameters.mLightDistribution + index * sizeof( float2 ) );
}

float LightProbability( uint index, uint count ) {
    if ( count == 0 ) {
        return 0.0;
    }
    if ( Parameters.mLightDistribution == 0 ) {
        return 1.0 / float( count );
    }
    // Renormalize the CDF over the chosen interval. Hybrid indirect omits analytical lights.
    uint first = count == LightCount() ? 0u : SceneData().mLightCount;
    if ( index < first || index >= first + count ) {
        return 0.0;
    }
    float lower = first > 0 ? LightDistribution( first - 1 ).x : 0.0;
    float upper = LightDistribution( first + count - 1 ).x;
    return LightDistribution( index ).y / ( upper - lower );
}

// Sample one emitter connection and return its direction, radiance and probability.
// Geometry/solid-angle conversion depends on the selected emitter type.
uint SampleLight( float sample, bool analytical, out float probability ) {
    uint count = LightCount( analytical );
    uint first = analytical ? 0u : SceneData().mLightCount;
    if ( Parameters.mLightDistribution == 0 ) {
        probability = 1.0 / float( count );
        return first + min( uint( sample * count ), count - 1 );
    }
    float lower = first > 0 ? LightDistribution( first - 1 ).x : 0.0;
    float upper = LightDistribution( first + count - 1 ).x;
    float target = min( lerp( lower, upper, sample ), NextFloat( upper, false ) );
    uint low = first;
    uint high = first + count - 1;
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

// Evaluate the probability of sampling this environment direction. Emitter-hit weighting uses
// the same distribution as explicit environment sampling.
float EnvironmentPDF( uint count ) {
    return count > 0 && HasEnvironment() ? INV_FOUR_PI * LightProbability(
                                                             SceneData().mLightCount + ( SceneData().mEmitterCount > 0 ? 1u : 0u ), count )
                                         : 0.0;
}

// Evaluate the light-sampling PDF for an emissive surface reached by another strategy. This is
// the reverse probability used for multiple-importance weighting.
float EmitterPDF( uint triangle, Surface light, float3 previous, uint count ) {
    SceneRecord scene = SceneData();
    if ( count == 0 ) {
        return 0.0;
    }

    // Triangle order allows logarithmic lookup when a BSDF ray reaches an emitter.
    uint low = 0;
    uint high = scene.mEmitterCount;
    while ( low < high ) {
        uint middle = low + ( high - low ) / 2;
        EmitterRecord candidate = LoadBufferValue<EmitterRecord>( Parameters.mData, scene.mEmitterOffset + middle * sizeof( EmitterRecord ) );
        if ( candidate.mTriangle < triangle ) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    if ( low == scene.mEmitterCount ) {
        return 0.0;
    }

    EmitterRecord emitter = LoadBufferValue<EmitterRecord>( Parameters.mData, scene.mEmitterOffset + low * sizeof( EmitterRecord ) );
    float3 delta = light.mPosition - previous;
    float distance2 = dot( delta, delta );
    float cosine = distance2 > 0.0 ? abs( dot( light.mGeometricNormal, -delta / sqrt( distance2 ) ) ) : 0.0;
    if ( emitter.mTriangle != triangle || cosine <= 0.0 || emitter.mArea <= 0.0 ) {
        return 0.0;
    }
    return LightProbability( scene.mLightCount, count ) * emitter.mProbability * distance2 / ( emitter.mArea * cosine );
}

float3 AnalyticalLight( Surface s, float3 view, uint index, uint lobes, inout uint random, bool shadows,
    MediumStack media = ( MediumStack )0 ) {
    SceneRecord scene = SceneData();
    LightRecord light = LoadBufferValue<LightRecord>( Parameters.mData, scene.mLightOffset + index * sizeof( LightRecord ) );
    float3 direction = light.mPositionType.w == 0.0 ? normalize( -light.mDirectionRadius.xyz ) : light.mPositionType.xyz - s.mPosition;
    float distance = light.mPositionType.w == 0.0 ? RAY_MAX_DISTANCE : length( direction );
    if ( distance <= 0.0 ) {
        return float3( 0.0 );
    }
    direction = normalize( direction );

    float3 target = light.mPositionType.xyz;
    float sourceWeight = 1.0;
    if ( Parameters.mFrame.w != 0 && light.mSpot.z > 0.0 ) {
        float2 sample = Random2D( random );
        if ( light.mPositionType.w == 0.0 ) {
            // Uniform solar-disk solid angle, normalized to preserve normal-incidence irradiance.
            // https://pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions
            float oneMinusCosMax = 2.0 * pow( sin( 0.5 * light.mSpot.z ), 2.0 );
            float oneMinusCos = sample.x * oneMinusCosMax;
            float sine = sqrt( max( 0.0, oneMinusCos * ( 2.0 - oneMinusCos ) ) );
            float phi = 2.0 * PI * sample.y;
            direction = ToWorld( float3( sine * cos( phi ), sine * sin( phi ), 1.0 - oneMinusCos ), direction );
            sourceWeight = 2.0 / ( 2.0 - oneMinusCosMax );
        }
        else if ( light.mPositionType.w == 1.0 ) {
            // PBRT Sphere::Sample: sample the visible spherical emitter uniformly in solid angle.
            // I/(pi*r^2) radiance and the cone PDF preserve the point source's far-field intensity.
            // https://pbr-book.org/4ed/Shapes/Spheres
            float radius = light.mSpot.z;
            if ( distance <= radius ) {
                return float3( 0.0 );// The back of a one-sided emitting sphere does not light its interior.
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
            direction = normalize( target - s.mPosition );
            sourceWeight = 2.0 / ( 1.0 + cosMax );
        }
    }

    // Range and cone attenuation preserve the engine's punctual-light convention.
    float attenuation = 1.0;
    if ( light.mPositionType.w != 0.0 ) {
        float falloff = saturate( 1.0 - pow( distance / max( light.mDirectionRadius.w, 0.0001 ), 4.0 ) );
        attenuation = falloff * falloff / max( distance * distance, 0.0001 );
    }
    if ( light.mPositionType.w == 2.0 ) {
        attenuation *= saturate( ( dot( -direction, normalize( light.mDirectionRadius.xyz ) ) - light.mSpot.y ) /
                                 max( light.mSpot.x - light.mSpot.y, 1e-6 ) );
    }

    float3 illumination = light.mColorIntensity.rgb * light.mColorIntensity.w;
    if ( light.mPositionType.w == 0 && HasAtmosphere() ) {
        AtmosphereData atmosphere = Atmosphere();
        if ( dot( normalize( -light.mDirectionRadius.xyz ), atmosphere.mSunDirection.xyz ) > 0.9999 ) {
            // The explicit sun and atmospheric sky must use the same irradiance.
            // Its air transmittance is separate from the cloud/fog shadow connection.
            illumination = atmosphere.mSunIlluminance.xyz * AtmosSunTransmittance( atmosphere,
                                                                AtmosWorldOrigin( atmosphere, s.mPosition ), direction );
        }
    }
    float pdf;
    float3 contribution = EvaluateBSDF( s, view, direction, pdf, lobes ) * ScatteringCosine( s, direction ) *
                          illumination * attenuation * sourceWeight;
    if ( !any( contribution > 0.0 ) ) {
        return float3( 0.0 );
    }
    float3 transmittance = !shadows ? float3( 1 ) : light.mPositionType.w == 0.0 ? ShadowTransmittance( s, direction, RAY_MAX_DISTANCE, media, random )
                                                                                : ShadowTransmittanceTo( s, target, media, random );
    return contribution * transmittance;
}

float3 Direct( Surface s, float3 view, inout uint random, bool shadows ) {
    // Hybrid raster replacement evaluates all punctual lights without sampling noise.
    float3 result = float3( 0.0 );
    for ( uint index = 0; index < SceneData().mLightCount; ++index ) {
        result += AnalyticalLight( s, view, index, ALL_LOBES, random, shadows );
    }
    return result;
}

// PBRT SampleLd: choose a light, sample it, then compare its full PDF with the BSDF.
// The emissive group retains its power/area CDF; other entries use uniform selection.
// https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer
// Connect the surface to a sampled light and test transmittance along the connection. Divide by
// the light PDF and apply the matching BSDF/phase value and MIS weight.
float3 SampleDirect( Surface s, float3 view, inout uint random, uint lobes = ALL_LOBES, bool analytical = true,
    MediumStack media = ( MediumStack )0, bool shadows = true ) {
    SceneRecord scene = SceneData();
    uint count = LightCount( analytical );
    if ( count == 0 ) {
        return float3( 0.0 );
    }

    float probability;
    uint selected = SampleLight( Random( random ), analytical, probability );
    if ( selected < scene.mLightCount ) {
        // Analytical sources use light sampling only. Secondary misses exclude their sun disk,
        // and point-source spheres are not scene geometry, so this proposal has MIS weight one.
        return AnalyticalLight( s, view, selected, lobes, random, shadows, media ) / probability;
    }

    float3 direction;
    float3 emission;
    float lightPDF;
    Surface light = ( Surface )0;
    bool environment = selected >= scene.mLightCount + ( scene.mEmitterCount > 0 ? 1u : 0u );
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
        uint low = 0;
        uint high = scene.mEmitterCount - 1;
        while ( low < high ) {
            uint middle = low + ( high - low ) / 2;
            EmitterRecord candidate = LoadBufferValue<EmitterRecord>( Parameters.mData, scene.mEmitterOffset + middle * sizeof( EmitterRecord ) );
            if ( choose >= candidate.mCDF ) {
                low = middle + 1;
            }
            else {
                high = middle;
            }
        }

        EmitterRecord emitter = LoadBufferValue<EmitterRecord>( Parameters.mData, scene.mEmitterOffset + low * sizeof( EmitterRecord ) );
        float2 sample = Random2D( random );
        float u = sqrt( sample.x );
        light = Evaluate( emitter.mTriangle, float2( u * ( 1.0 - sample.y ), u * sample.y ) );
        float3 delta = light.mPosition - s.mPosition;
        float distance2 = dot( delta, delta );
        if ( distance2 <= 0.0 ) {
            return float3( 0.0 );
        }
        direction = delta / sqrt( distance2 );
        float cosine = dot( light.mGeometricNormal, -direction );
        if ( ( !light.mEmissionTwoSided && cosine <= 0.0 ) || abs( cosine ) <= 0.0 ||
             ( light.mAlphaMode == 1 && light.mAlpha < light.mCutoff ) ) {
            return float3( 0.0 );
        }
        emission = light.mEmission * ( light.mAlphaMode == 2 ? light.mAlpha : 1.0 );
        lightPDF = probability * emitter.mProbability * distance2 / ( emitter.mArea * abs( cosine ) );
    }

    float bsdfPDF;
    float3 value = EvaluateBSDF( s, view, direction, bsdfPDF, lobes );
    if ( lightPDF <= 0.0 || bsdfPDF <= 0.0 || !any( emission > 0.0 ) ) {
        return float3( 0.0 );
    }
    float3 transmittance = !shadows ? float3( 1 ) : environment ? ShadowTransmittance( s, direction, RAY_MAX_DISTANCE, media, random )
                                                                : ShadowTransmittanceTo( s, Offset( light, s.mPosition - light.mPosition ), media, random );
    return value * emission * transmittance * ScatteringCosine( s, direction ) * PowerWeight( lightPDF, bsdfPDF ) / lightPDF;
}
