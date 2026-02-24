//    Copyright 2025 ケイト
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

#include "base/base.slang"

[[vk::binding(0, DYNAMIC_SET_INDEX)]]
RWStructuredBuffer<uint> OutputBuffer;

bool IsPrime(uint n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if ((n & 1) == 0) return false;

    uint limit = uint(floor(sqrt(float(n))));

    for (uint d = 3; d <= limit; d += 2) {
        if (n % d == 0) return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 gid: SV_DispatchThreadID) {
    uint idx = gid.x;

    if (IsPrime(idx))
        OutputBuffer[idx] = idx;
    else
        OutputBuffer[idx] = 0;
}