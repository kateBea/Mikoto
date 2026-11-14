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

#pragma once

// https://shader-slang.org/slang/user-guide/convenience-features
// https://docs.shader-slang.org/en/latest/external/core-module-reference/types/descriptorhandle-0a/index.html

// Shader references are always eight bytes in CPU/shared records. Vulkan uses
// a native device address. D3D12 packs an SRV descriptor + 1 in the high word
// and a byte offset in the low word; the paired UAV follows the SRV descriptor.
// Native addresses used to build acceleration structures are a separate API.
T LoadBufferValue<T>( uint64_t address, uint byteOffset ) {
#if defined( MKT_SUPPORT_D3D12 )
    ByteAddressBuffer buffer = ResourceDescriptorHeap[NonUniformResourceIndex( uint( address >> 32 ) - 1 )];
    return buffer.Load<T>( uint( address ) + byteOffset );
#else
    return ( ( T * )( address + byteOffset ) )[0];
#endif
}

T LoadRWBufferValue<T>( uint64_t address, uint byteOffset ) {
#if defined( MKT_SUPPORT_D3D12 )
    RWByteAddressBuffer buffer = ResourceDescriptorHeap[NonUniformResourceIndex( uint( address >> 32 ) )];
    return buffer.Load<T>( uint( address ) + byteOffset );
#else
    return ( ( T * )( address + byteOffset ) )[0];
#endif
}

void StoreBufferValue<T>( uint64_t address, uint byteOffset, T value ) {
#if defined( MKT_SUPPORT_D3D12 )
    RWByteAddressBuffer buffer = ResourceDescriptorHeap[NonUniformResourceIndex( uint( address >> 32 ) )];
    buffer.Store<T>( uint( address ) + byteOffset, value );
#else
    ( ( T * )( address + byteOffset ) )[0] = value;
#endif
}

struct BufferReference<T> {
    uint64_t address;

    __init( uint64_t value ) {
        address = value;
    }

    __subscript( uint index )->T {
        get {
            return LoadBufferValue<T>( address, index * sizeof( T ) );
        }
    }
};

struct RWBufferReference<T> {
    uint64_t address;

    __init( uint64_t value ) {
        address = value;
    }

    __subscript( uint index )->T {
        get {
            return LoadRWBufferValue<T>( address, index * sizeof( T ) );
        }
        [nonmutating] set {
            StoreBufferValue<T>( address, index * sizeof( T ), newValue );
        }
    }
};

#if defined( MKT_SUPPORT_D3D12 )
struct ResourceArray<T : IOpaqueDescriptor> {
    __subscript( uint index )->T {
        get {
            return getDescriptorFromHandle( DescriptorHandle<T>( uint2( NonUniformResourceIndex( index ), 0 ) ) );
        }
    }
};

struct SamplerArray {
    __subscript( uint index )->SamplerState {
        get {
            return SamplerDescriptorHeap[NonUniformResourceIndex( index )];
        }
    }
};
#endif
