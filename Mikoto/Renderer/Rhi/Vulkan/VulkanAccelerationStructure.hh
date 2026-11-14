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

#ifndef MIKOTO_RENDERER_RHI_VULKAN_VULKAN_ACCELERATION_STRUCTURE_HH
#define MIKOTO_RENDERER_RHI_VULKAN_VULKAN_ACCELERATION_STRUCTURE_HH

#include <Core/Platform.hh>

#include <Renderer/Rhi/AccelerationStructure.hh>
#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

// https://docs.vulkan.org/tutorial/latest/courses/18_Ray_tracing/02_Acceleration_structures.html

namespace mikoto::renderer::vulkan {


    class AccelerationStructure final : public rhi::IAccelerationStructure {
        friend class CommandList;

    public:
        explicit AccelerationStructure( const rhi::AccelStructureCreateDescription &description );
        ~AccelerationStructure() override;

        DISABLE_COPY_AND_MOVE_FOR( AccelerationStructure );

        auto GetAddress() const -> rhi::DeviceAddress override;
        auto GetType() const -> rhi::AccelStructureType override;
        auto Handle() const -> VkAccelerationStructureKHR;
        auto IsBuilt() const -> bool;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        // Describe BLAS triangles or TLAS instances before querying build sizes.
        auto PrepareGeometry() -> void;

        // Query build storage requirements, allocate the result and create the KHR object.
        auto CreateStorage() -> void;

        renderer::rhi::AccelStructureCreateDescription mDescription{};

        // Result and instance storage belong to the structure; build scratch belongs to the recording.
        rhi::BufferHandle mResult{};
        rhi::BufferHandle mInstances{};
        VkDeviceSize mScratchSize{};
        VkDeviceSize mScratchAlignment{ 1 };

        // Native object and address used by TLAS instances.
        VkAccelerationStructureKHR mStructure{};
        VkDeviceAddress mAddress{};

        // Build description consumed when the command list records Build().
        VkAccelerationStructureGeometryKHR mGeometry{};
        VkAccelerationStructureBuildGeometryInfoKHR mBuild{};
        VkAccelerationStructureBuildRangeInfoKHR mRange{};

        bool mBuilt{};
    };
}// namespace mikoto::renderer::vulkan

#endif// MIKOTO_RENDERER_RHI_VULKAN_VULKAN_ACCELERATION_STRUCTURE_HH
