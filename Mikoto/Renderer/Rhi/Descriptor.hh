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

#ifndef MIKOTO_RHI_DESCRIPTOR_HH
#define MIKOTO_RHI_DESCRIPTOR_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/ResourcePool.hh>

#include <Memory/BufferSpan.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    struct BindingLayoutItem {
        core::u32 mSlot{};

        Format mFormat{ Format::eUnknown };
        ResourceType mType{ ResourceType::eInvalid };
        TextureDimension mDimension{ TextureDimension::eInvalid };

        MKT_NODISCARD static auto Sampler( core::u32 slot ) -> BindingLayoutItem;
        MKT_NODISCARD static auto TextureSRV( core::u32 slot ) -> BindingLayoutItem;
        MKT_NODISCARD static auto ConstantBuffer( core::u32 slot ) -> BindingLayoutItem;

        MKT_NODISCARD static auto StructuredSRV(core::u32 slot) -> BindingLayoutItem;
        MKT_NODISCARD static auto StructuredUAV(core::u32 slot) -> BindingLayoutItem;
    };

    struct BindingTableItem {
        BufferRange mRange{};
        core::u32 mBindingIndex{};
        core::IResource* mResource{};

        ResourceType mType{ ResourceType::eInvalid };
        Format mFormat{ Format::eUnknown };
        TextureDimension mDimension{ TextureDimension::eInvalid };
        TextureSubresourceSet mSubResourceSet{};

        static auto None(core::u32 slot = 0) -> BindingTableItem;
        static auto ConstantBuffer(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer ) -> BindingTableItem;
        static auto TextureSRV(core::u32 slot, ITexture* texture, Format format = Format::eUnknown,
            TextureSubresourceSet subResources = kAllSubResources, TextureDimension dimension = TextureDimension::eInvalid) -> BindingTableItem;
        static auto TextureUAV(core::u32 slot, ITexture* texture, Format format = Format::eUnknown,
            TextureSubresourceSet subResources = TextureSubresourceSet(0, 1, 0, TextureSubresourceSet::kAllArraySlices),
            TextureDimension dimension = TextureDimension::eInvalid) -> BindingTableItem;
        static auto TypedBufferSRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;
        static auto TypedBufferUAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;
        static auto Sampler(core::u32 slot, ISampler* sampler) -> BindingTableItem;
        static auto StructuredSRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;
        static auto StructuredUAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;
        static auto RawBufferSRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;
        static auto RawBufferUAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;
    };

    struct BindingLayoutDescription {
        // For Vulkan this maps to descriptor sets
        core::u32 mRegisterSpace{};

        eastl::vector<BindingLayoutItem> mBindings{};
        ShaderFlags mStageVisibility{ ShaderFlagsBits::Vertex };

        auto SetRegisterSpace( core::u32 group ) -> BindingLayoutDescription&;
        auto AddItem( const BindingLayoutItem& item ) -> BindingLayoutDescription&;
        auto SetShaderVisibility( ShaderFlags visibility ) -> BindingLayoutDescription&;
    };

    struct BindlessLayoutItem {
        core::u32 mSlot{};
        core::u32 mMaxCapacity{};

        ResourceType mType{ ResourceType::eInvalid };

        MKT_NODISCARD static auto Samplers(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto TextureSRV(core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem;
        MKT_NODISCARD static auto TextureUAV(core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem;
        MKT_NODISCARD static auto ConstantBuffer(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto RawBufferSRV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto RawBufferUAV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto StructuredSRV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto StructuredUAV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto AccelerationStructure(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
    };

    // For Graphics APIs that support natively bindless design
    struct DescriptorTableLayoutDescription {
        eastl::string mName{};
        core::u32 mRegisterSpace{};
        ShaderFlags mStageVisibility{ ShaderFlagsBits::Vertex };

        eastl::fixed_vector<BindlessLayoutItem, kMaxBindlessRegisterSpaces> mSlots{};

        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        auto SetDebugName( eastl::string_view name ) -> DescriptorTableLayoutDescription&;
        auto SetRegisterSpace( core::u32 registerSpace ) -> DescriptorTableLayoutDescription&;
        auto SetVisibility( ShaderFlags visibility ) -> DescriptorTableLayoutDescription&;
        auto AddBindlessItem( const BindlessLayoutItem& item ) -> DescriptorTableLayoutDescription&;

        auto AddShader( ShaderModuleHandle shader ) -> DescriptorTableLayoutDescription&;
    };

    struct BindingTableDescription {
        eastl::vector<BindingTableItem> mBindings{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        // TODO: Move this to a reflection module
        // Vulkan for instance via the spirv_reflect library allows us to
        // introspect SPIRV binaries to know descriptor layouts, amongst other
        // information, this could be helpful to generate the appropriate
        // binding layouts from the RHI for our pipeline
        bool mUseReflection{};

        auto AddItem(const BindingTableItem& value) -> BindingTableDescription&;
        auto AddShader( ShaderModuleHandle shader ) -> BindingTableDescription&;
    };

    // Upon creation, its contents can mutate but we can only update
    // the slots with new resources
    // Treated as a table of bindings where each one has a unique
    // slot index
    class IBindingTable : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using BindingTableHandle = core::Ref<IBindingTable>;

    // A resizable BindingSet
    class IDescriptorTable : public IBindingTable {
    public:
        // How many indices it holds for instance on Vulkan when we
        // say the descriptor set size for bindless descriptor indexing
        MKT_NODISCARD virtual auto GetCapacity( core::u32 slot ) const -> core::u32 = 0;
    };

    using DescriptorTableHandle = core::Ref<IDescriptorTable>;

}

#endif//MIKOTO_RHI_DESCRIPTOR_HH
