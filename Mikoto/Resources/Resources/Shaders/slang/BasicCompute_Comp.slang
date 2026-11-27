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

// Frame-graph compute example: test candidate integers for primality and write the results
// through the bound output buffer. It exercises dispatch and storage-buffer access.

#pragma once

#include "base/Base.slang"
#include "base/Memory_Helpers.slang"

using namespace mikoto;

MKT_DECLARE_STRUCTURED_UAV sBuffers;

// Matches the CPU dispatch divisor for this frame-graph example.
static const uint kPrimeTestGroupSize = 64;

bool IsPrime( uint n ) {
    if ( n < 2 ) {
        return false;
    }
    if ( n == 2 ) {
        return true;
    }
    if ( ( n & 1 ) == 0 ) {
        return false;
    }

    uint limit = uint( floor( sqrt( float( n ) ) ) );

    for ( uint d = 3; d <= limit; d += 2 ) {
        if ( n % d == 0 ) {
            return false;
        }
    }
    return true;
}

struct MyStruct {
    int mX;
    int mY;
    int mPrime;
    int mPad;
};

struct ShaderParameters {
    Pointer mBufferID;
    ArraySize mBufferSize;
};

MKT_DECLARE_PUSH_CONSTANT( ShaderParameters, uParams );

[numthreads( kPrimeTestGroupSize, 1, 1 )]
[shader( "compute" )]
void main( uint3 gid : SV_DispatchThreadID ) {
    MyStruct s = ( MyStruct )0;
    s.mX = gid.x;
    s.mY = gid.y;

    uint idx = gid.x;
    if ( idx >= uParams.mBufferSize ) {
        return;
    }

    uint value = 0;
    if ( IsPrime( idx ) ) {
        value = idx;
    }

    s.mPrime = value;

    RWBufferReference<MyStruct> buf = ( RWBufferReference<MyStruct> )uParams.mBufferID;
    buf[idx] = s;
}
