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

#pragma once

#include "Base.slang"

#ifdef MKT_SUPPORT_D3D11
        #define MKT_DECLARE_CONSTANT_BUFFERS \
            ByteAddressBuffer ConstantBuffers[kArrayMaxSize] : register(t32)

        #define MKT_DECLARE_STRUCTURED_BUFFERS \
            ByteAddressBuffer StructuredBuffers[kArrayMaxSize] : register(t32)

        #define MKT_DECLARE_RW_CONSTANT_BUFFERS \
            RWByteAddressBuffer RWConstantBuffers[kArrayMaxSize] : register(u0)

        #define MKT_DECLARE_RW_STRUCTURED_BUFFERS \
            RWByteAddressBuffer sBuffers[kArrayMaxSize] : register(u0)

        #define MKT_DECLARE_TEXTURE_2D \
            Texture2D Texture2Ds[kArrayMaxSize] : register(t0)

        #define MKT_DECLARE_TEXTURES_CUBE \
            TextureCube TextureCubes[kArrayMaxSize] : register(t16)

        #define MKT_DECLARE_SAMPLERS \
            SamplerState Samplers[kArrayMaxSize] : register(s0)
#else
    // TODO: these 2 will be removed, we will declare a single binding for a constant buffers maybe a some so the user can push few different constant buffers
    //#define MKT_DECLARE_CONSTANT_BUFFERS(binding, type) DECL_BINDING(MKT_CONSTANT_BUFFER_BINDING, 3) ConstantBuffer<Type>  // leave them at set 3 for instance
    #define MKT_DECLARE_STRUCTURED_SRV DECL_BINDING(MKT_STRUCTURED_SRV_BINDING, MKT_DEFAULT_REGISTER_SPACE) ByteAddressBuffer[]
    #define MKT_DECLARE_STRUCTURED_UAV DECL_BINDING(MKT_STRUCTURED_UAV_BINDING, MKT_DEFAULT_REGISTER_SPACE) RWByteAddressBuffer[]

    #define MKT_DECLARE_SAMPLERS DECL_BINDING(MKT_SAMPLER_BINDING, MKT_DEFAULT_REGISTER_SPACE) SamplerState[]

    #define MKT_DECLARE_TEXTURE_SRV(DIMENSION) DECL_BINDING(MKT_TEXTURE_SRV_BINDING, MKT_DEFAULT_REGISTER_SPACE) DIMENSION[]
    #define MKT_DECLARE_TEXTURE_UAV(DIMENSION) DECL_BINDING(MKT_TEXTURE_UAV_BINDING, MKT_DEFAULT_REGISTER_SPACE) DIMENSION[]

    #define MKT_DECLARE_ACCEL_STRUCTURES DECL_BINDING(MKT_ACCELERATION_STRUCTURE_BINDING, MKT_DEFAULT_REGISTER_SPACE) TextureCube[]
#endif

// Extensions
extension RWByteAddressBuffer {
    void Write<T>(uint baseOffset, uint index, T element) {
        uint offset = baseOffset + (index * sizeof(T));
        this.Store<T>(offset, element);
    }

    T Read<T>(uint baseOffset, uint index) {
        uint offset = baseOffset + (index * sizeof(T));
        return this.Load<T>(offset);
    }
}

extension ByteAddressBuffer {
    T Read<T>(uint baseOffset, uint index) {
        uint offset = baseOffset + (index * sizeof(T));
        return this.LoadByteOffset<T>(offset);
    }
}


namespace mikoto::memory {
#ifndef MKT_SUPPORT_D3D11
    T LoadData<T>( ByteAddressBuffer[] sbs, uint bufferID, uint baseOffset, uint index) {
        return sbs[bufferID].LoadByteOffset<T>(
            (baseOffset + index) * sizeof(T)
        );
    }

    void  WriteData<T>( RWByteAddressBuffer[] sbs, uint bufferID, uint baseOffset, uint index, T element) {
        sbs[bufferID].StoreByteOffset<T>((baseOffset + index) * sizeof(T), element );
        //sbs[bufferID].StoreAligned<T>((baseOffset + index) * sizeof(T), element );
    }


    T Load<T>( ByteAddressBuffer sb, uint baseOffset, uint index) {
        return sb.LoadByteOffset<T>(
            (baseOffset + index) * sizeof(T)
        );
    }

#else
    // Nee to handle properly bufferID must be a constant expression known at compile time
    T LoadData<T>( ByteAddressBuffer[] sbs, uint bufferID, uint baseOffset, uint index) {
        return sbs[bufferID].LoadByteOffset<T>(
            (baseOffset + index) * sizeof(T)
        );
    }

    // Nee to handle properly bufferID must be a constant expression known at compile time
    void  WriteData<T>( RWByteAddressBuffer[] sbs, uint bufferID, uint baseOffset, uint index, T element) {
        sbs[bufferID].StoreByteOffset<T>((baseOffset + index) * sizeof(T), element );
        //sbs[bufferID].StoreAligned<T>((baseOffset + index) * sizeof(T), element );
    }
#endif
}