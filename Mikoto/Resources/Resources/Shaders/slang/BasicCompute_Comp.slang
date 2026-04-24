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

MKT_DECLARE_RW_STRUCTURED_BUFFERS sBuffers;

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
    int z;
};

struct ShaderParameters {
    int BufferID;
};

MKT_DECLARE_PUSH_CONSTANT
ShaderParameters uParams;

[numthreads(64, 1, 1)]
[shader("compute")]
void main(uint3 gid: SV_DispatchThreadID) {
    MyStruct s;
    s.x = 1;
    s.y = 1;
    s.z = 1;

#ifdef MKT_SUPPORT_D3D11
    // Nee to handle properly bufferID must be a constant expression known at compile time
    memory::WriteData<MyStruct>(sBuffers, 0, 0, 5, s);
#else
    memory::WriteData<MyStruct>(sBuffers, uParams.BufferID, 0, 5, s);
#endif

}