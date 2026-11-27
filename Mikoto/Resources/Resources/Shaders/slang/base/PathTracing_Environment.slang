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

// Evaluate background radiance from atmosphere or the environment map. World-to-planet
// conversion and direction mapping match the scene environment settings.

#pragma once

// Sample the scene atmosphere, environment faces or panorama in world space.

bool HasAtmosphere() {
    return Effects().mControl.x != 0.0;
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
        float3 observer = AtmosWorldOrigin( a, Parameters.mCamera.xyz );
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
    if ( SceneData().mHasFaces != 0 ) {
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
        return max( MaterialMap( LoadBufferValue<uint>( Parameters.mData, SceneData().mFaceOffset + face * 4 ) )
                        .SampleLevel( GetAtmosphereSampler(), uv * 0.5 + 0.5, 0 )
                        .rgb,
                   0.0 ) *
               Effects().mBackground.w;
    }
    int index = SceneData().mEnvironment;
    if ( index < 0 ) {
        // Hybrid GI must not invent environment illumination when raster IBL
        // has no map. Preserve the standalone path tracer's fallback sky.
        if ( Parameters.mFrame.w == 0 ) {
            return float3( 0.0 );
        }
        return lerp( float3( 0.04, 0.05, 0.08 ), float3( 0.35, 0.5, 0.8 ), saturate( direction.y * 0.5 + 0.5 ) ) *
               Effects().mBackground.w;
    }
    float2 uv = float2( atan2( direction.z, direction.x ) / ( 2.0 * PI ) + 0.5,
        acos( clamp( direction.y, -1.0, 1.0 ) ) / PI );
    return max( Map( index, uv ).rgb, 0.0 ) * Effects().mBackground.w;
}
