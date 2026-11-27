// Adapted from Wicked Engine, Copyright (c) 2026 Turánszki János (MIT).
// Pinned source: 0208210a27b6dd05cf4684476c40294d29594497.
// License: LICENSE.txt beside this file. Adapted sources: globals.hlsli,
// oceanSurfacePS.hlsl and oceanSurfaceVS.hlsl from the pinned repository:
// https://github.com/turanszkij/WickedEngine/tree/0208210a27b6dd05cf4684476c40294d29594497/WickedEngine/shaders
// Mikoto supplies time, RHI bindings, projected grids, depth validation, persistent
// coast foam, pixel-footprint filtering and atmospheric haze. Caustics use visible
// water receivers, not Wicked's transparent shadow-atlas infrastructure.

// Adapted Wicked Engine water-noise, caustic and foam functions. Keep the upstream attribution
// below with these helpers; Mikoto supplies the world coordinates and optical scale at the call
// site.

#pragma once

// https://www.shadertoy.com/view/Msf3WH
float2 WickedHashSimplex2D(float2 p) // replace this by something better
{
	p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
	return -1.0 + 2.0 * frac(sin(p) * 43758.5453123);
}
float WickedSimplex2D(in float2 p)
{
	const float K1 = 0.366025404; // (sqrt(3)-1)/2;
	const float K2 = 0.211324865; // (3-sqrt(3))/6;

	float2  i = floor(p + (p.x + p.y) * K1);
	float2  a = p - i + (i.x + i.y) * K2;
	float m = step(a.y, a.x);
	float2  o = float2(m, 1.0 - m);
	float2  b = a - o + K2;
	float2  c = a - 1.0 + 2.0 * K2;
	float3  h = max(0.5 - float3(dot(a, a), dot(b, b), dot(c, c)), 0.0);
	float3  n = h * h * h * h * float3(dot(a, WickedHashSimplex2D(i + 0.0)), dot(b, WickedHashSimplex2D(i + o)), dot(c, WickedHashSimplex2D(i + 1.0)));
	return dot(n, 70.0.xxx);
}

// Based on: https://www.shadertoy.com/view/MslGD8
float2 WickedHashVoronoi(float2 p)
{
	//p = mod(p, 4.0); // tile
	p = float2(dot(p, float2(127.1, 311.7)),
			 dot(p, float2(269.5, 183.3)));
	return frac(sin(p) * 18.5453);
}

// return distance, and cell id
float2 WickedVoronoi(in float2 x, in float seed)
{
	float2 n = floor(x);
	float2 f = frac(x);

	float3 m = 8.0;
	for (int j = -1; j <= 1; j++)
		for (int i = -1; i <= 1; i++)
		{
			float2 g = float2(float(i), float(j));
			float2 o = WickedHashVoronoi(n + g);
			//float2  r = g - f + o;
			float2 r = g - f + (0.5 + 0.5 * sin(seed + 6.2831 * o));
			float d = dot(r, r);
			if (d < m.x)
				m = float3(d, o);
		}

	return float2(sqrt(m.x), m.y + m.z);
}

// Caustic pattern from: https://www.shadertoy.com/view/XtKfRG
inline float WickedCausticPattern(float2 uv, float time)
{
	float3 k = float3(uv, time);
	float3x3 m = float3x3(-2, -1, 2, 3, -2, 1, 1, 2, 2);
	float3 a = mul(k, m) * 0.5;
	float3 b = mul(a, m) * 0.4;
	float3 c = mul(b, m) * 0.3;
	return pow(min(min(length(0.5 - frac(a)), length(0.5 - frac(b))), length(0.5 - frac(c))), 7) * 25.;
}
inline float3 WickedCaustics(float2 uv, float time)
{
	const float2 chromatic_offset = 1.0 / 256.0 * 8; // tweaked to old caustic shader that ran at 256x256 res
	return float3(
		WickedCausticPattern(uv + float2(0, 0), time),
		WickedCausticPattern(uv + float2(chromatic_offset.x, 0), time),
		WickedCausticPattern(uv + float2(0, chromatic_offset.y), time)
	);
}


// oceanSurfacePS.hlsl's three-octave simplex/Voronoi foam breakup, with explicit
// time and footprint filtering so unresolved distant octaves do not sparkle.
float WickedWaterFoam( float coverage, float2 position, float time, float footprint ) {
    float simplex = 0;
    float voronoi = 0;
    for ( uint octave = 0; octave < 3; ++octave ) {
        float frequency = exp2( float( octave ) );
        float visible = 1 - smoothstep( .25, 1.0, footprint * frequency );
        float2 p = octave == 2 ? position.yx : position;
        simplex += smoothstep( 0, .8, WickedSimplex2D( p * frequency + ( octave == 2 ? -time * 2 : time ) ) ) * visible;
        voronoi += smoothstep( .5, .8, WickedVoronoi( position * frequency, time ).x ) * visible;
    }
    float foam = smoothstep( .5, .6, saturate( coverage + .1 ) );
    foam += voronoi * simplex * coverage;
    return saturate( foam * 2 );
}
