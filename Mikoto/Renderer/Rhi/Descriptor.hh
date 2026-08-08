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
        MKT_NODISCARD static auto Texture_SRV( core::u32 slot ) -> BindingLayoutItem;
        MKT_NODISCARD static auto ConstantBuffer( core::u32 slot ) -> BindingLayoutItem;

        MKT_NODISCARD static auto StructuredBuffer_SRV(core::u32 slot) -> BindingLayoutItem;
        MKT_NODISCARD static auto StructuredBuffer_UAV(core::u32 slot) -> BindingLayoutItem;
    };

    struct BindingSetItem {
        core::IResource* mResource{};

        core::u32 mSlot{};
        BufferRange mRange{};

        ResourceType mType{ ResourceType::eInvalid };
        Format mFormat{ Format::eUnknown };
        TextureDimension mDimension{ TextureDimension::eInvalid };
        TextureSubresourceSet mSubResourceSet{};

        static auto None(core::u32 slot = 0) -> BindingSetItem;

        static auto ConstantBuffer(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer ) -> BindingSetItem;

        static auto Texture_SRV(core::u32 slot, ITexture* texture, Format format = Format::eUnknown,
            TextureSubresourceSet subResources = kAllSubResources, TextureDimension dimension = TextureDimension::eInvalid) -> BindingSetItem;
        static auto Texture_UAV(core::u32 slot, ITexture* texture, Format format = Format::eUnknown,
            TextureSubresourceSet subResources = TextureSubresourceSet(0, 1, 0, TextureSubresourceSet::kAllArraySlices),
            TextureDimension dimension = TextureDimension::eInvalid) -> BindingSetItem;

        static auto TypedBuffer_SRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingSetItem;
        static auto TypedBuffer_UAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingSetItem;

        static auto Sampler(core::u32 slot, ISampler* sampler) -> BindingSetItem;

        static auto StructuredBuffer_SRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingSetItem;
        static auto StructuredBuffer_UAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingSetItem;

        static auto RawBuffer_SRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingSetItem;
        static auto RawBuffer_UAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingSetItem;
    };

    struct BindingLayoutDescription {
        // For Vulkan this maps to descriptor sets
        core::u32 mRegisterSpace{};

        eastl::vector<BindingLayoutItem> mBindings{};
        ShaderFlags mStageVisibility{ ShaderFlagsBits::kVertex };

        auto SetRegisterSpace( core::u32 group ) -> BindingLayoutDescription&;
        auto AddItem( const BindingLayoutItem& item ) -> BindingLayoutDescription&;
        auto SetShaderVisibility( ShaderFlags visibility ) -> BindingLayoutDescription&;
    };

    struct BindlessLayoutItem {
        core::u32 mSlot{};
        core::u32 mMaxCapacity{};

        ResourceType mType{ ResourceType::eInvalid };

        // --- Samplers ---
        MKT_NODISCARD static auto Samplers(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        // --- Textures ---
        MKT_NODISCARD static auto Texture_SRV(core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem;
        MKT_NODISCARD static auto Texture_UAV(core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem;

        // --- Constant Buffers ---
        MKT_NODISCARD static auto ConstantBuffer(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto ConstantBuffer_UAV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        // --- Raw / ByteAddress Buffers ---
        MKT_NODISCARD static auto Buffer_SRV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto Buffer_UAV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        // --- Structured Buffers ---
        MKT_NODISCARD static auto StructuredBuffer_SRV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
        MKT_NODISCARD static auto StructuredBuffer_UAV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        // --- Acceleration Structures ---
        MKT_NODISCARD static auto AccelerationStructures(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
    };

    // For Graphics APIs that support natively bindless design
    struct BindlessLayoutDescription {
        eastl::string mName{};
        core::u32 mRegisterSpace{};
        ShaderFlags mStageVisibility{ ShaderFlagsBits::kVertex };

        eastl::fixed_vector<BindlessLayoutItem, kMaxBindlessRegisterSpaces> mSlots{};

        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        auto SetDebugName( eastl::string_view name ) -> BindlessLayoutDescription&;
        auto SetRegisterSpace( core::u32 registerSpace ) -> BindlessLayoutDescription&;
        auto SetVisibility( ShaderFlags visibility ) -> BindlessLayoutDescription&;
        auto AddBindlessItem( const BindlessLayoutItem& item ) -> BindlessLayoutDescription&;

        auto AddShader( ShaderModuleHandle shader ) -> BindlessLayoutDescription&;
    };

    struct BindingSetDescription {
        eastl::vector<BindingSetItem> mBindings{};

        // Backends offer support for shader reflection
        // which simplifies binding layout creation
        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        auto AddItem(const BindingSetItem& value) -> BindingSetDescription&;
        auto AddShader( ShaderModuleHandle shader ) -> BindingSetDescription&;
    };

    // Upon creation, its contents cannot mutate
    class IBindingSet : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using BindingSetHandle = core::Ref<IBindingSet>;

    // A resizable BindingSet
    class IDescriptorTable : public IBindingSet {
    public:
        // How many indices it holds for instance on Vulkan when we
        // say the descriptor set size for bindless descriptor indexing
        MKT_NODISCARD virtual auto GetCapacity( core::u32 slot ) const -> core::u32 = 0;
    };

    using DescriptorTableHandle = core::Ref<IDescriptorTable>;

}

#endif//MIKOTO_RHI_DESCRIPTOR_HH
