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


#include <Core/Exception.hh>

#include <Assets/ShaderLibrary.hh>
#include <Assets/AssetRegistryService.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>

namespace mikoto::renderer {
    // The fullscreen shaders use the same fixed slots.
    static constexpr core::u32 kFullscreenUniformSlot{};
    static constexpr core::u32 kFullscreenSourceSlot{ 1 };
    static constexpr core::u32 kFullscreenSamplerSlot{ 2 };
    static constexpr core::u32 kFullscreenExtraSourceSlot{ 3 };

    MKT_NODISCARD static auto CreateUniform( rhi::IGpuDevice* device, core::usize size ) -> rhi::BufferHandle {
        auto buffer{ device->CreateBuffer( rhi::BufferCreateDescription{}
            .SetHeapType( rhi::HeapType::eUpload )
            .SetCpuAccessType( rhi::AccessType::eWrite )
            .SetInitialState( rhi::ResourceStates::eConstantBuffer )
            .SetBufferUsage( rhi::BufferUsageFlagsBits::Constant | rhi::BufferUsageFlagsBits::CopyDest )
            .SetResourceType( rhi::ResourceType::eConstantBuffer )
            .SetByteSize( size ) ) };

        if ( !buffer ) {
            MKT_THROW_RUNTIME_ERROR( "Could not create a compatibility pass uniform buffer." );
        }

        return buffer;
    }

    auto CreateReusableCompatibilityUniform( rhi::IGpuDevice* device, core::usize size ) -> rhi::BufferHandle {
        // Reuse the allocation only when earlier draws keep seeing their own uniform data.
        if ( device->IsFeatureSupported( rhi::GpuFeatureSupportFlagsBits::OrderedUniformBufferWrites ) ) {
            return CreateUniform( device, size );
        }

        return {};
    }

    auto WriteCompatibilityUniform( rhi::IGpuDevice* device, rhi::ICommandList* commands,
        rhi::BufferHandle reusable, const void* data, core::usize size ) -> rhi::BufferHandle {
        // A fresh upload buffer preserves the bytes until the recorded binding retires.
        auto uniform{ reusable ? eastl::move( reusable ) : CreateUniform( device, size ) };
        commands->Write( uniform.GetPtr(), data, size );
        return uniform;
    }

    auto ResizeCompatibilityTarget(
        rhi::IGpuDevice* device,
        rhi::TextureHandle& target,
        rhi::Extent2D size, rhi::Format format,
        rhi::TextureUsageFlags usage, eastl::string_view name ) -> void {
        if ( target && target->GetWidth() == size.mWidth && target->GetHeight() == size.mHeight ) {
            return;
        }

        auto replacement{ device->CreateTexture( rhi::TextureCreateDescription{}
            .SetName( name )
            .SetWidth( size.mWidth )
            .SetHeight( size.mHeight )
            .SetDimensions( rhi::TextureDimension::eTexture2D )
            .SetFormat( format )
            .SetUsage( usage ) ) };

        if ( !replacement ) {
            MKT_THROW_RUNTIME_ERROR( "Could not create a compatibility render target." );
        }

        target = eastl::move( replacement );
    }

    auto SetCompatibilityViewport( rhi::ICommandList* commands, rhi::Extent2D size ) -> void {
        commands->SetViewportState( rhi::ViewportState{}
            .AddViewportAndScissorRect( rhi::Viewport{ core::as<core::f32>( size.mWidth ), core::as<core::f32>( size.mHeight ) } ) );
    }

