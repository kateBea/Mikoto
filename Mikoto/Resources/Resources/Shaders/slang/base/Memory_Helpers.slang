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

static const uint kArrayMaxSize = 10;

#ifdef MKT_SUPPORT_D3D11
        #define MKT_DECLARE_CONSTANT_BUFFERS DECL_BINDING(MKT_CONSTANT_BUFFER_BINDING, MKT_DEFAULT_REGISTER_SPACE) ByteAddressBuffer[kArrayMaxSize]
        #define MKT_DECLARE_STRUCTURED_BUFFERS DECL_BINDING(MKT_STRUCTURED_BUFFER_BINDING, MKT_DEFAULT_REGISTER_SPACE) ByteAddressBuffer[kArrayMaxSize]

        #define MKT_DECLARE_RW_CONSTANT_BUFFERS DECL_BINDING(MKT_RW_CONSTANT_BUFFER_BINDING, MKT_DEFAULT_REGISTER_SPACE) RWByteAddressBuffer[kArrayMaxSize]
        #define MKT_DECLARE_RW_STRUCTURED_BUFFERS DECL_BINDING(MKT_RW_STRUCTURED_BUFFER_BINDING, MKT_DEFAULT_REGISTER_SPACE) RWByteAddressBuffer[kArrayMaxSize]

        #define MKT_DECLARE_SAMPLERS DECL_BINDING(MKT_SAMPLER_BINDING, MKT_DEFAULT_REGISTER_SPACE) SamplerState[kArrayMaxSize]
        #define MKT_DECLARE_TEXTURE_2D DECL_BINDING(MKT_TEXTURE_2D_BINDING, MKT_DEFAULT_REGISTER_SPACE) Texture2D[kArrayMaxSize]
        #define MKT_DECLARE_TEXTURES_CUBE DECL_BINDING(MKT_TEXTURE_CUBE_BINDING, MKT_DEFAULT_REGISTER_SPACE) TextureCube[kArrayMaxSize]
#else
    #define MKT_DECLARE_CONSTANT_BUFFERS DECL_BINDING(MKT_CONSTANT_BUFFER_BINDING, MKT_DEFAULT_REGISTER_SPACE) ByteAddressBuffer[]
    #define MKT_DECLARE_STRUCTURED_BUFFERS DECL_BINDING(MKT_STRUCTURED_BUFFER_BINDING, MKT_DEFAULT_REGISTER_SPACE) ByteAddressBuffer[]

    #define MKT_DECLARE_RW_CONSTANT_BUFFERS DECL_BINDING(MKT_RW_CONSTANT_BUFFER_BINDING, MKT_DEFAULT_REGISTER_SPACE) RWByteAddressBuffer[]
    #define MKT_DECLARE_RW_STRUCTURED_BUFFERS DECL_BINDING(MKT_RW_STRUCTURED_BUFFER_BINDING, MKT_DEFAULT_REGISTER_SPACE) RWByteAddressBuffer[]

    #define MKT_DECLARE_SAMPLERS DECL_BINDING(MKT_SAMPLER_BINDING, MKT_DEFAULT_REGISTER_SPACE) SamplerState[]
    #define MKT_DECLARE_TEXTURE_2D DECL_BINDING(MKT_TEXTURE_2D_BINDING, MKT_DEFAULT_REGISTER_SPACE) Texture2D[]
    #define MKT_DECLARE_TEXTURES_CUBE DECL_BINDING(MKT_TEXTURE_CUBE_BINDING, MKT_DEFAULT_REGISTER_SPACE) TextureCube[]

    #define MKT_DECLARE_ACCEL_STRUCTURES DECL_BINDING(MKT_ACCELERATION_STRUCTURE_BINDING, MKT_DEFAULT_REGISTER_SPACE) TextureCube[]
#endif

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