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


#include <EASTL/array.h>
#include <EASTL/algorithm.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Assets/ShaderLibrary.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/SkyboxMaterial.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityEnvironmentModule.hh>

namespace mikoto::renderer {
    // Match CompatibilityEnvironment.slang.
    static constexpr core::u32 kSamplerSlot{ 3 };
    static constexpr core::u32 kFirstTextureSlot{ 9 };
    static constexpr core::u32 kEnvironmentAtlasSlot{ 17 };
    static constexpr core::u32 kEnvironmentBrdfSlot{ 18 };

    // Match the layout in CompatibilityIBL.slang.
    static constexpr core::u32 kEnvironmentGutter{ 1 };
    static constexpr core::u32 kSpecularLevels{ 5 };
    static constexpr rhi::Extent2D kEnvironmentTileSize{ .mWidth = 256, .mHeight = 128 };
    static constexpr rhi::Extent2D kBrdfLutSize{ .mWidth = 128, .mHeight = 128 };
    static constexpr core::usize kPanoramaSlot{};
    static constexpr core::usize kFirstFaceSlot{ 1 };
    static constexpr eastl::array kFaces{
        material::SkyboxFace::eRight, material::SkyboxFace::eLeft,
        material::SkyboxFace::eTop, material::SkyboxFace::eBottom,
        material::SkyboxFace::eBack, material::SkyboxFace::eFront
    };

    enum class EnvironmentSource : core::u32 {
        eNone,
        ePanorama,
        eFaces,
    };

    struct alignas( 16 ) EnvironmentBakeParameters {
        core::uint2 mTileSize{};
        core::f32 mSource{};
        core::f32 mLastSourceMip{};
    };

    static auto RecordEnvironmentBake( rhi::ICommandList* commands, rhi::TextureHandle texture, rhi::IPipeline& pipeline,
        const rhi::BindResourcesDescription& bindings, eastl::string_view name ) -> void {
        const rhi::Extent2D size{ .mWidth = texture->GetWidth(), .mHeight = texture->GetHeight() };
        auto pass{ rhi::RenderPassDescription{}
            .SetScopeName( name )
            .SetRenderArea( rhi::Rect{ core::as<core::i32>( size.mWidth ), core::as<core::i32>( size.mHeight ) } )
            .AddRenderTarget( texture, rhi::kColorTransparent ) };

        commands->SetTransition( rhi::TransitionDescription{}.AddTexture( texture.GetPtr(), rhi::ResourceStates::eRenderTarget ) );
        commands->BeginRenderPass( pass );
        SetCompatibilityViewport( commands, size );
        commands->BindPipeline( &pipeline );

        if ( !bindings.mResourceSets.empty() ) {
            commands->BindPipelineResources( bindings );
        }

        commands->Draw( rhi::DrawArguments{}.SetVertexCount( kFullscreenTriangleVertexCount ).SetInstanceCount( 1 ) );
        commands->EndRenderPass();
        commands->SetTransition( rhi::TransitionDescription{}.AddTexture( texture.GetPtr(), rhi::ResourceStates::eShaderResource ) );
    }

    auto CompatibilityEnvironment::DeclareBindings( rhi::BindingLayoutDescription& layout ) -> void {
        layout.AddItem( rhi::BindingLayoutItem::Sampler( kSamplerSlot ) );

        for ( core::u32 i{}; i < kTextureCount; ++i ) {
            layout.AddItem( rhi::BindingLayoutItem::TextureSRV( kFirstTextureSlot + i ) );
        }
    }

    auto CompatibilityEnvironment::AddBindings( rhi::BindingTableDescription& bindings ) const -> void {
        auto sampler{ mSampler };
        bindings.AddItem( rhi::BindingTableItem::Sampler( kSamplerSlot, sampler.GetPtr() ) );

        for ( core::u32 i{}; i < mTextures.size(); ++i ) {
            auto texture{ mTextures[i] ? mTextures[i] : mFallback };
            bindings.AddItem( rhi::BindingTableItem::TextureSRV( kFirstTextureSlot + i, texture.GetPtr() ) );
        }
    }

    auto CompatibilityEnvironment::DeclareLightingBindings( rhi::BindingLayoutDescription& layout ) -> void {
        layout.AddItem( rhi::BindingLayoutItem::TextureSRV( kEnvironmentAtlasSlot ) );
        layout.AddItem( rhi::BindingLayoutItem::TextureSRV( kEnvironmentBrdfSlot ) );
    }

    auto CompatibilityEnvironment::AddLightingBindings( rhi::BindingTableDescription& bindings ) const -> void {
        auto filtered{ mFilteredLighting ? mFilteredLighting : mFallback };
        auto brdf{ mBrdfLut ? mBrdfLut : mFallback };
        bindings.AddItem( rhi::BindingTableItem::TextureSRV( kEnvironmentAtlasSlot, filtered.GetPtr() ) );
        bindings.AddItem( rhi::BindingTableItem::TextureSRV( kEnvironmentBrdfSlot, brdf.GetPtr() ) );
    }

