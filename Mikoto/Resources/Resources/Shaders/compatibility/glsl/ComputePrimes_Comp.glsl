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

#version 430 core

// Host contract: UBO binding 0 is 16 bytes; SSBO binding 1 stores mElementCount results.
// Dispatch ( ( mElementCount + 63 ) / 64, 1, 1 ).
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

layout( std140, binding = 0 ) uniform ComputeParameters {
    uint mElementCount;
    uint mPadding0;
    uint mPadding1;
    uint mPadding2;
};

// Matches the existing compute sample's 16-byte x/y/prime/padding layout.
struct PrimeResult {
    uint mX;
    uint mY;
    uint mPrime;
    uint mPadding;
};

layout( std430, binding = 1 ) writeonly buffer PrimeResults {
    PrimeResult mValues[];
} uResults;

// Trial-division reference, adapted to skip even divisors:
// https://cp-algorithms.com/algebra/primality_tests.html#trial-division
bool IsPrime( uint value ) {
    if ( value < 2u ) {
        return false;
    }

    if ( ( value & 1u ) == 0u ) {
        return value == 2u;
    }

    // Integer division avoids overflow in divisor * divisor and floating-point rounding.
    for ( uint divisor = 3u; divisor <= value / divisor; divisor += 2u ) {
        if ( value % divisor == 0u ) {
            return false;
        }
    }

    return true;
}

void main() {
    const uint index = gl_GlobalInvocationID.x;

    // Rounded-up dispatches must leave elements outside the requested range untouched.
    if ( index >= mElementCount ) {
        return;
    }

    PrimeResult result;
    result.mX = index;
    result.mY = gl_GlobalInvocationID.y;
    result.mPrime = IsPrime( index ) ? index : 0u;
    result.mPadding = 0u;

    uResults.mValues[index] = result;
}
