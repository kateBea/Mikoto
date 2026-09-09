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
#if defined(MKT_SUPPORT_D3D12)

#define MKT_DECLARE_STRUCTURED_SRV                \
    ByteAddressBuffer MKT_STRUCTURED_SRVS[]       \
        : register(t##MKT_STRUCTURED_SRV_BINDING, \
                   space##MKT_DEFAULT_REGISTER_SPACE)

#define MKT_DECLARE_STRUCTURED_UAV                \
    RWByteAddressBuffer MKT_STRUCTURED_UAVS[]     \
        : register(u##MKT_STRUCTURED_UAV_BINDING, \
                   space##MKT_DEFAULT_REGISTER_SPACE)

#define MKT_DECLARE_BUFFER_DEVICE_ADDRESS                  \
    StructuredBuffer<uint64_t> MKT_BUFFER_DEVICE_ADDRESSES \
        : register(t##MKT_BUFFER_DEVICE_ADDRESS_BINDING,   \
                   space##MKT_DEFAULT_REGISTER_SPACE)

#define MKT_DECLARE_SAMPLERS               \
    SamplerState MKT_SAMPLERS[]            \
        : register(s##MKT_SAMPLER_BINDING, \
                   space##MKT_DEFAULT_REGISTER_SPACE)

#define MKT_DECLARE_TEXTURE_SRV(DIMENSION)     \
    DIMENSION MKT_TEXTURE_SRVS[]               \
        : register(t##MKT_TEXTURE_SRV_BINDING, \
                   space##MKT_DEFAULT_REGISTER_SPACE)

#define MKT_DECLARE_TEXTURE_UAV(DIMENSION)     \
    DIMENSION MKT_TEXTURE_UAVS[]               \
        : register(u##MKT_TEXTURE_UAV_BINDING, \
                   space##MKT_DEFAULT_REGISTER_SPACE)

#define MKT_DECLARE_ACCEL_STRUCTURES                       \
    RaytracingAccelerationStructure MKT_ACCEL_STRUCTURES[] \
        : register(t##MKT_ACCELERATION_STRUCTURE_BINDING,  \
                   space##MKT_DEFAULT_REGISTER_SPACE)

#elif defined(MKT_SUPPORT_VULKAN)

#define MKT_DECLARE_STRUCTURED_SRV              \
    [[vk::binding(MKT_STRUCTURED_SRV_BINDING,   \
                  MKT_DEFAULT_REGISTER_SPACE)]] \
    ByteAddressBuffer MKT_STRUCTURED_SRVS[]

#define MKT_DECLARE_STRUCTURED_UAV              \
    [[vk::binding(MKT_STRUCTURED_UAV_BINDING,   \
                  MKT_DEFAULT_REGISTER_SPACE)]] \
    RWByteAddressBuffer MKT_STRUCTURED_UAVS[]

#define MKT_DECLARE_BUFFER_DEVICE_ADDRESS            \
    [[vk::binding(MKT_BUFFER_DEVICE_ADDRESS_BINDING, \
                  MKT_DEFAULT_REGISTER_SPACE)]]      \
    StructuredBuffer<uint64_t> MKT_BUFFER_DEVICE_ADDRESSES

#define MKT_DECLARE_SAMPLERS                    \
    [[vk::binding(MKT_SAMPLER_BINDING,          \
                  MKT_DEFAULT_REGISTER_SPACE)]] \
    SamplerState MKT_SAMPLERS[]

#define MKT_DECLARE_TEXTURE_SRV(DIMENSION)      \
    [[vk::binding(MKT_TEXTURE_SRV_BINDING,      \
                  MKT_DEFAULT_REGISTER_SPACE)]] \
    DIMENSION MKT_TEXTURE_SRVS[]

#define MKT_DECLARE_TEXTURE_UAV(DIMENSION)      \
    [[vk::binding(MKT_TEXTURE_UAV_BINDING,      \
                  MKT_DEFAULT_REGISTER_SPACE)]] \
    DIMENSION MKT_TEXTURE_UAVS[]

#define MKT_DECLARE_ACCEL_STRUCTURES                  \
    [[vk::binding(MKT_ACCELERATION_STRUCTURE_BINDING, \
                  MKT_DEFAULT_REGISTER_SPACE)]]       \
    RaytracingAccelerationStructure MKT_ACCEL_STRUCTURES[]

#endif

#define MKT_DECLARE_SHADER_RESOURCES              \
    MKT_DECLARE_STRUCTURED_SRV;                   \
    MKT_DECLARE_STRUCTURED_UAV;                   \
    MKT_DECLARE_BUFFER_DEVICE_ADDRESS;            \
    MKT_DECLARE_SAMPLERS;                         \
    MKT_DECLARE_TEXTURE_SRV(Texture2D);           \
    MKT_DECLARE_TEXTURE_SRV(TextureCube)

// #define MKT_DECLARE_SHADER_RESOURCES              \
//     MKT_DECLARE_STRUCTURED_SRV;                   \
//     MKT_DECLARE_STRUCTURED_UAV;                   \
//     MKT_DECLARE_BUFFER_DEVICE_ADDRESS;            \
//     MKT_DECLARE_SAMPLERS;                         \
//     MKT_DECLARE_TEXTURE_SRV(Texture1D);           \
//     MKT_DECLARE_TEXTURE_SRV(Texture2D);           \
//     MKT_DECLARE_TEXTURE_SRV(Texture3D);           \
//     MKT_DECLARE_TEXTURE_SRV(TextureCube);         \
//     MKT_DECLARE_TEXTURE_UAV(RWTexture1D<float4>); \
//     MKT_DECLARE_TEXTURE_UAV(RWTexture2D<float4>); \
//     MKT_DECLARE_TEXTURE_UAV(RWTexture3D<float4>); \
//     MKT_DECLARE_ACCEL_STRUCTURES