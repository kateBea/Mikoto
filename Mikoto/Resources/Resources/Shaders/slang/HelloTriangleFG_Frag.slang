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

// Write the interpolated color of the frame-graph example triangle. This is a diagnostic
// graphics pass rather than a lit scene material.

#pragma once

struct FSInput {
    float4 mPosition : SV_POSITION;
    float3 mColor : COLOR;
};

[shader( "fragment" )]
float4 main( FSInput input ) : SV_Target {
    return float4( input.mColor, 1.0 );
}