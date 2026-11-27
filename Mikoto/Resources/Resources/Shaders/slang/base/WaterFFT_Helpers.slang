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

// Ocean simulation layout, complex multiplication and FFT addressing. The spectrum, butterfly
// and gradient stages share the same flattened field indexing.

#pragma once

// Keep the workgroup size and patch resolution in sync with WaterSimulationModule.cc.
static const uint kWaterFftResolution = 256;
static const uint kWaterFftLogResolution = 8;
static const uint kWaterFftMask = kWaterFftResolution - 1;
static const uint kWaterFftTile = 8;
static const float kWaterFftTau = 6.28318530718;

uint WaterFftIndex( uint2 coordinate ) {
    return coordinate.y * kWaterFftResolution + coordinate.x;
}

float2 MultiplyWaterComplex( float2 a, float2 b ) {
    return float2( a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x );
}
