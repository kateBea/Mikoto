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

#include "base/CompatibilityFrame.slang"
#include "base/CompatibilityEnvironment.slang"

[shader( "fragment" )]
float4 main( float2 clipPosition : TEXCOORD0 ) : SV_Target {
    float4 nearPoint = mul( frame.InverseSkyViewProjection, float4( clipPosition, 0.0, 1.0 ) );
    float4 farPoint = mul( frame.InverseSkyViewProjection, float4( clipPosition, 0.5, 1.0 ) );
    float3 direction = normalize( farPoint.xyz / farPoint.w - nearPoint.xyz / nearPoint.w );
    float3 color = ReadEnvironment( direction, frame.Environment.y * frame.Environment.z, frame.Environment.w );

    return EncodeSceneColor( color );
}
