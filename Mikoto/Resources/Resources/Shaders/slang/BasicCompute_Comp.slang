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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

using namespace mikoto;

MKT_DECLARE_STRUCTURED_UAV sBuffers;

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

struct MyStruct {
    int x;
    int y;
    int prime;
    int _pad;
};

struct ShaderParameters {
    int BufferID;
};

MKT_DECLARE_PUSH_CONSTANT(ShaderParameters) uParams;

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 gid: SV_DispatchThreadID) {
    MyStruct s;
    s.x = gid.x;
    s.y = gid.y;

    uint idx = gid.x;

    uint value = 0;
    if (IsPrime(idx))
        value = idx;

    s.prime = value;

    RWByteAddressBuffer buf = sBuffers[uParams.BufferID];
    buf.Write<MyStruct>(0, idx, s);
}