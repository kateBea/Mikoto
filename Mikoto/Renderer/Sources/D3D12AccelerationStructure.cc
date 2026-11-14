#include <Core/Exception.hh>

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

#include <cstring>

#include <Renderer/Rhi/D3D12/D3D12Buffer.hh>
#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12AccelerationStructure.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

namespace mikoto::renderer::d3d12 {
    using namespace core;
    using namespace rhi;
    using Microsoft::WRL::ComPtr;


    static auto CheckRT( HRESULT result ) -> void {
        if ( FAILED( result ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 ray-tracing operation failed." );
        }
    }

    static auto RayDevice( renderer::rhi::IGpuDevice* device ) -> ComPtr<ID3D12Device5> {
        ComPtr<ID3D12Device5> result{};
        CheckRT( static_cast<Device*>( device )->GetDevice()->QueryInterface( IID_PPV_ARGS( &result ) ) );
        return result;
    }


    AccelerationStructure::AccelerationStructure( const renderer::rhi::AccelStructureCreateDescription& description )
        : mDescription{ description } {
        mResourceType = renderer::rhi::ResourceType::eRayTracingAccelStruct;
    }

    AccelerationStructure::~AccelerationStructure() {
        Destroy();
    }
    
    auto AccelerationStructure::GetAddress() const -> renderer::rhi::DeviceAddress {
        return mResult ? mBuild.DestAccelerationStructureData : 0;
    }

    auto AccelerationStructure::GetType() const -> renderer::rhi::AccelStructureType {
        return mDescription.mType;
    }
    
    auto AccelerationStructure::IsBuilt() const -> bool {
        return mBuilt;
    }
    
    auto AccelerationStructure::Initialize() -> void {
        auto device{ RayDevice( mDevice ) };
        auto& input{ mBuild.Inputs };

        input.Type = GetType() == renderer::rhi::AccelStructureType::eTopLevel ? 
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL : 
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

        input.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        input.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

        if ( GetType() == renderer::rhi::AccelStructureType::eTopLevel ) {
            eastl::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances{};

            for ( const auto& source: mDescription.mInstances ) {
                auto& instance{ instances.emplace_back() };
                std::memcpy( instance.Transform, source.mTransform.data(), sizeof( instance.Transform ) );
                instance.InstanceID = source.mInstanceID;
                instance.InstanceMask = source.mMask;
                instance.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
                instance.AccelerationStructure = source.mBottomLevel->GetAddress();
            }

            const auto bytes{ instances.size() * sizeof( instances[0] ) };
            mInstances = mDevice->CreateBuffer( rhi::BufferCreateDescription{}
                .SetName( "TLAS Instances" )
                .SetByteSize( bytes )
                .SetHeapType( rhi::HeapType::eUpload )
                .SetResourceType( rhi::ResourceType::eRawBuffer_SRV )
                .SetBufferUsage( rhi::BufferUsageFlagsBits::AccelStructureBuildInput ) );

            auto commands{ mDevice->CreateCommandList( rhi::QueueType::eTransfer ) };
            commands->Write( mInstances.GetPtr(), instances.data(), bytes );
            input.NumDescs = static_cast<UINT>( instances.size() );
            input.InstanceDescs = mInstances->GetGpuDeviceAddress();
        }
        else {
            mGeometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            mGeometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            auto& triangles{ mGeometry.Triangles };
            triangles.VertexBuffer.StartAddress = mDescription.mVertices->GetGpuDeviceAddress();
            triangles.VertexBuffer.StrideInBytes = mDescription.mVertexStride;
            triangles.VertexCount = mDescription.mVertexCount;
            triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            triangles.IndexBuffer = mDescription.mIndices->GetGpuDeviceAddress() + mDescription.mIndexOffset;
            triangles.IndexCount = mDescription.mIndexCount;
            triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

            input.NumDescs = 1;
            input.pGeometryDescs = &mGeometry;
        }

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO sizes{};
        device->GetRaytracingAccelerationStructurePrebuildInfo( &input, &sizes );

        if ( !sizes.ResultDataMaxSizeInBytes || !sizes.ScratchDataSizeInBytes ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid DXR acceleration-structure size." );
        }

        mResult = mDevice->CreateBuffer( rhi::BufferCreateDescription{}
            .SetName( "Acceleration Structure" )
            .SetByteSize( sizes.ResultDataMaxSizeInBytes )
            .SetResourceType( rhi::ResourceType::eRawBuffer_SRV )
            .SetBufferUsage( rhi::BufferUsageFlagsBits::AccelStructureStorage ) );

        mScratch = mDevice->CreateBuffer( rhi::BufferCreateDescription{}
            .SetName( "Acceleration Structure Scratch" )
            .SetByteSize( sizes.ScratchDataSizeInBytes )
            .SetResourceType( rhi::ResourceType::eRawBuffer_UAV )
            .SetBufferUsage( rhi::BufferUsageFlagsBits::AccelStructureScratch ) );

        mBuild.DestAccelerationStructureData = mResult->GetGpuDeviceAddress();
        mBuild.ScratchAccelerationStructureData = mScratch->GetGpuDeviceAddress();

        mIsAllocated = true;
    }

    auto AccelerationStructure::Destroy() -> void {
        mInstances.Reset();
        mScratch.Reset();
        mResult.Reset();
        mBuilt = false;
        mIsAllocated = false;
    }
}
#endif