    auto CompatibilityEnvironmentModule::Initialize( rhi::IGpuDevice* device ) -> void {
        mDevice = device;
        mFallback = device->CreateTexture( rhi::TextureCreateDescription{}
            .SetName( "Compatibility texture fallback" )
            .SetWidth( 1 )
            .SetHeight( 1 )
            .SetDimensions( rhi::TextureDimension::eTexture2D )
            .SetFormat( rhi::Format::eRGBA8_UNORM )
            .SetUsage( rhi::TextureUsageFlagsBits::ShaderResource | rhi::TextureUsageFlagsBits::CopyDest ) );
        mSampler = device->CreateSampler( rhi::SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( rhi::SamplerWrapMode::eClampToEdge ) );

        auto layout{ rhi::BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility( rhi::ShaderFlagsBits::Pixel )
            .AddItem( rhi::BindingLayoutItem::ConstantBuffer( kCompatibilityFrameSlot ) ) };
        CompatibilityEnvironment::DeclareBindings( layout );
        mBindings = device->CreateBindingLayout( layout );
        mLayout = device->CreatePipelineLayout( rhi::PipelineLayoutCreateDescription{}.AddBindingLayout( mBindings ) );

        auto* library{ asset::AssetRegistryService::Get()->GetShaderLibrary() };
        mPipeline = device->CreatePipeline( rhi::GraphicsPipelineDescription{}
            .AddColorFormat( kCompatibilitySceneFormat )
            .SetDepthFormat( kCompatibilityDepthFormat )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetUseReflection( false )
            .SetPipelineLayout( mLayout )
            .AddShader( library->LoadShader( "../compatibility/slang/CompatibilitySkybox_Vert.slang", rhi::ShaderType::eVertex ) )
            .AddShader( library->LoadShader( "../compatibility/slang/CompatibilitySkybox_Frag.slang", rhi::ShaderType::ePixel ) ) );

        // The bake reads the same panorama or faces as the skybox, with its own constants.
        mBakeUniform = CreateReusableCompatibilityUniform( device, sizeof( EnvironmentBakeParameters ) );
        const auto bakePipeline{ rhi::GraphicsPipelineDescription{}
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetDepthTest( false )
            .SetDepthWrite( false )
            .SetUseReflection( false ) };
        auto prefilter{ bakePipeline };
        mBakePipeline = device->CreatePipeline( prefilter
            .AddColorFormat( kCompatibilitySceneFormat )
            .SetPipelineLayout( mLayout )
            .AddShader( library->LoadShader( "../compatibility/slang/CompatibilityFullscreen_Vert.slang", rhi::ShaderType::eVertex ) )
            .AddShader( library->LoadShader( "../compatibility/slang/CompatibilityEnvironmentBake_Frag.slang", rhi::ShaderType::ePixel ) ) );

        mBrdfLayout = device->CreatePipelineLayout( rhi::PipelineLayoutCreateDescription{} );
        auto lut{ bakePipeline };
        mBrdfPipeline = device->CreatePipeline( lut
            .AddColorFormat( rhi::Format::eRG16_FLOAT )
            .SetPipelineLayout( mBrdfLayout )
            .AddShader( library->LoadShader( "BRDFLut_Vert.slang", rhi::ShaderType::eVertex ) )
            .AddShader( library->LoadShader( "BRDFLut_Frag.slang", rhi::ShaderType::ePixel ) ) );

        if ( !mFallback || !mSampler || !mPipeline || !mBakePipeline || !mBrdfLayout || !mBrdfPipeline ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize the compatibility environment pass." );
        }
    }

