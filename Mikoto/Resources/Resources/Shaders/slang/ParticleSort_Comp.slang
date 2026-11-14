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

#include "base/Particle.slang"

struct SortParameters {
    RWBufferReference<ParticleRenderData> mOutput;
    uint mCount, mStage, mStride, mClear;
};
MKT_DECLARE_PUSH_CONSTANT( SortParameters, parameters );

[shader( "compute" )]
[numthreads( 128, 1, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint index = thread.x;
    if ( index >= parameters.mCount ) {
        return;
    }
    if ( parameters.mClear != 0 ) {
        ParticleRenderData empty = ( ParticleRenderData )0;
        empty.mRotationSoftness.z = -3.402823466e+38;
        empty.mRotationSoftness.w = float( index );
        parameters.mOutput[index] = empty;
        return;
    }
    uint other = index ^ parameters.mStride;
    // One thread owns both records in each disjoint pair; the host inserts a
    // storage barrier between stages. Dead slots sort after all visible particles.
    if ( other <= index ) {
        return;
    }
    ParticleRenderData a = parameters.mOutput[index];
    ParticleRenderData b = parameters.mOutput[other];
    bool aBeforeB = a.mRotationSoftness.z != b.mRotationSoftness.z ? a.mRotationSoftness.z > b.mRotationSoftness.z
                                                                   : a.mRotationSoftness.w < b.mRotationSoftness.w;
    bool descending = ( index & parameters.mStage ) == 0;
    if ( aBeforeB != descending ) {
        parameters.mOutput[index] = b;
        parameters.mOutput[other] = a;
    }
}
