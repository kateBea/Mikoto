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

// Classify surface blend modes for coverage handling. Depth, picking and color passes use the
// same translucent/cutout classification.

#pragma once

// Keep alpha rounding consistent with the forward renderer's depth and color passes.
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#alpha-coverage
bool IsTranslucentBlend( float alpha, bool blend ) {
    static const float kOpaqueAlpha = 1.0 - 1e-6;
    return blend && alpha < kOpaqueAlpha;
}
