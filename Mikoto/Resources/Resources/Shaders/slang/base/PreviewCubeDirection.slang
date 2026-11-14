#pragma once
float3 PreviewCubeDirection( float2 uv, uint face ) {
    // Inverse of the standard cubemap face lookup, with top-left render-target UVs.
    float2 p = uv * 2.0 - 1.0;
    if ( face == 0 ) {
        return normalize( float3( 1, -p.y, -p.x ) );
    }
    if ( face == 1 ) {
        return normalize( float3( -1, -p.y, p.x ) );
    }
    if ( face == 2 ) {
        return normalize( float3( p.x, 1, p.y ) );
    }
    if ( face == 3 ) {
        return normalize( float3( p.x, -1, -p.y ) );
    }
    if ( face == 4 ) {
        return normalize( float3( p.x, -p.y, 1 ) );
    }
    return normalize( float3( -p.x, -p.y, -1 ) );
}