    auto CompatibilityEnvironmentModule::Prepare( rhi::ICommandList* commands, const material::SkyboxMaterial* skybox,
        core::f32 strength, core::f32 blur ) -> CompatibilityEnvironment {
        MKT_BEGIN_PROFILER();

        if ( !mFallbackUploaded ) {
            constexpr eastl::array<core::u8, 4> white{ 255, 255, 255, 255 };
            commands->SetTransition( rhi::TransitionDescription{}.AddTexture( mFallback.GetPtr(), rhi::ResourceStates::eCopyDest ) );
            commands->Write( mFallback.GetPtr(), white.data(), white.size() );
            commands->SetTransition( rhi::TransitionDescription{}.AddTexture( mFallback.GetPtr(), rhi::ResourceStates::eShaderResource ) );
            mFallbackUploaded = true;
        }

        CompatibilityEnvironment environment{ .mFallback = mFallback, .mSampler = mSampler };
        EnvironmentSource source{};
        core::u32 mipCount{ 1 };

        if ( skybox ) {
            auto& panorama{ environment.mTextures[kPanoramaSlot] };
            panorama = skybox->GetEquirectangular();

            if ( panorama ) {
                source = EnvironmentSource::ePanorama;
                mipCount = eastl::max( mipCount, panorama->GetMipLevelCount() );
            }
            else {
                for ( core::usize i{}; i < kFaces.size(); ++i ) {
                    auto& face{ environment.mTextures[kFirstFaceSlot + i] };
                    face = skybox->GetFace( kFaces[i] );

                    if ( face ) {
                        source = EnvironmentSource::eFaces;
                        mipCount = eastl::max( mipCount, face->GetMipLevelCount() );
                    }
                }
            }
        }

        environment.mParameters = { strength, blur, core::as<core::f32>( mipCount - 1 ), core::as<core::f32>( source ) };
        environment.mHasSkybox = source != EnvironmentSource::eNone;

        if ( environment.mHasSkybox && strength > 0.0f ) {
            if ( !mFilteredLighting || mBakedSources != environment.mTextures ) {
                BakeLighting( commands, environment );
            }

            environment.mFilteredLighting = mFilteredLighting;
            environment.mBrdfLut = mBrdfLut;
        }
        else if ( !environment.mHasSkybox ) {
            mBakedSources = {};
            mFilteredLighting.Reset();
        }

        return environment;
    }

    auto CompatibilityEnvironmentModule::BakeLighting( rhi::ICommandList* commands, const CompatibilityEnvironment& environment ) -> void {
        MKT_BEGIN_PROFILER();

        const auto usage{ rhi::TextureUsageFlagsBits::RenderTarget | rhi::TextureUsageFlagsBits::ShaderResource };
        if ( !mBrdfLut ) {
            ResizeCompatibilityTarget( mDevice, mBrdfLut, kBrdfLutSize, rhi::Format::eRG16_FLOAT, usage, "Compatibility BRDF LUT" );
            RecordEnvironmentBake( commands, mBrdfLut, *mBrdfPipeline.GetPtr(), {}, "Compatibility BRDF LUT" );
        }

        // Write all roughness levels in one pass using the compatibility backends' 2D targets.
        const rhi::Extent2D atlasSize{
            .mWidth = kEnvironmentTileSize.mWidth + 2 * kEnvironmentGutter,
            .mHeight = ( kEnvironmentTileSize.mHeight + 2 * kEnvironmentGutter ) * ( kSpecularLevels + 1 )
        };
        ResizeCompatibilityTarget( mDevice, mFilteredLighting, atlasSize, kCompatibilitySceneFormat, usage, "Compatibility environment lighting" );

        const EnvironmentBakeParameters parameters{
            .mTileSize = { kEnvironmentTileSize.mWidth, kEnvironmentTileSize.mHeight },
            .mSource = environment.mParameters.w,
            .mLastSourceMip = environment.mParameters.z
        };
        auto uniform{ WriteCompatibilityUniform( mDevice, commands, mBakeUniform, &parameters, sizeof( parameters ) ) };
        auto description{ rhi::BindingTableDescription{}
            .AddItem( rhi::BindingTableItem::ConstantBuffer( kCompatibilityFrameSlot, uniform.GetPtr() ) ) };
        environment.AddBindings( description );
        auto table{ mDevice->CreateBindingTable( description, mBindings ) };

        const auto bindings{ rhi::BindResourcesDescription{}
            .SetBindPoint( rhi::PipelineType::eGraphics )
            .SetPipelineLayout( mLayout.GetPtr() )
            .AddResourceSet( 0, table.GetPtr() ) };
        RecordEnvironmentBake( commands, mFilteredLighting, *mBakePipeline.GetPtr(), bindings, "Compatibility environment lighting" );
        mBakedSources = environment.mTextures;
    }

    auto CompatibilityEnvironmentModule::RecordSkybox( rhi::ICommandList* commands, const CompatibilityEnvironment& environment,
        rhi::IBuffer& frameUniform ) -> void {
        MKT_BEGIN_PROFILER();

        auto description{ rhi::BindingTableDescription{}
            .AddItem( rhi::BindingTableItem::ConstantBuffer( kCompatibilityFrameSlot, &frameUniform ) ) };
        environment.AddBindings( description );
        auto table{ mDevice->CreateBindingTable( description, mBindings ) };

        commands->BindPipeline( mPipeline.GetPtr() );
        commands->BindPipelineResources( rhi::BindResourcesDescription{}
            .SetBindPoint( rhi::PipelineType::eGraphics )
            .SetPipelineLayout( mLayout.GetPtr() )
            .AddResourceSet( 0, table.GetPtr() ) );
        commands->Draw( rhi::DrawArguments{}.SetVertexCount( kFullscreenTriangleVertexCount ).SetInstanceCount( 1 ) );
    }
}// namespace mikoto::renderer
