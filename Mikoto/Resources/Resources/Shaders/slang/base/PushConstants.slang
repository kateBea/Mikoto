// Direct constant-buffer declarations for layouts compatible with CPU and cbuffer packing.
// Packed root-constant records use the separate decoding wrapper in Base.slang.

#pragma once

// Use for layouts that already match the CPU and HLSL cbuffer packing rules.
// D3D12 root constants are exposed at b0; Vulkan carries the same bytes through
// its push-constant range. Packed layouts use Base.slang's decoding wrapper.
#if defined( MKT_SUPPORT_D3D12 )
#define MKT_DECLARE_PUSH_CONSTANT_BUFFER( Type, Name ) ConstantBuffer<Type> Name : register( b0 )
#elif defined( MKT_SUPPORT_D3D11 )
#define MKT_DECLARE_PUSH_CONSTANT_BUFFER( Type, Name ) ConstantBuffer<Type> Name : register( b0 )
#else
#define MKT_DECLARE_PUSH_CONSTANT_BUFFER( Type, Name ) [[vk::push_constant]] ConstantBuffer<Type> Name
#endif
