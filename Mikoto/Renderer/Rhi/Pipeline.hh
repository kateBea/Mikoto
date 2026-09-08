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

#ifndef MIKOTO_RHI_PIPELINE_HH
#define MIKOTO_RHI_PIPELINE_HH

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Memory/BufferSpan.hh>
#include <Core/ResourcePool.hh>

#include <Assets/Image.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>
#include <Renderer/Rhi/Descriptor.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    /**
     * Describes vertex-buffer bindings and shader input attributes.
     */
    struct InputLayoutCreateDescription {
        ShaderModuleHandle mShaderModule{};
        eastl::fixed_vector<VertexBindingDescription, kMaxVertexBindings> mVertexBindingDescriptions{};
        eastl::fixed_vector<VertexAttributeDescription, kMaxVertexAttributes> mVertexAttributeDescriptions{};

        // Backends offer support for shader reflection
        // which simplifies binding layout creation
        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        /**
         * Sets the value handled by SetShader.
         *
         * @param shader Input value used by this operation.
         * @returns The result of SetShader.
         */
        auto SetShader( ShaderModuleHandle shader ) -> InputLayoutCreateDescription&;

        /**
         * Sets the value handled by SetBindings.
         *
         * @param items Input value used by this operation.
         * @returns The result of SetBindings.
         */
        auto SetBindings( eastl::span<const VertexBindingDescription> items ) -> InputLayoutCreateDescription&;

        /**
         * Sets the value handled by SetAttributes.
         *
         * @param items Input value used by this operation.
         * @returns The result of SetAttributes.
         */
        auto SetAttributes( eastl::span<const VertexAttributeDescription> items ) -> InputLayoutCreateDescription&;

        /**
         * Adds the supplied value through PushBinding.
         *
         * @param desc Input value used by this operation.
         * @returns The result of PushBinding.
         */
        auto PushBinding( const VertexBindingDescription& desc ) -> InputLayoutCreateDescription&;

        /**
         * Adds the supplied value through PushAttribute.
         *
         * @param desc Input value used by this operation.
         * @returns The result of PushAttribute.
         */
        auto PushAttribute( const VertexAttributeDescription& desc ) -> InputLayoutCreateDescription&;
    };


    /**
     * Backend-specific representation of a vertex input layout.
     */
    class IInputLayout : public DeviceObject {
    public:

        /**
         * Returns the number of declared vertex attributes.
         * @returns The result of GetNumAttributes.
         */
        MKT_NODISCARD virtual auto GetNumAttributes() const -> core::u32 = 0;

        /**
         * Returns the vertex attribute at @p index.
         *
         * @param index Input value used by this operation.
         * @returns The result of GetAttributeDescription.
         */
        MKT_NODISCARD virtual auto GetAttributeDescription(core::u32 index) const -> const VertexAttributeDescription& = 0;

        using DeviceObject::Initialize;
    };

    using InputLayoutHandle = core::Ref<IInputLayout>;

    /**
     * Layout of a conventional or bindless resource-binding set.
     */
    class IBindingLayout : public DeviceObject {
    public:

        /**
         * Returns the API register space or descriptor-set index.
         * @returns The result of GetRegisterSpace.
         */
        MKT_NODISCARD virtual auto GetRegisterSpace() const -> core::u32 = 0;

        /**
         * Returns whether this layout represents bindless descriptor arrays.
         * @returns The result of IsBindless.
         */
        MKT_NODISCARD virtual auto IsBindless() const -> bool = 0;

        using DeviceObject::Initialize;

    protected:

        /**
         * Creates the native binding-layout object.
         */
        auto Initialize() -> void override = 0;

        /**
         * Releases the native binding-layout object.
         */
        auto Destroy() -> void override = 0;
    };

    using BindingLayoutHandle = core::Ref<IBindingLayout>;

    /**
     * Combines binding layouts and push-constant visibility for a pipeline.
     */
    class IPipelineLayout : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    protected:

        /**
         * Creates the native pipeline-layout object.
         */
        auto Initialize() -> void override = 0;

        /**
         * Releases the native pipeline-layout object.
         */
        auto Destroy() -> void override = 0;
    };

    using PipelineLayoutHandle = core::Ref<IPipelineLayout>;

    /**
     * Creation parameters for an @ref IPipelineLayout.
     */
    struct PipelineLayoutCreateDescription {
        core::usize mPushConstantsSize{ kMaxPushConstantSize };
        ShaderFlags mPushConstantsVisibility{ ShaderFlagsBits::All };
        eastl::fixed_vector<BindingLayoutHandle, kMaxBindingLayouts> mBindingLayouts{};

        /**
         * Sets the value handled by SetPushConstantSize.
         *
         * @param size Input value used by this operation.
         * @returns The result of SetPushConstantSize.
         */
        auto SetPushConstantSize( core::usize size ) -> PipelineLayoutCreateDescription&;

        /**
         * Sets the value handled by SetPushConstantsVisibility.
         *
         * @param stage Input value used by this operation.
         * @returns The result of SetPushConstantsVisibility.
         */
        auto SetPushConstantsVisibility( ShaderFlags stage ) -> PipelineLayoutCreateDescription&;

        /**
         * Adds the supplied value through AddBindingLayout.
         *
         * @param layout Input value used by this operation.
         * @returns The result of AddBindingLayout.
         */
        auto AddBindingLayout( BindingLayoutHandle layout ) -> PipelineLayoutCreateDescription&;
    };

    /**
     * Base interface for immutable graphics and compute pipelines.
     */
    class IPipeline : public DeviceObject {
    public:

        /**
         * Performs the operation represented by IPipeline.
         *
         * @returns The result of IPipeline.
         */
        ~IPipeline() override = default;

        /**
         * Returns whether this is a graphics or compute pipeline.
         * @returns The result of GetPipelineType.
         */
        MKT_NODISCARD auto GetPipelineType() const -> PipelineType;

        /**
         * Returns the layout used to bind resources to this pipeline.
         * @returns The result of GetPipelineLayout.
         */
        MKT_NODISCARD virtual auto GetPipelineLayout() const -> PipelineLayoutHandle = 0;

    protected:

        /**
         * Constructs a pipeline wrapper.
         *
         * @param pipelineType Type of pipeline being created.
         */
        explicit IPipeline( PipelineType pipelineType);

    protected:
        const PipelineType mPipelineType{ PipelineType::eInvalid };
    };

    using PipelineHandle = core::Ref<IPipeline>;

    /**
     * Shader and layout configuration for a compute pipeline.
     */
    struct ComputePipelineDescription {
        ShaderModuleHandle mStage{};
        PipelineLayoutHandle mPipelineLayout{};

        bool mUseReflection{ false };

        /**
         * Sets the value handled by SetUseReflection.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetUseReflection.
         */
        auto SetUseReflection( bool value ) -> ComputePipelineDescription&;

        /**
         * Sets the value handled by SetComputeStage.
         *
         * @param handle Input value used by this operation.
         * @returns The result of SetComputeStage.
         */
        auto SetComputeStage( ShaderModuleHandle handle ) -> ComputePipelineDescription&;

        /**
         * Sets the value handled by SetPipelineLayout.
         *
         * @param handle Input value used by this operation.
         * @returns The result of SetPipelineLayout.
         */
        auto SetPipelineLayout( PipelineLayoutHandle handle ) -> ComputePipelineDescription&;
    };

    /**
     * Immutable graphics pipeline state and attached shader modules.
     */
    struct GraphicsPipelineDescription {
        bool mEnableDepthTest{ true };
        bool mEnableDepthWrite{ true };
        bool mEnableStencilTest{ false };

        bool mEnableAlphaBlending{ true };
        bool mEnableSampleRateShading{ false };

        bool mUseReflection{ false };

        core::f32 mPolygonLineWidth{ 1.0f };

        Multisampling mMultisampling{ Multisampling::eMsaaX1 };
        CullMode mCullMode{ CullMode::eNone };
        PolygonMode mPolygonMode{ PolygonMode::eFill };
        PrimitiveTopology mPrimitiveTopology{ PrimitiveTopology::eTriangleList };
        WindingOrder mWindingOrder{ WindingOrder::eCounterClockwise };
        CompareOp mDepthCompareOp{ CompareOp::eLessOrEqual };

        // Resources
        InputLayoutHandle mInputLayout{};
        PipelineLayoutHandle mPipelineLayout{};

        // Shaders
        eastl::fixed_hash_map<ShaderType, ShaderModuleHandle, kMaxShaders> mShaders{};

        Format mDepthFormat{};
        eastl::fixed_vector<Format, kMaxColorFormats> mColorFormats{};

        /**
         * Sets the value handled by SetPipelineLayout.
         *
         * @param handle Input value used by this operation.
         * @returns The result of SetPipelineLayout.
         */
        auto SetPipelineLayout( PipelineLayoutHandle handle ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetInputLayout.
         *
         * @param handle Input value used by this operation.
         * @returns The result of SetInputLayout.
         */
        auto SetInputLayout( InputLayoutHandle handle ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetPolygonMode.
         *
         * @param mode Input value used by this operation.
         * @returns The result of SetPolygonMode.
         */
        auto SetPolygonMode( PolygonMode mode ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetTopology.
         *
         * @param topology Input value used by this operation.
         * @returns The result of SetTopology.
         */
        auto SetTopology( PrimitiveTopology topology ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetMultisampling.
         *
         * @param msaa Input value used by this operation.
         * @returns The result of SetMultisampling.
         */
        auto SetMultisampling( Multisampling msaa ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetUseReflection.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetUseReflection.
         */
        auto SetUseReflection( bool value ) -> GraphicsPipelineDescription&;

        /**
         * Adds the supplied value through AddShader.
         *
         * @param handle Input value used by this operation.
         * @returns The result of AddShader.
         */
        auto AddShader( ShaderModuleHandle handle ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetCullMode.
         *
         * @param mode Input value used by this operation.
         * @returns The result of SetCullMode.
         */
        auto SetCullMode( CullMode mode ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetBlendEnable.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetBlendEnable.
         */
        auto SetBlendEnable( bool value ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetDepthTest.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetDepthTest.
         */
        auto SetDepthTest( bool value ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetDepthWrite.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetDepthWrite.
         */
        auto SetDepthWrite( bool value ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetDepthFormat.
         *
         * @param format Input value used by this operation.
         * @returns The result of SetDepthFormat.
         */
        auto SetDepthFormat( Format format ) -> GraphicsPipelineDescription&;

        /**
         * Adds the supplied value through AddColorFormat.
         *
         * @param format Input value used by this operation.
         * @returns The result of AddColorFormat.
         */
        auto AddColorFormat( Format format ) -> GraphicsPipelineDescription&;

        /**
         * Sets the value handled by SetWindingOrder.
         *
         * @param order Input value used by this operation.
         * @returns The result of SetWindingOrder.
         */
        auto SetWindingOrder( WindingOrder order ) -> GraphicsPipelineDescription&;
    };

    /**
     * Immutable graphics pipeline created from @ref GraphicsPipelineDescription.
     */
    class IGraphicsPipeline : public IPipeline {
    public:

        /**
         * Creates a graphics-pipeline wrapper.
         *
         * @param desc Immutable graphics-pipeline description.
         */
        explicit IGraphicsPipeline(const GraphicsPipelineDescription& desc);

        /**
         * Returns the immutable description used to create this pipeline.
         * @returns The result of GetDescription.
         */
        MKT_NODISCARD auto GetDescription() const noexcept -> const GraphicsPipelineDescription&;

        /**
         * Returns the layout used by this graphics pipeline.
         *
         * @returns The pipeline layout.
         */
        MKT_NODISCARD auto GetPipelineLayout() const -> PipelineLayoutHandle override;

    protected:
        GraphicsPipelineDescription mDesc{};
    };

    /**
     * Immutable compute pipeline created from @ref ComputePipelineDescription.
     */
    class IComputePipeline : public IPipeline {
    public:

        /**
         * Creates a compute-pipeline wrapper.
         *
         * @param desc Immutable compute-pipeline description.
         */
        explicit IComputePipeline(const ComputePipelineDescription& desc);

        /**
         * Returns the immutable description used to create this pipeline.
         * @returns The result of GetDescription.
         */
        MKT_NODISCARD auto GetDescription() const noexcept -> const ComputePipelineDescription&;

        /**
         * Returns the layout used by this compute pipeline.
         *
         * @returns The pipeline layout.
         */
        MKT_NODISCARD auto GetPipelineLayout() const -> PipelineLayoutHandle override;

    protected:
        ComputePipelineDescription mDesc{};
    };
}// namespace mikoto::renderer::rhi

#endif//MIKOTO_RHI_PIPELINE_HH
