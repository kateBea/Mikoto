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

#ifndef MIKOTO_RHI_BUFFER_HH
#define MIKOTO_RHI_BUFFER_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Memory/BufferSpan.hh>
#include <Core/ResourcePool.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    using DeviceAddress = core::u64;

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
        core::size_t mElementCount{};
        core::size_t mElementSize{};

        // For Vulkan and D3D12 when we need to manage
        // frequently updating uniform/constant buffers
        core::size_t mMaxVersions{ 0 };
        bool mIsVolatile{};

        bool mTrackState{ true };
        ResourceStates mInitialState{ ResourceStates::eUnknown };

        Format mFormat{ Format::eUnknown };
        HeapType mHeapType{ HeapType::eDeviceLocal };
        CpuAccessType mCpuAccess{ CpuAccessType::eNone };
        ResourceType mResourceType{ ResourceType::eConstantBuffer };

        BufferDataType mDataType{ BufferDataType::eInvalid };
        BufferUsageFlags mUsageFlags{ BufferUsageFlagsBits::kNone };

        auto SetName( eastl::string_view name ) -> BufferCreateDescription&;
        auto ForElement( core::size_t byteSize, core::size_t count ) -> BufferCreateDescription&;
        auto SetByteSize( core::size_t byteSize ) -> BufferCreateDescription&;
        auto SetFormat( Format format ) -> BufferCreateDescription&;
        auto SetInitialData( memory::BufferSpanHandle data ) -> BufferCreateDescription&;
        auto SetBufferUsage( BufferUsageFlags usage ) -> BufferCreateDescription&;
        auto SetBufferDataType( BufferDataType type ) -> BufferCreateDescription&;
        auto SetHeapType( HeapType type ) -> BufferCreateDescription&;
        auto SetCpuAccessType( CpuAccessType type ) -> BufferCreateDescription&;
        auto SetResourceType( ResourceType type ) -> BufferCreateDescription&;

        auto SetKeepInitializerResources( bool value ) -> BufferCreateDescription&;

        auto SetIsVolatile( bool value ) -> BufferCreateDescription&;
        auto SetMaxVersions( core::u32 count ) -> BufferCreateDescription&;

        constexpr auto SetInitialState( ResourceStates value ) -> BufferCreateDescription& {
            mInitialState = value;
            return *this;
        }
        constexpr auto EnableAutomaticStateTracking( ResourceStates initialState ) -> BufferCreateDescription& {
            mInitialState = initialState;
            mTrackState = true;
            return *this;
        }
    };

    class IBuffer : public DeviceObject {
    public:
        static constexpr DeviceAddress kNullDeviceAddress{ 0 };

        MKT_NODISCARD virtual auto GetGpuDeviceAddress(IBuffer* buffer) -> DeviceAddress = 0;

        MKT_NODISCARD auto GetUsage() const -> BufferUsageFlags { return mUsage; }
        MKT_NODISCARD auto GetDataType() const -> BufferDataType { return mDataType; }

        MKT_NODISCARD auto GetData() const -> memory::BufferSpanHandle { return mUploadContents; }
        MKT_NODISCARD auto GetSizeBytes() const -> size_t { return mElementCount == 0 ? mElementSize : mElementCount * mElementSize; }

        MKT_NODISCARD auto GetFormat() const -> Format { return mFormat; }

        // FIXME: does not produce expected results
        MKT_NODISCARD auto GetCount() const -> size_t {
            return mElementCount == 0 ? InferElementCount(mFormat, mElementSize) : mElementCount * mElementSize;
        }

        using DeviceObject::Initialize;

    protected:
        explicit IBuffer( const BufferCreateDescription& desc )
            : DeviceObject{ desc.mHeapType, desc.mResourceType },
              mUploadContents{ desc.mSpanHandle },
              mElementCount{ desc.mElementCount },
              mElementSize{ desc.mElementSize },
              mDataType{ desc.mDataType },
              mUsage{ desc.mUsageFlags },
              mIsVolatile{ desc.mIsVolatile },
              mMaxVersions{ desc.mMaxVersions },
              mFormat{ desc.mFormat }
        {}

    protected:
        memory::BufferSpanHandle mUploadContents{};

        // These 2 attributes specify that the buffer is typed
        // meaning it will hold mElementCount elements of mElementSize size in bytes
        // This is useful for the backend API to manage alignment as it considers necessary
        // If it is going to be treated as raw bytes
        // then mElementSize holds the total size in bytes
        core::size_t mElementCount{};
        core::size_t mElementSize{};

        BufferDataType mDataType{ BufferDataType::eInvalid };
        BufferUsageFlags mUsage{ BufferUsageFlagsBits::kNone };

        bool mIsVolatile{};
        core::size_t mMaxVersions{ 0 };

        Format mFormat{ Format::eUnknown };
    };

    using BufferHandle = core::Ref<IBuffer>;
}// namespace mikoto::renderer::rhi

#endif//MIKOTO_RHI_BUFFER_HH