    auto CompatibilityFullscreenPass::Initialize( rhi::IGpuDevice* device, eastl::string_view fragmentShader,
        core::usize uniformSize, bool extraSource ) -> void {
        mDevice = device;
        mUniform = CreateReusableCompatibilityUniform( device, uniformSize );
        mSampler = device->CreateSampler( rhi::SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( rhi::SamplerWrapMode::eClampToEdge ) );

        auto bindings{ rhi::BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility( rhi::ShaderFlagsBits::Pixel )
            .AddItem( rhi::BindingLayoutItem::ConstantBuffer( kFullscreenUniformSlot ) )
            .AddItem( rhi::BindingLayoutItem::TextureSRV( kFullscreenSourceSlot ) )
            .AddItem( rhi::BindingLayoutItem::Sampler( kFullscreenSamplerSlot ) ) };
        if ( extraSource ) {
            bindings.AddItem( rhi::BindingLayoutItem::TextureSRV( kFullscreenExtraSourceSlot ) );
        }

        mBindings = device->CreateBindingLayout( bindings );
        mLayout = device->CreatePipelineLayout( rhi::PipelineLayoutCreateDescription{}.AddBindingLayout( mBindings ) );

        auto* library{ asset::AssetRegistryService::Get()->GetShaderLibrary() };
        mPipeline = device->CreatePipeline( rhi::GraphicsPipelineDescription{}
            .AddColorFormat( kCompatibilityOutputFormat )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetUseReflection( false )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetPipelineLayout( mLayout )
            .AddShader( library->LoadShader( "../compatibility/slang/CompatibilityFullscreen_Vert.slang", rhi::ShaderType::eVertex ) )
            .AddShader( library->LoadShader( fragmentShader, rhi::ShaderType::ePixel ) ) );

        if ( !mSampler || !mPipeline ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize a compatibility fullscreen pass." );
        }
    }

    auto CompatibilityFullscreenPass::Record( rhi::ICommandList* commands, eastl::string_view name, rhi::TextureHandle source,
        rhi::TextureHandle output, const void* parameters, core::usize parameterSize, const rhi::TextureHandle& extraSource ) -> void {
        auto uniform{ WriteCompatibilityUniform( mDevice, commands, mUniform, parameters, parameterSize ) };
        auto description{ rhi::BindingTableDescription{}
            .AddItem( rhi::BindingTableItem::ConstantBuffer( kFullscreenUniformSlot, uniform.GetPtr() ) )
            .AddItem( rhi::BindingTableItem::TextureSRV( kFullscreenSourceSlot, source.GetPtr() ) )
            .AddItem( rhi::BindingTableItem::Sampler( kFullscreenSamplerSlot, mSampler.GetPtr() ) ) };
        if ( extraSource ) {
            auto texture{ extraSource };
            description.AddItem( rhi::BindingTableItem::TextureSRV( kFullscreenExtraSourceSlot, texture.GetPtr() ) );
        }
        auto bindings{ mDevice->CreateBindingTable( description, mBindings ) };

        const rhi::Extent2D size{ .mWidth = output->GetWidth(), .mHeight = output->GetHeight() };
        auto pass{ rhi::RenderPassDescription{}
            .SetScopeName( name )
            .SetRenderArea( rhi::Rect{ core::as<core::i32>( size.mWidth ), core::as<core::i32>( size.mHeight ) } )
            .AddRenderTarget( output, {} ) };

        commands->SetTransition( rhi::TransitionDescription{}
            .AddTexture( source.GetPtr(), rhi::ResourceStates::eShaderResource )
            .AddTexture( output.GetPtr(), rhi::ResourceStates::eRenderTarget ) );
        commands->BeginRenderPass( pass );
        SetCompatibilityViewport( commands, size );

        commands->BindPipeline( mPipeline.GetPtr() );
        commands->BindPipelineResources( rhi::BindResourcesDescription{}
            .SetBindPoint( rhi::PipelineType::eGraphics )
            .SetPipelineLayout( mLayout.GetPtr() )
            .AddResourceSet( 0, bindings.GetPtr() ) );
        commands->Draw( rhi::DrawArguments{}.SetVertexCount( kFullscreenTriangleVertexCount ).SetInstanceCount( 1 ) );

        commands->EndRenderPass();
        commands->SetTransition( rhi::TransitionDescription{}.AddTexture( output.GetPtr(), rhi::ResourceStates::eShaderResource ) );
    }
}// namespace mikoto::renderer
