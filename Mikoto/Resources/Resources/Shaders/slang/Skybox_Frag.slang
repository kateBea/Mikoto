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

// Select the configured environment representation for the visible background. Original and
// prefiltered sky sampling share the uploaded exposure and display settings.

#pragma once

#include "base/Base.slang"
#include "base/Camera_Helpers.slang"
#include "base/Lighting_Helpers.slang"
#include "base/Memory_Helpers.slang"
#include "base/Tonemap_Helpers.slang"

static const uint kBGTypeSkybox = 0;
static const uint kBGTypePrefilter = 1;

struct FSInput {
    float3 mDirection : TEXCOORD0;
};

struct FSOutput {
    float4 mColor : SV_Target0;
};

struct DrawParams {
    uint mCameraBufferID;
    uint mBasicSamplerID;

    uint mVertexBufferID;
    uint mIndexBufferID;

    uint mSkyboxCubeRtID;
    uint mPrefilterCubeRtID;

    uint mBackGroundType;// Prefilter map, Original skybox, clear color

    uint mMaxMipLevel;
    float mExposure;
    float mGamma;
    float mSkyboxBlur;
};

MKT_DECLARE_PUSH_CONSTANT( DrawParams, Parameters );

MKT_DECLARE_SAMPLERS Samplers;
MKT_DECLARE_TEXTURE_SRV( TextureCube ) Textures;

[shader( "fragment" )]
FSOutput main( FSInput input ) {
    FSOutput o;

    SamplerState sampler = Samplers[Parameters.mBasicSamplerID];

    uint index = 0;
    if ( Parameters.mBackGroundType == kBGTypeSkybox ) {
        index = Parameters.mSkyboxCubeRtID;
    } else if ( Parameters.mBackGroundType == kBGTypePrefilter ) {
        index = Parameters.mPrefilterCubeRtID;
    }

    TextureCube texture = Textures[index];

    // This is just the LOD
    // https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-samplelevel
    uint width;
    uint height;
    uint levels;
    texture.GetDimensions( 0u, width, height, levels );

    // Clamp to the selected image's actual chain. Falling back to implicit
    // derivatives at the end of the slider suddenly made the sky sharp again.
    const float blur = clamp( Parameters.mSkyboxBlur, 0.0,
                              float( max( levels, 1u ) - 1u ) );
    float3 color = texture.SampleLevel( sampler, input.mDirection, blur ).rgb;

    o.mColor = float4( color, 1.0 );
    return o;
}
