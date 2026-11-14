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

#include "base/Thumbnail_Helpers.slang"
#include "base/Tonemap_Helpers.slang"
#include "base/Bloom_Helpers.slang"

[shader( "fragment" )]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    PreviewScene scene = ( ( BufferReference<PreviewScene> )preview.mScene )[0];
    float4 color = previewTextures[preview.mColor].SampleLevel( previewSamplers[preview.mClampSampler], uv, 0 );
    float3 bloom = previewTextures[preview.mBloom].SampleLevel( previewSamplers[preview.mClampSampler], uv, 0 ).rgb;
    color = BloomComposite( color, bloom, scene.mBloom.y );
    return ApplyToneMapping( max( color, 0.0 ), c_ToneMapKhronosNeutral, scene.mCamera.w, 2.2 );
}
