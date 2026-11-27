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

// Terrain draw parameters and debug-view classification. Vertex and fragment stages share the
// same terrain/surface resource addresses.

#pragma once

#include "Memory_Helpers.slang"

// Matches the 48-byte CPU elevation palette; RGB colors are padded to float4.
struct TerrainElevationColors {
    float4 mLow;
    float4 mMiddle;
    float4 mHigh;
};

// Matches the CPU flag bits in TerrainGenerationModule.cc.
enum class TerrainDebugFlag : uint {
    eNone = 0,
    eGridLines = 1u << 0,
    eVertices = 1u << 1
};

// Matches TerrainParameters in TerrainGenerationModule.cc.
// Sharing this layout keeps the vertex and fragment stages in agreement.
struct TerrainParameters {
    float4x4 mWorld;
    Pointer mVertices;
    Pointer mIndices;
    Pointer mCamera;
    uint mFirstVertex;
    uint mFirstIndex;
    uint mColumns;
    uint mDebugFlags;
    BufferReference<TerrainElevationColors> mElevationColors;
    Pointer mSurface;
};

MKT_DECLARE_PUSH_CONSTANT( TerrainParameters, parameters );

bool IsDebugView( TerrainDebugFlag view ) {
    return ( parameters.mDebugFlags & uint( view ) ) != 0u;
}
