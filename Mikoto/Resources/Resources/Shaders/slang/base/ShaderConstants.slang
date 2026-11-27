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

// Shared topology and sampling footprints. Dispatch values match the corresponding
// CPU frame-graph dispatch divisors; changing them requires updating those divisors too.
#pragma once

// Screen-space and volume workgroup dimensions.
static const uint kImageGroupWidth = 8;
static const uint kImageGroupHeight = 8;
static const uint kVolumeGroupSide = 4;

// Primitive expansion counts: two triangles cover one quad, eight corners bound a box.
static const uint kTriangleVertexCount = 3;
static const uint kQuadCornerCount = 4;
static const uint kVerticesPerQuad = 6;
static const uint kBoxCornerCount = 8;
static const uint kCubemapFaceCount = 6;

// Linear reconstruction visits two neighbors per axis: four in 2D, eight in 3D.
static const int kLinearFilterSide = 2;
static const uint kBilinearTapCount = 4;
static const uint kTrilinearTapCount = 8;
