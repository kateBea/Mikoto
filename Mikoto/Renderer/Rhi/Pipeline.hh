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

#include <Memory/BufferSpan.hh>

#include <Assets/Image.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>
#include <Renderer/Rhi/Descriptor.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    struct InputLayoutCreateDescription {
        ShaderModuleHandle mShaderModule{};
        eastl::fixed_vector<VertexBindingDescription, kMaxVertexBindings> mVertexBindingDescriptions{};
        eastl::fixed_vector<VertexAttributeDescription, kMaxVertexAttributes> mVertexAttributeDescriptions{};

        // Backends offer support for shader reflection
        // which simplifies binding layout creation
        bool mUseReflection{};
        eastl::fixed_vector<ShaderModuleHandle, kMaxShaders> mShaders{};

        auto SetShader( ShaderModuleHandle shader ) -> InputLayoutCreateDescription&;

        auto SetBindings( eastl::span<const VertexBindingDescription> items ) -> InputLayoutCreateDescription&;
        auto SetAttributes( eastl::span<const VertexAttributeDescription> items ) -> InputLayoutCreateDescription&;

        auto PushBinding( const VertexBindingDescription& desc ) -> InputLayoutCreateDescription&;
        auto PushAttribute( const VertexAttributeDescription& desc ) -> InputLayoutCreateDescription&;
    };


    class IInputLayout : public DeviceObject {
    public:
        MKT_NODISCARD virtual auto GetNumAttributes() const -> core::u32 = 0;
        MKT_NODISCARD virtual auto GetAttributeDescription(core::u32 index) const -> const VertexAttributeDescription& = 0;

        using DeviceObject::Initialize;
    };

    using InputLayoutHandle = core::Ref<IInputLayout>;

    class IBindingLayout : public DeviceObject {
    public:

        MKT_NODISCARD virtual auto GetRegisterSpace() const -> core::u32 = 0;
        MKT_NODISCARD virtual auto IsBindless() const -> bool = 0;

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using BindingLayoutHandle = core::Ref<IBindingLayout>;

    class IPipelineLayout : public DeviceObject {
    public:

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;
    };

    using PipelineLayoutHandle = core::Ref<IPipelineLayout>;

    struct PipelineLayoutCreateDescription {
        core::usize mPushConstantsSize{ kMaxPushConstantSize };
        ShaderFlags mPushConstantsVisibility{ ShaderFlagsBits::kAll };
        eastl::fixed_vector<BindingLayoutHandle, kMaxBindingLayouts> mBindingLayouts{};

        auto SetPushConstantSize( core::usize size ) -> PipelineLayoutCreateDescription&;
        auto SetPushConstantsVisibility( ShaderFlags stage ) -> PipelineLayoutCreateDescription&;
        auto AddBindingLayout( BindingLayoutHandle layout ) -> PipelineLayoutCreateDescription&;
    };

    class IPipeline : public DeviceObject {
    public:
        ~IPipeline() override = default;

        MKT_NODISCARD auto GetPipelineType() const -> PipelineType;

        MKT_NODISCARD virtual auto GetPipelineLayout() const -> PipelineLayoutHandle = 0;

    protected:
        explicit IPipeline( PipelineType pipelineType);

    protected:
        const PipelineType mPipelineType{ PipelineType::eInvalid };
    };

    using PipelineHandle = core::Ref<IPipeline>;

    struct ComputePipelineDescription {
        ShaderModuleHandle mStage{};
        PipelineLayoutHandle mPipelineLayout{};

        bool mUseReflection{ false };

        auto SetUseReflection( bool value ) -> ComputePipelineDescription&;
        auto SetComputeStage( ShaderModuleHandle handle ) -> ComputePipelineDescription&;
        auto SetPipelineLayout( PipelineLayoutHandle handle ) -> ComputePipelineDescription&;
    };

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
        DepthCompareOp mDepthCompareOp{ DepthCompareOp::eLessOrEqual };

        // Resources
        InputLayoutHandle mInputLayout{};
        PipelineLayoutHandle mPipelineLayout{};

        // Shaders
        eastl::fixed_hash_map<ShaderType, ShaderModuleHandle, kMaxShaders> mShaders{};

        Format mDepthFormat{};
        eastl::fixed_vector<Format, kMaxColorFormats> mColorFormats{};

        auto SetPipelineLayout( PipelineLayoutHandle handle ) -> GraphicsPipelineDescription&;
        auto SetInputLayout( InputLayoutHandle handle ) -> GraphicsPipelineDescription&;

        auto SetPolygonMode( PolygonMode mode ) -> GraphicsPipelineDescription&;
        auto SetTopology( PrimitiveTopology topology ) -> GraphicsPipelineDescription&;
        auto SetMultisampling( Multisampling msaa ) -> GraphicsPipelineDescription&;

        auto SetUseReflection( bool value ) -> GraphicsPipelineDescription&;

        auto AddShader( ShaderModuleHandle handle ) -> GraphicsPipelineDescription&;

        auto SetCullMode( CullMode mode ) -> GraphicsPipelineDescription&;

        auto SetBlendEnable( bool value ) -> GraphicsPipelineDescription&;

        auto SetDepthTest( bool value ) -> GraphicsPipelineDescription&;
        auto SetDepthWrite( bool value ) -> GraphicsPipelineDescription&;
        auto SetDepthFormat( Format format ) -> GraphicsPipelineDescription&;

        auto AddColorFormat( Format format ) -> GraphicsPipelineDescription&;
        auto SetWindingOrder( WindingOrder order ) -> GraphicsPipelineDescription&;
    };

    // Pipelines are immutable objects, this is because Vulkan
    // and DirectX12 do not behave like DirecX11 where you set the state on
    // the fly. That is why there are no setters except fot certain features
    // that we know for sure all backends support
    class IGraphicsPipeline : public IPipeline {
    public:
        explicit IGraphicsPipeline(const GraphicsPipelineDescription& desc);

        MKT_NODISCARD auto GetDescription() const noexcept -> const GraphicsPipelineDescription&;

        MKT_NODISCARD auto GetPipelineLayout() const -> PipelineLayoutHandle override;

    protected:
        GraphicsPipelineDescription mDesc{};
    };

    class IComputePipeline : public IPipeline {
    public:
        explicit IComputePipeline(const ComputePipelineDescription& desc);

        MKT_NODISCARD auto GetDescription() const noexcept -> const ComputePipelineDescription&;

        MKT_NODISCARD auto GetPipelineLayout() const -> PipelineLayoutHandle override;

    protected:
        ComputePipelineDescription mDesc{};
    };
}// namespace mikoto::renderer::rhi

#endif//MIKOTO_RHI_PIPELINE_HH
