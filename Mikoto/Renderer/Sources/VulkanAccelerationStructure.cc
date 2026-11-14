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

#include <Core/Exception.hh>

#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanAccelerationStructure.hh>

namespace mikoto::renderer::vulkan {
    using namespace core;
    using namespace rhi;


    static auto CheckRT( VkResult result ) -> void {
        if ( result != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan ray-tracing operation failed." );
        }
    }

    AccelerationStructure::AccelerationStructure( const renderer::rhi::AccelStructureCreateDescription &description )
        : mDescription{ description } {
        mResourceType = renderer::rhi::ResourceType::eRayTracingAccelStruct;
    }

    AccelerationStructure::~AccelerationStructure() {
        Destroy();
    }

    auto AccelerationStructure::GetAddress() const -> renderer::rhi::DeviceAddress {
        return mAddress;
    }

    auto AccelerationStructure::GetType() const -> renderer::rhi::AccelStructureType {
        return mDescription.mType;
    }

    auto AccelerationStructure::Handle() const -> VkAccelerationStructureKHR {
        return mStructure;
    }

    auto AccelerationStructure::IsBuilt() const -> bool {
        return mBuilt;
    }

    auto AccelerationStructure::Initialize() -> void {
        PrepareGeometry();
        CreateStorage();

        mIsAllocated = true;
    }

    auto AccelerationStructure::PrepareGeometry() -> void {
        const bool top{ GetType() == renderer::rhi::AccelStructureType::eTopLevel };
        mGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        mGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        mGeometry.geometryType = top ? VK_GEOMETRY_TYPE_INSTANCES_KHR : VK_GEOMETRY_TYPE_TRIANGLES_KHR;

        if ( top ) {
            eastl::vector<VkAccelerationStructureInstanceKHR> instances{};

            for ( const auto &source: mDescription.mInstances ) {
                auto &instance{ instances.emplace_back() };
                std::memcpy( &instance.transform, source.mTransform.data(), sizeof( instance.transform ) );
                instance.instanceCustomIndex = source.mInstanceID;
                instance.mask = source.mMask;
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                instance.accelerationStructureReference = source.mBottomLevel->GetAddress();
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

            mGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            mGeometry.geometry.instances.data.deviceAddress = mInstances->GetGpuDeviceAddress();

            mRange.primitiveCount = static_cast<core::u32>( instances.size() );
        }
        else {
            auto &triangles{ mGeometry.geometry.triangles };

            triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;

            triangles.vertexData.deviceAddress = mDescription.mVertices->GetGpuDeviceAddress();
            triangles.vertexStride = mDescription.mVertexStride;
            triangles.maxVertex = mDescription.mVertexCount - 1;

            triangles.indexType = VK_INDEX_TYPE_UINT32;

            triangles.indexData.deviceAddress = mDescription.mIndices->GetGpuDeviceAddress() + mDescription.mIndexOffset;
            mRange.primitiveCount = mDescription.mIndexCount / 3;
        }
    }

    auto AccelerationStructure::CreateStorage() -> void {
        auto &device{ *static_cast<Device *>( mDevice ) };

        const bool top{ GetType() == renderer::rhi::AccelStructureType::eTopLevel };
        mBuild.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        mBuild.type = top ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        mBuild.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        mBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        mBuild.geometryCount = 1;
        mBuild.pGeometries = &mGeometry;

        // Query the sizes for this geometry before allocating GPU storage.
        VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        vkGetAccelerationStructureBuildSizesKHR( device.GetDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &mBuild, &mRange.primitiveCount, &sizes );

        // Scratch addresses must satisfy the device-specific build alignment.
        VkPhysicalDeviceAccelerationStructurePropertiesKHR properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
        VkPhysicalDeviceProperties2 physical{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &properties };
        vkGetPhysicalDeviceProperties2( device.GetPhysicalDevice()->mPhysicalDevice, &physical );

        // Scratch is needed only while building, not when tracing the completed structure.
        mScratchSize = sizes.buildScratchSize;
        mScratchAlignment = properties.minAccelerationStructureScratchOffsetAlignment;

        // Keep only the traversal result resident with the acceleration structure.
        mResult = mDevice->CreateBuffer( rhi::BufferCreateDescription{}
            .SetName( "Acceleration Structure" )
            .SetByteSize( sizes.accelerationStructureSize )
            .SetResourceType( rhi::ResourceType::eRawBuffer_SRV )
            .SetBufferUsage( rhi::BufferUsageFlagsBits::AccelStructureStorage ) );

        // Create the KHR object backed by the result buffer.
        VkAccelerationStructureCreateInfoKHR create{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        create.buffer = mResult->GetNativeHandle( rhi::ObjectType::Vk_Buffer );
        create.size = sizes.accelerationStructureSize;
        create.type = mBuild.type;
        CheckRT( vkCreateAccelerationStructureKHR( device.GetDevice(), &create, nullptr, &mStructure ) );
        mBuild.dstAccelerationStructure = mStructure;

        // TLAS instances refer to BLAS objects through this device address.
        VkAccelerationStructureDeviceAddressInfoKHR address{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
        address.accelerationStructure = mStructure;
        mAddress = vkGetAccelerationStructureDeviceAddressKHR( device.GetDevice(), &address );
    }

    auto AccelerationStructure::Destroy() -> void {

        if ( mStructure ) {
            vkDestroyAccelerationStructureKHR( static_cast<Device *>( mDevice )->GetDevice(), mStructure, nullptr );
            mStructure = VK_NULL_HANDLE;
        }

        mInstances.Reset();
        mResult.Reset();
        mScratchSize = 0;
        mScratchAlignment = 1;
        mAddress = 0;
        mBuilt = false;
        mIsAllocated = false;
    }
}// namespace mikoto::renderer::vulkan
