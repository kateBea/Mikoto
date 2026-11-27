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

// Evaluate distance from the nearest silhouette boundary and blend the configured outline over
// scene color. The MSAA variant reads the matching multisample scene inputs.

#pragma once

#include "base/OutlineComposite_Helpers.slang"

[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0, float4 position : SV_Position ) : SV_Target0 {
    int2 pixel = int2( position.xy );
    // This is the article's interior stencil test expressed through the module's own mask.
    if ( textures[parameters.mInterior].Load( int3( pixel, 0 ) ).r > 0.0f ) {
        discard;
    }
    return EvaluateOutline( pixel, uv );
}
