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

    /**
     * Declares one slot in a conventional resource-binding layout.
     */
    struct BindingLayoutItem {
        core::u32 mSlot{};

        Format mFormat{ Format::eUnknown };
        ResourceType mType{ ResourceType::eInvalid };
        TextureDimension mDimension{ TextureDimension::eInvalid };

        /**
         * Performs the operation represented by Sampler.
         *
         * @param slot Input value used by this operation.
         * @returns The result of Sampler.
         */
        MKT_NODISCARD static auto Sampler( core::u32 slot ) -> BindingLayoutItem;

        /**
         * Performs the operation represented by TextureSRV.
         *
         * @param slot Input value used by this operation.
         * @returns The result of TextureSRV.
         */
        MKT_NODISCARD static auto TextureSRV( core::u32 slot ) -> BindingLayoutItem;

        /**
         * Performs the operation represented by ConstantBuffer.
         *
         * @param slot Input value used by this operation.
         * @returns The result of ConstantBuffer.
         */
        MKT_NODISCARD static auto ConstantBuffer( core::u32 slot ) -> BindingLayoutItem;

        /**
         * Performs the operation represented by StructuredSRV.
         *
         * @param slot Input value used by this operation.
         * @returns The result of StructuredSRV.
         */
        MKT_NODISCARD static auto StructuredSRV(core::u32 slot) -> BindingLayoutItem;

        /**
         * Performs the operation represented by StructuredUAV.
         *
         * @param slot Input value used by this operation.
         * @returns The result of StructuredUAV.
         */
        MKT_NODISCARD static auto StructuredUAV(core::u32 slot) -> BindingLayoutItem;
    };

    /**
     * Assigns one resource view or sampler to a binding-table slot.
     */
    struct BindingTableItem {
        BufferRange mRange{};
        core::u32 mBindingIndex{};
        core::IResource* mResource{};

        ResourceType mType{ ResourceType::eInvalid };
        Format mFormat{ Format::eUnknown };
        TextureDimension mDimension{ TextureDimension::eInvalid };
        TextureSubresourceSet mSubResourceSet{};

        /**
         * Performs the operation represented by None.
         *
         * @param slot Input value used by this operation.
         * @returns The result of None.
         */
        static auto None(core::u32 slot = 0) -> BindingTableItem;

        /**
         * Performs the operation represented by ConstantBuffer.
         *
         * @param slot Input value used by this operation.
         * @param buffer Input value used by this operation.
         * @param range Input value used by this operation.
         * @returns The result of ConstantBuffer.
         */
        static auto ConstantBuffer(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer ) -> BindingTableItem;

        /**
         * Performs the operation represented by TextureSRV.
         *
         * @param slot Input value used by this operation.
         * @param texture Input value used by this operation.
         * @param format Input value used by this operation.
         * @param subResources Input value used by this operation.
         * @param dimension Input value used by this operation.
         * @returns The result of TextureSRV.
         */
        static auto TextureSRV(core::u32 slot, ITexture* texture, Format format = Format::eUnknown,
            TextureSubresourceSet subResources = kAllSubResources, TextureDimension dimension = TextureDimension::eInvalid) -> BindingTableItem;

        /**
         * Performs the operation represented by TextureUAV.
         *
         * @param slot Input value used by this operation.
         * @param texture Input value used by this operation.
         * @param format Input value used by this operation.
         * @param subResources Input value used by this operation.
         * @param dimension Input value used by this operation.
         * @returns The result of TextureUAV.
         */
        static auto TextureUAV(core::u32 slot, ITexture* texture, Format format = Format::eUnknown,
            TextureSubresourceSet subResources = TextureSubresourceSet(0, 1, 0, TextureSubresourceSet::kAllArraySlices),
            TextureDimension dimension = TextureDimension::eInvalid) -> BindingTableItem;

        /**
         * Performs the operation represented by TypedBufferSRV.
         *
         * @param slot Input value used by this operation.
         * @param buffer Input value used by this operation.
         * @param range Input value used by this operation.
         * @returns The result of TypedBufferSRV.
         */
        static auto TypedBufferSRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;

        /**
         * Performs the operation represented by TypedBufferUAV.
         *
         * @param slot Input value used by this operation.
         * @param buffer Input value used by this operation.
         * @param range Input value used by this operation.
         * @returns The result of TypedBufferUAV.
         */
        static auto TypedBufferUAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;

        /**
         * Performs the operation represented by Sampler.
         *
         * @param slot Input value used by this operation.
         * @param sampler Input value used by this operation.
         * @returns The result of Sampler.
         */
        static auto Sampler(core::u32 slot, ISampler* sampler) -> BindingTableItem;

        /**
         * Performs the operation represented by StructuredSRV.
         *
         * @param slot Input value used by this operation.
         * @param buffer Input value used by this operation.
         * @param range Input value used by this operation.
         * @returns The result of StructuredSRV.
         */
        static auto StructuredSRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;

        /**
         * Performs the operation represented by StructuredUAV.
         *
         * @param slot Input value used by this operation.
         * @param buffer Input value used by this operation.
         * @param range Input value used by this operation.
         * @returns The result of StructuredUAV.
         */
        static auto StructuredUAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;

        /**
         * Performs the operation represented by RawBufferSRV.
         *
         * @param slot Input value used by this operation.
         * @param buffer Input value used by this operation.
         * @param range Input value used by this operation.
         * @returns The result of RawBufferSRV.
         */
        static auto RawBufferSRV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;

        /**
         * Performs the operation represented by RawBufferUAV.
         *
         * @param slot Input value used by this operation.
         * @param buffer Input value used by this operation.
         * @param range Input value used by this operation.
         * @returns The result of RawBufferUAV.
         */
        static auto RawBufferUAV(core::u32 slot, IBuffer* buffer, BufferRange range = kEntireBuffer) -> BindingTableItem;
    };

    /**
     * Describes the slots and shader visibility of a binding layout.
     */
    struct BindingLayoutDescription {
        // For Vulkan this maps to descriptor sets
        core::u32 mRegisterSpace{};

        eastl::vector<BindingLayoutItem> mBindings{};
        ShaderFlags mStageVisibility{ ShaderFlagsBits::Vertex };

        /**
         * Sets the value handled by SetRegisterSpace.
         *
         * @param group Input value used by this operation.
         * @returns The result of SetRegisterSpace.
         */
        auto SetRegisterSpace( core::u32 group ) -> BindingLayoutDescription&;

        /**
         * Adds the supplied value through AddItem.
         *
         * @param item Input value used by this operation.
         * @returns The result of AddItem.
         */
        auto AddItem( const BindingLayoutItem& item ) -> BindingLayoutDescription&;

        /**
         * Sets the value handled by SetShaderVisibility.
         *
         * @param visibility Input value used by this operation.
         * @returns The result of SetShaderVisibility.
         */
        auto SetShaderVisibility( ShaderFlags visibility ) -> BindingLayoutDescription&;
    };

    /**
     * Declares one bindless descriptor array.
     */
    struct BindlessLayoutItem {
        core::u32 mSlot{};
        core::u32 mMaxCapacity{};

        ResourceType mType{ ResourceType::eInvalid };

        /**
         * Performs the operation represented by Samplers.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of Samplers.
         */
        MKT_NODISCARD static auto Samplers(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        /**
         * Performs the operation represented by TextureSRV.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of TextureSRV.
         */
        MKT_NODISCARD static auto TextureSRV(core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem;

        /**
         * Performs the operation represented by TextureUAV.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of TextureUAV.
         */
        MKT_NODISCARD static auto TextureUAV(core::u32 slot, core::u32 maxCapacity ) -> BindlessLayoutItem;

        /**
         * Performs the operation represented by ConstantBuffer.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of ConstantBuffer.
         */
        MKT_NODISCARD static auto ConstantBuffer(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        /**
         * Performs the operation represented by RawBufferSRV.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of RawBufferSRV.
         */
        MKT_NODISCARD static auto RawBufferSRV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        /**
         * Performs the operation represented by RawBufferUAV.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of RawBufferUAV.
         */
        MKT_NODISCARD static auto RawBufferUAV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        /**
         * Performs the operation represented by StructuredSRV.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of StructuredSRV.
         */
        MKT_NODISCARD static auto StructuredSRV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        /**
         * Performs the operation represented by StructuredUAV.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of StructuredUAV.
         */
        MKT_NODISCARD static auto StructuredUAV(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;

        /**
         * Performs the operation represented by AccelerationStructure.
         *
         * @param slot Input value used by this operation.
         * @param maxCapacity Input value used by this operation.
         * @returns The result of AccelerationStructure.
         */
        MKT_NODISCARD static auto AccelerationStructure(core::u32 slot, core::u32 maxCapacity) -> BindlessLayoutItem;
    };

    /**
     * Layout of a bindless descriptor table for APIs that support it natively.
     */
    struct DescriptorTableLayoutDescription {
        eastl::string mName{};
        core::u32 mRegisterSpace{};
        ShaderFlags mStageVisibility{ ShaderFlagsBits::Vertex };

        eastl::fixed_vector<BindlessLayoutItem, kMaxBindlessRegisterSpaces> mSlots{};

        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        /**
         * Sets the value handled by SetDebugName.
         *
         * @param name Input value used by this operation.
         * @returns The result of SetDebugName.
         */
        auto SetDebugName( eastl::string_view name ) -> DescriptorTableLayoutDescription&;

        /**
         * Sets the value handled by SetRegisterSpace.
         *
         * @param registerSpace Input value used by this operation.
         * @returns The result of SetRegisterSpace.
         */
        auto SetRegisterSpace( core::u32 registerSpace ) -> DescriptorTableLayoutDescription&;

        /**
         * Sets the value handled by SetVisibility.
         *
         * @param visibility Input value used by this operation.
         * @returns The result of SetVisibility.
         */
        auto SetVisibility( ShaderFlags visibility ) -> DescriptorTableLayoutDescription&;

        /**
         * Adds the supplied value through AddBindlessItem.
         *
         * @param item Input value used by this operation.
         * @returns The result of AddBindlessItem.
         */
        auto AddBindlessItem( const BindlessLayoutItem& item ) -> DescriptorTableLayoutDescription&;

        /**
         * Adds the supplied value through AddShader.
         *
         * @param shader Input value used by this operation.
         * @returns The result of AddShader.
         */
        auto AddShader( ShaderModuleHandle shader ) -> DescriptorTableLayoutDescription&;
    };

    /**
     * Resource values to write to a conventional binding table.
     */
    struct BindingTableDescription {
        eastl::vector<BindingTableItem> mBindings{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        // TODO: Move this to a reflection module
        // Vulkan for instance via the spirv_reflect library allows us to
        // introspect SPIRV binaries to know descriptor layouts, amongst other
        // information, this could be helpful to generate the appropriate
        // binding layouts from the RHI for our pipeline
        bool mUseReflection{};

        /**
         * Adds the supplied value through AddItem.
         *
         * @param value Input value used by this operation.
         * @returns The result of AddItem.
         */
        auto AddItem(const BindingTableItem& value) -> BindingTableDescription&;

        /**
         * Adds the supplied value through AddShader.
         *
         * @param shader Input value used by this operation.
         * @returns The result of AddShader.
         */
        auto AddShader( ShaderModuleHandle shader ) -> BindingTableDescription&;
    };

    /**
     * Mutable table of resources matching an @ref IBindingLayout.
     *
     * The layout is immutable, while the resources assigned to its slots may be updated.
     */
    class IBindingTable : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    protected:

        /**
         * Creates the backend binding-table allocation.
         */
        auto Initialize() -> void override = 0;

        /**
         * Releases the backend binding-table allocation.
         */
        auto Destroy() -> void override = 0;
    };

    using BindingTableHandle = core::Ref<IBindingTable>;

    /**
     * Resizable bindless form of an @ref IBindingTable.
     */
    class IDescriptorTable : public IBindingTable {
    public:

        /**
         * Returns the number of descriptors allocated for @p slot.
         *
         * @param slot Input value used by this operation.
         * @returns The result of GetCapacity.
         */
        MKT_NODISCARD virtual auto GetCapacity( core::u32 slot ) const -> core::u32 = 0;
    };

    using DescriptorTableHandle = core::Ref<IDescriptorTable>;

}

#endif//MIKOTO_RHI_DESCRIPTOR_HH
