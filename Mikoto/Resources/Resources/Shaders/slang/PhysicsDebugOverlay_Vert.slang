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

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Memory_Helpers.slang"

struct DebugVertex {
    float4 position;
    float4 color;
};

struct Parameters {
    Pointer lines;
    Pointer camera;
    float2 viewport;
    float lineWidth;
    float padding;
};
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
};

[shader( "vertex" )]
VSOutput main( uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID ) {
    BufferReference<DebugVertex> vertices = ( BufferReference<DebugVertex> )parameters.lines;
    BufferReference<CameraInfo> camera = ( BufferReference<CameraInfo> )parameters.camera;

    VSOutput output;

    DebugVertex start = vertices[instanceID * 2];
    DebugVertex end = vertices[instanceID * 2 + 1];
    float4 a = mul( camera[0].mProjection, mul( camera[0].mViewMatrix, float4( start.position.xyz, 1 ) ) );
    float4 b = mul( camera[0].mProjection, mul( camera[0].mViewMatrix, float4( end.position.xyz, 1 ) ) );
    output.color = float4( start.color.rgb, 1 );

    // Clip before perspective division and line expansion. Camera volumes and
    // large light ranges can cross any of the six planes, including the eye.
    static const float4 planes[6] = { float4( 1, 0, 0, 1 ),  float4( -1, 0, 0, 1 ), float4( 0, 1, 0, 1 ),
                                      float4( 0, -1, 0, 1 ), float4( 0, 0, 1, 0 ),  float4( 0, 0, -1, 1 ) };
    for ( uint plane = 0; plane < 6; ++plane ) {
        float da = dot( a, planes[plane] );
        float db = dot( b, planes[plane] );
        if ( da < 0 && db < 0 ) {
            output.position = float4( 0, 0, -1, 1 );
            return output;
        }
        if ( ( da < 0 ) != ( db < 0 ) ) {
            float4 intersection = lerp( a, b, da / ( da - db ) );
            if ( da < 0 ) {
                a = intersection;
            } else {
                b = intersection;
            }
        }
    }
    if ( a.w <= 0 || b.w <= 0 ) {
        output.position = float4( 0, 0, -1, 1 );
        return output;
    }
    float2 delta = ( b.xy / b.w - a.xy / a.w ) * parameters.viewport;
    float2 normal = float2( -delta.y, delta.x ) / max( length( delta ), 1e-6 );
    static const uint corners[6] = { 0, 1, 2, 0, 2, 3 };
    uint corner = corners[vertexID];
    bool atEnd = corner == 1 || corner == 2;
    float side = corner < 2 ? -1 : 1;
    output.position = atEnd ? b : a;
    output.position.xy += normal * side * max( parameters.lineWidth, 1 ) / parameters.viewport * output.position.w;

    // A small clip-space bias avoids surface fighting without a constant NDC
    // offset, which would pull distant lines through substantial scene geometry.
    output.position.z -= 0.00001;
    output.color = float4( atEnd ? end.color.rgb : start.color.rgb, 1 );

    return output;
}
