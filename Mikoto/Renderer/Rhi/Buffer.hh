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

#ifndef MIKOTO_RENDERER_RHI_BUFFER_HH
#define MIKOTO_RENDERER_RHI_BUFFER_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Resource.hh>

#include <Memory/BufferSpan.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    /**
     * Describes a buffer before it is created by an RHI device.
     */
    using DeviceAddress = core::u64;

    /**
     * Creation parameters for an RHI buffer.
     *
     * @details A typed buffer uses both @ref mElementCount and @ref mElementSize;
     * a raw buffer stores its complete byte size in @ref mElementSize.
     */
    struct BufferCreateDescription {
        eastl::string mName{};
        bool mKeepInitializerResources{ false };

        memory::BufferSpanHandle mSpanHandle{};

        // Hint to specify the count of elements
        // Used by the backend to compute alignment
        // If specified both must be different to 0
        // this says the buffer is typed and needs to hold mElementCount
        // elements of size mElementSize. If it is going to be treated as raw bytes
        // then mElementSize holds the total size in bytes
        core::usize mElementCount{};
        core::usize mElementSize{};

        // For Vulkan and D3D12 when we need to manage
        // frequently updating uniform/constant buffers
        core::usize mMaxVersions{ 0 };
        bool mIsVolatile{};

        bool mTrackState{ true };
        ResourceStates mInitialState{ ResourceStates::eUnknown };

        Format mFormat{ Format::eUnknown };
        HeapType mHeapType{ HeapType::eDeviceLocal };
        AccessType mCpuAccess{ AccessType::eNone };
        ResourceType mResourceType{ ResourceType::eConstantBuffer };

        BufferDataType mDataType{ BufferDataType::eInvalid };
        BufferUsageFlags mUsageFlags{ BufferUsageFlagsBits::None };

        /**
         * Sets a backend debug name.
         *
         * @param name Input value used by this operation.
         * @returns The result of SetName.
         */
        auto SetName( eastl::string_view name ) -> BufferCreateDescription&;

        /**
         * Configures @p count elements of @p byteSize bytes each.
         *
         * @param byteSize Input value used by this operation.
         * @param count Input value used by this operation.
         * @returns The result of ForElement.
         */
        auto ForElement( core::usize byteSize, core::usize count ) -> BufferCreateDescription&;

        /**
         * Configures this description as a raw buffer of @p byteSize bytes.
         *
         * @param byteSize Input value used by this operation.
         * @returns The result of SetByteSize.
         */
        auto SetByteSize( core::usize byteSize ) -> BufferCreateDescription&;

        /**
         * Sets the value handled by SetFormat.
         *
         * @param format Input value used by this operation.
         * @returns The result of SetFormat.
         */
        auto SetFormat( Format format ) -> BufferCreateDescription&;

        /**
         * Sets initial CPU data and enables copy-destination usage.
         *
         * @param data Input value used by this operation.
         * @returns The result of SetInitialData.
         */
        auto SetInitialData( memory::BufferSpanHandle data ) -> BufferCreateDescription&;

        /**
         * Sets the value handled by SetBufferUsage.
         *
         * @param usage Input value used by this operation.
         * @returns The result of SetBufferUsage.
         */
        auto SetBufferUsage( BufferUsageFlags usage ) -> BufferCreateDescription&;

        /**
         * Sets the value handled by SetBufferDataType.
         *
         * @param type Input value used by this operation.
         * @returns The result of SetBufferDataType.
         */
        auto SetBufferDataType( BufferDataType type ) -> BufferCreateDescription&;

        /**
         * Sets the value handled by SetHeapType.
         *
         * @param type Input value used by this operation.
         * @returns The result of SetHeapType.
         */
        auto SetHeapType( HeapType type ) -> BufferCreateDescription&;

        /**
         * Sets CPU access, choosing an upload heap for CPU-writeable resources.
         *
         * @param type Input value used by this operation.
         * @returns The result of SetCpuAccessType.
         */
        auto SetCpuAccessType( AccessType type ) -> BufferCreateDescription&;

        /**
         * Sets the value handled by SetResourceType.
         *
         * @param type Input value used by this operation.
         * @returns The result of SetResourceType.
         */
        auto SetResourceType( ResourceType type ) -> BufferCreateDescription&;

        /**
         * Controls whether upload data is retained after resource creation.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetKeepInitializerResources.
         */
        auto SetKeepInitializerResources( bool value ) -> BufferCreateDescription&;

        /**
         * Sets the value handled by SetIsVolatile.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetIsVolatile.
         */
        auto SetIsVolatile( bool value ) -> BufferCreateDescription&;

        /**
         * Sets the value handled by SetMaxVersions.
         *
         * @param count Input value used by this operation.
         * @returns The result of SetMaxVersions.
         */
        auto SetMaxVersions( core::u32 count ) -> BufferCreateDescription&;

        /**
         * Sets the value handled by SetInitialState.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetInitialState.
         */
        constexpr auto SetInitialState( ResourceStates value ) -> BufferCreateDescription& {
            mInitialState = value;
            return *this;
        }

        /**
         * Performs the operation represented by EnableAutomaticStateTracking.
         *
         * @param initialState Input value used by this operation.
         * @returns The result of EnableAutomaticStateTracking.
         */
        constexpr auto EnableAutomaticStateTracking( ResourceStates initialState ) -> BufferCreateDescription& {
            mInitialState = initialState;
            mTrackState = true;
            return *this;
        }
    };

    /**
     * Backend-independent interface for a GPU buffer.
     *
     * Implementations expose an optional GPU virtual address and retain the
     * metadata supplied through @ref BufferCreateDescription.
     */
    class IBuffer : public DeviceObject {
    public:
        static constexpr DeviceAddress kNullDeviceAddress{ 0 };

        /**
         * Returns the GPU virtual address, or @ref kNullDeviceAddress when unavailable.
         * @returns The result of GetGpuDeviceAddress.
         */
        MKT_NODISCARD virtual auto GetGpuDeviceAddress() -> DeviceAddress = 0;

        /**
         * Returns the Shader virtual address, or @ref kNullDeviceAddress when unavailable.
         * @returns The result of GetShaderDeviceAddress, can be used to directly access resources within shaders.
         */
        MKT_NODISCARD virtual auto GetShaderDeviceAddress() -> DeviceAddress = 0;

        /**
         * Returns the intended uses declared for this buffer.
         * @returns The result of GetUsage.
         */
        MKT_NODISCARD virtual auto GetUsage() const -> BufferUsageFlags = 0;

        /**
         * Returns the logical data classification of this buffer.
         * @returns The result of GetDataType.
         */
        MKT_NODISCARD virtual auto GetDataType() const -> BufferDataType = 0;

        /**
         * Returns retained CPU upload contents, if initialization data was preserved.
         * @returns The result of GetData.
         */
        MKT_NODISCARD virtual auto GetData() const -> memory::BufferSpanHandle = 0;

        /**
         * Returns the total logical size of the buffer in bytes.
         * @returns The result of GetSizeBytes.
         */
        MKT_NODISCARD virtual auto GetSizeBytes() const -> core::usize = 0;

        /**
         * Returns the element format, or @ref Format::eUnknown for unformatted buffers.
         * @returns The result of GetFormat.
         */
        MKT_NODISCARD virtual auto GetFormat() const -> Format = 0;

        /**
         * Returns the element count, inferring it for raw formatted buffers.
         * @returns The result of GetCount.
         */
        MKT_NODISCARD virtual auto GetCount() const -> core::usize = 0;

        using DeviceObject::Initialize;
    };

    using BufferHandle = core::Ref<IBuffer>;
}// namespace mikoto::renderer::rhi

#endif// MIKOTO_RENDERER_RHI_BUFFER_HH

