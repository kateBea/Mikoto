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

// Keep the SDK path in a separate shader variant. Disabling NRD then restores
// the original transport kernel's register footprint as well as its accumulation.

// Enable the NRD signal-producing variant of the shared ray-generation shader. Surface-guide
// generation and radiance demodulation are defined in the denoiser helpers.

#define MKT_NRD_TRACE 1
#include "PathTracing_RayGen.slang"
