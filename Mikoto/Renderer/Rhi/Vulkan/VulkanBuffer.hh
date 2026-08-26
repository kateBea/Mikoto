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

#ifndef MIKOTO_VULKAN_BUFFER_HH
#define MIKOTO_VULKAN_BUFFER_HH

#include <EASTL/string_view.h>

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>

#include <Renderer/Rhi/Vulkan/VulkanMemoryAllocator.hh>

namespace mikoto::renderer::vulkan {

    class Buffer final : public rhi::IBuffer {
    public:
        explicit Buffer( const rhi::BufferCreateDescription& createInfo );

        auto PersistentMap() -> void;
        auto PersistentUnmap() -> void;

        auto SetDebugName( eastl::string_view name) -> void override;

        MKT_NODISCARD auto GetGpuDeviceAddress( IBuffer* buffer ) -> rhi::DeviceAddress override;

        MKT_NODISCARD auto GetAlignedSize() const -> core::u32;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD auto IsMapped() const -> bool;
        MKT_NODISCARD auto GetMappedAddress() -> void*;
        MKT_NODISCARD auto GetMappedAddress() const -> const void*;

        MKT_NODISCARD operator VkBuffer() const;

        ~Buffer() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        // If the buffer is dynamic this value contains
        // the size of each frame in flight slice
        core::size_t mAlignedSizeBytes{};
        core::size_t mStagingSliceSize{};

        BufferAllocation mAllocation{};

        bool mKeepInitializerResources{ false };
    };
}

#endif // MIKOTO_VULKAN_BUFFER_HH
