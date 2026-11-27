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

// Render layered procedural noise into the generated-noise target. Increasing-frequency octaves
// add detail while their amplitudes decay.

#include "base/Noise_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( NoiseParameters, parameters );

[shader("fragment")]
float4 main( float2 uv : TEXCOORD0 ) : SV_Target0 {
    FBMSettings settings;
    settings.mFrequency = parameters.mBaseFrequency;
    settings.mAmplitude = 1.0f;
    settings.mPersistence = parameters.mPersistence;
    settings.mLacunarity = parameters.mLacunarity;
    settings.mOctaves = parameters.mOctaves;
    float noise = FBM( uv, settings );
    float height = saturate( noise * 0.5f + 0.5f );
    return float4( height, height, height, 1.0f );
}

