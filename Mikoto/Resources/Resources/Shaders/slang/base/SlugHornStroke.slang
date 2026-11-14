// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.
#pragma once

// A flat copy of SlugHorn's original curves retains horizontal edges omitted
// by scanline coverage. Reuse Mikoto's closest-quadratic-point solve for
// the existing rounded outline feature, without introducing an MSDF dependency.
float SlugHornStrokeCoverage( Texture2D curves, uint first, uint count,
                              float2 position, float radius ) {
    float2 dx = ddx( position );
    float2 dy = ddy( position );
    float reach = radius + max( length( abs( dx ) + abs( dy ) ), 1e-7 );
    float2 nearest = float2( reach, 0 );

    uint width, height;
    curves.GetDimensions( width, height );
    for ( uint i = 0; i < count; ++i ) {
        uint address = first + 2 * i;
        float4 p01 = curves.Load( int3( address % width, address / width, 0 ) );
        ++address;
        float2 p2 = curves.Load( int3( address % width, address / width, 0 ) ).xy;

        float2 lower = min( min( p01.xy, p01.zw ), p2 ) - position;
        float2 upper = max( max( p01.xy, p01.zw ), p2 ) - position;
        float2 boxDistance = max( max( lower, -upper ), float2( 0 ) );
        if ( dot( boxDistance, boxDistance ) > dot( nearest, nearest ) ) {
            continue;
        }
        float2 candidate = SlugNearestCurve( p01.xy - position, p01.zw - position, p2 - position );
        if ( dot( candidate, candidate ) < dot( nearest, nearest ) ) {
            nearest = candidate;
        }
    }

    float distance = length( nearest );
    float2 normal = distance > 1e-7 ? nearest / distance : float2( 1, 0 );
    float aa = max( abs( dot( normal, dx ) ) + abs( dot( normal, dy ) ), 1e-7 );
    return saturate( 0.5 + ( radius - distance ) / aa );
}
