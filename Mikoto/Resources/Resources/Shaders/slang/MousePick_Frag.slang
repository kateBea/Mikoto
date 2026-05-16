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

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"
#include "base/Camera_Helpers.slang"
#include "base/Lighting_Helpers.slang"

struct FSOutput {
    uint ObjectID : SV_Target0;
};

struct FSInput {
    uint ObjectID : ATTRIBUTE0;
    float4 ScreenPos : SV_Position;
};

MKT_DECLARE_STRUCTURED_SRV Buffers;

[shader("fragment")]
FSOutput main(FSInput input) {
    FSOutput o;

    o.ObjectID = input.ObjectID;

    return o;
}