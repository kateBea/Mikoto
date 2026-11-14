// Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0.

struct PSInput {
    float4 Position : SV_Position;
    float3 WorldPosition : TEXCOORD0;
    float4 Color : COLOR0;
};

[shader( "fragment" )]
float4 main( PSInput input ) : SV_Target0 {
    // Flat normals preserve the shape of ozz's bone mesh under nonuniform model scales.
    float3 normal = normalize( cross( ddx( input.WorldPosition ), ddy( input.WorldPosition ) ) );
    // ozz's sample ambient shading keeps the facets readable without scene lights.
    // https://github.com/guillaumeblanc/ozz-animation/blob/master/samples/framework/internal/shader.cc#L406
    float3 alpha = ( normal + 1 ) * 0.5f;
    float2 bottomTop = lerp( float2( 0.3f, 0.7f ), float2( 0.4f, 0.8f ), alpha.xz );
    float3 ambient = lerp( float3( bottomTop.x, 0.3f, bottomTop.x ),
        float3( bottomTop.y, 0.8f, bottomTop.y ), alpha.y );
    return float4( input.Color.rgb * ambient, 1 );
}
