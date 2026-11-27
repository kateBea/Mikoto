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

// Limit bloom colours to the RGBA16F target range.

// Shared HDR bloom filtering, thresholding and composition. Highlight limiting keeps very
// bright samples from dominating the downsample pyramid.

#pragma once

float3 BloomClampColor( float3 value ) {
    return clamp( value, 0.0, 65504.0 );
}

// Extract the colour that contributes to bloom using the brightest RGB channel.
// A soft knee fades in the contribution around the threshold; scaling all channels
// equally preserves the input colour. Apply before combining taps.
// Extract the highlight contribution using the configured threshold/soft knee. Clamp to the
// finite HDR target range before the downsample combines neighboring highlights.
float3 BloomPrefilter( float3 value, float threshold ) {
    value = BloomClampColor( value );
    float brightness = max( value.x, max( value.y, value.z ) );
    float knee = max( threshold * 0.5, 0.00001 );
    float soft = clamp( brightness - threshold + knee, 0.0, 2.0 * knee );
    soft = soft * soft / ( 4.0 * knee );
    float contribution = max( brightness - threshold, soft ) / max( brightness, 0.00001 );
    return value * contribution;
}

// Sample mip level zero and limit the colour to the HDR target range.
// Enable prefilter for the first downsample to extract bloom; later levels only
// filter the bloom already extracted, without applying the threshold again.
float3 BloomSample( Texture2D source, SamplerState sampler, float2 uv, float threshold, bool prefilter ) {
    float3 color = BloomClampColor( source.SampleLevel( sampler, uv, 0 ).rgb );
    return prefilter ? BloomPrefilter( color, threshold ) : color;
}

// Combine a multi-tap neighborhood into the next bloom level. The wider filter footprint
// reduces aliasing when bright structures shrink below one destination texel.
float3 BloomDownsample( Texture2D source, SamplerState sampler, float2 uv, float threshold, bool prefilter ) {
    uint width;
    uint height;
    source.GetDimensions( width, height );

    float2 srcTexelSize = 1.0 / float2( max( width, 1u ), max( height, 1u ) );
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Ref: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom
    // Take 13 samples around the current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = BloomSample( source, sampler, uv + float2( -2 * x,  2 * y ), threshold, prefilter );
    float3 b = BloomSample( source, sampler, uv + float2(      0,  2 * y ), threshold, prefilter );
    float3 c = BloomSample( source, sampler, uv + float2(  2 * x,  2 * y ), threshold, prefilter );

    float3 d = BloomSample( source, sampler, uv + float2( -2 * x,      0 ), threshold, prefilter );
    float3 e = BloomSample( source, sampler, uv,                           threshold, prefilter );
    float3 f = BloomSample( source, sampler, uv + float2(  2 * x,      0 ), threshold, prefilter );

    float3 g = BloomSample( source, sampler, uv + float2( -2 * x, -2 * y ), threshold, prefilter );
    float3 h = BloomSample( source, sampler, uv + float2(      0, -2 * y ), threshold, prefilter );
    float3 i = BloomSample( source, sampler, uv + float2(  2 * x, -2 * y ), threshold, prefilter );

    float3 j = BloomSample( source, sampler, uv + float2( -x,  y ), threshold, prefilter );
    float3 k = BloomSample( source, sampler, uv + float2(  x,  y ), threshold, prefilter );
    float3 l = BloomSample( source, sampler, uv + float2( -x, -y ), threshold, prefilter );
    float3 m = BloomSample( source, sampler, uv + float2(  x, -y ), threshold, prefilter );

    // Group the overlapping regions into normalized weights:
    // center: 0.125, outer corners: 4 * 0.03125,
    // outer edges: 4 * 0.0625, inner samples: 4 * 0.125.
    // Total weight = 1.0, preserving uniform brightness.
    float3 result = e * 0.125;
    result += ( a + c + g + i ) * 0.03125;
    result += ( b + d + f + h ) * 0.0625;
    result += ( j + k + l + m ) * 0.125;

    return BloomClampColor( result );
}

// Filter the smaller bloom level back into the current image scale. Reconstruction spreads the
// glow while retaining the accumulated larger-radius contribution.
float3 BloomUpsample( Texture2D lower, Texture2D detail, SamplerState sampler, float2 uv, float scatter, float radius ) {
    uint width;
    uint height;
    lower.GetDimensions( width, height );

    // Radius remains in source texels, matching the renderer's radius control.
    float2 srcTexelSize = 1.0 / float2( max( width, 1u ), max( height, 1u ) );
    float x = srcTexelSize.x * radius;
    float y = srcTexelSize.y * radius;

    // Ref: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom
    // Take 9 samples around the current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = BloomClampColor( lower.SampleLevel( sampler, uv + float2( -x,  y ), 0 ).rgb );
    float3 b = BloomClampColor( lower.SampleLevel( sampler, uv + float2(  0,  y ), 0 ).rgb );
    float3 c = BloomClampColor( lower.SampleLevel( sampler, uv + float2(  x,  y ), 0 ).rgb );

    float3 d = BloomClampColor( lower.SampleLevel( sampler, uv + float2( -x,  0 ), 0 ).rgb );
    float3 e = BloomClampColor( lower.SampleLevel( sampler, uv,                   0 ).rgb );
    float3 f = BloomClampColor( lower.SampleLevel( sampler, uv + float2(  x,  0 ), 0 ).rgb );

    float3 g = BloomClampColor( lower.SampleLevel( sampler, uv + float2( -x, -y ), 0 ).rgb );
    float3 h = BloomClampColor( lower.SampleLevel( sampler, uv + float2(  0, -y ), 0 ).rgb );
    float3 i = BloomClampColor( lower.SampleLevel( sampler, uv + float2(  x, -y ), 0 ).rgb );

    // Apply the 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    float3 blurred = e * 4.0;
    blurred += ( b + d + f + h ) * 2.0;
    blurred += ( a + c + g + i );
    blurred *= 1.0 / 16.0;

    // Retain high-frequency detail without multiplying uniform brightness by
    // the number of pyramid levels. Intensity is applied once, in tonemap.
    return BloomClampColor( lerp( BloomClampColor( detail.SampleLevel( sampler, uv, 0 ).rgb ), blurred, saturate( scatter ) ) );
}

float4 BloomComposite( float4 scene, float3 bloom, float intensity ) {
    return float4( scene.rgb + BloomClampColor( bloom ) * intensity, scene.a );
}
