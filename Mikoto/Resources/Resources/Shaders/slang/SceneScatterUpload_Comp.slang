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

// Scatter uploaded byte ranges into their destination scene buffers. The CPU-provided copy
// records describe source offsets, destination addresses and copy sizes.

#include "base/Memory_Helpers.slang"

// Matches the CPU scatter-upload dispatch divisor.
static const uint kSceneScatterGroupSize = 64;

struct Parameters {
    Pointer mSource;
    Pointer mDestination;
    uint mCount;
    uint mWords;
};
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

[shader( "compute" )]
[numthreads( kSceneScatterGroupSize, 1, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint word = thread.x;
    if ( word >= parameters.mCount * parameters.mWords ) {
        return;
    }

    uint record = word / parameters.mWords;
    uint slot = LoadBufferValue<uint>( parameters.mSource, record * 4 );
    uint value = LoadBufferValue<uint>( parameters.mSource, ( parameters.mCount + word ) * 4 );

    StoreBufferValue<uint>( parameters.mDestination, ( slot * parameters.mWords + word % parameters.mWords ) * 4, value );
}
