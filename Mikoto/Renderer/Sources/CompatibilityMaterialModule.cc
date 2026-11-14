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

#include <glm/matrix.hpp>

#include <Core/Exception.hh>

#include <Material/SurfaceMaterial.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityMaterialModule.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityEnvironmentModule.hh>

namespace mikoto::renderer {
    // Match the draw uniform and fixed slots in CompatibilitySurface.slang and CompatibilityPBR_Frag.slang.
    static constexpr core::u32 kDrawSlot{ 1 };
    static constexpr core::u32 kSamplerSlot{ 2 };
    static constexpr core::u32 kFirstTextureSlot{ 4 };
    static constexpr core::u32 kShadowMapSlot{ 16 };
    static constexpr core::u32 kDoubleSided{ 1u };
    static constexpr core::u32 kMirrored{ 2u };
    static constexpr core::u32 kBaseColorSRGB{ 1u };
    static constexpr core::u32 kEmissiveSRGB{ 2u };
    static constexpr eastl::array kMaps{
        material::MapType::eBaseColor, material::MapType::eMetallicRoughness, material::MapType::eNormal,
        material::MapType::eAmbientOcclusion, material::MapType::eEmissive
    };

    struct alignas( 16 ) CompatibilityDrawParameters {
        core::float4x4 mWorld{ 1.0f };
        core::float4x4 mNormalMatrix{ 1.0f };
        core::float4 mBaseColor{ 1.0f };
        core::float4 mEmission{};
        core::float4 mSurface{ 0.0f, 1.0f, 0.0f, 0.0f };
        core::uint4 mObject{};
        core::float4 mUVTransform{ 1.0f, 1.0f, 0.0f, 0.0f };
        core::uint4 mUVSets{};
        core::uint4 mMaterial{};
    };

    auto CompatibilityMaterialModule::Initialize( rhi::IGpuDevice& device ) -> void {
        mDevice = &device;
        mUniform = CreateReusableCompatibilityUniform( device, sizeof( CompatibilityDrawParameters ) );
        mSampler = device.CreateSampler( rhi::SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( rhi::SamplerWrapMode::eRepeat ) );

        auto layout{ rhi::BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility( rhi::ShaderFlagsBits::Vertex | rhi::ShaderFlagsBits::Pixel )
            .AddItem( rhi::BindingLayoutItem::ConstantBuffer( kCompatibilityFrameSlot ) )
            .AddItem( rhi::BindingLayoutItem::ConstantBuffer( kDrawSlot ) )
            .AddItem( rhi::BindingLayoutItem::Sampler( kSamplerSlot ) ) };

        for ( core::u32 i{}; i < kMaps.size(); ++i ) {
            layout.AddItem( rhi::BindingLayoutItem::TextureSRV( kFirstTextureSlot + i ) );
        }

        CompatibilityEnvironment::DeclareBindings( layout );
        CompatibilityEnvironment::DeclareLightingBindings( layout );
        layout.AddItem( rhi::BindingLayoutItem::TextureSRV( kShadowMapSlot ) );
        mBindings = device.CreateBindingLayout( layout );
        mLayout = device.CreatePipelineLayout( rhi::PipelineLayoutCreateDescription{}.AddBindingLayout( mBindings ) );

        if ( !mSampler || !mLayout ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize compatibility material bindings." );
        }
    }

    auto CompatibilityMaterialModule::BeginFrame() -> void {
        // The frame uniform and environment can change between views.
        mFrameTables.clear();
    }

    auto CompatibilityMaterialModule::GetLayout() const -> const rhi::PipelineLayoutHandle& {
        return mLayout;
    }

    auto CompatibilityMaterialModule::CreateBindings( const material::SurfaceMaterial* surface, const CompatibilityEnvironment& environment,
        rhi::IBuffer& frameUniform, rhi::IBuffer& drawUniform, const rhi::TextureHandle& shadowMap ) -> rhi::BindingTableHandle {
        auto description{ rhi::BindingTableDescription{}
            .AddItem( rhi::BindingTableItem::ConstantBuffer( kCompatibilityFrameSlot, &frameUniform ) )
            .AddItem( rhi::BindingTableItem::ConstantBuffer( kDrawSlot, &drawUniform ) )
            .AddItem( rhi::BindingTableItem::Sampler( kSamplerSlot, mSampler.GetPtr() ) ) };

        for ( core::u32 i{}; i < kMaps.size(); ++i ) {
            auto texture{ surface ? surface->GetTexture( kMaps[i] ) : rhi::TextureHandle{} };
            if ( !texture ) {
                texture = environment.mFallback;
            }

            description.AddItem( rhi::BindingTableItem::TextureSRV( kFirstTextureSlot + i, texture.GetPtr() ) );
        }

        environment.AddBindings( description );
        environment.AddLightingBindings( description );
        auto depthMap{ shadowMap ? shadowMap : environment.mFallback };
        description.AddItem( rhi::BindingTableItem::TextureSRV( kShadowMapSlot, depthMap.GetPtr() ) );
        return mDevice->CreateBindingTable( description, mBindings );
    }

    auto CompatibilityMaterialModule::Bind( rhi::ICommandList& commands, const material::SurfaceMaterial* surface, const core::float4x4& world,
        const core::float4x4& normalMatrix, const CompatibilityEnvironment& environment, rhi::IBuffer& frameUniform,
        core::u32 id, bool highlighted, const rhi::TextureHandle& shadowMap, bool transparentPass ) -> void {
        CompatibilityDrawParameters parameters{
            .mWorld = world,
            .mNormalMatrix = normalMatrix,
            .mObject = { id, highlighted ? 1u : 0u, 0u, transparentPass ? 1u : 0u },
            .mMaterial = { 0u, glm::determinant( core::float3x3{ world } ) < 0.0f ? kMirrored : 0u, 0u, 0u }
        };

        if ( surface ) {
            parameters.mBaseColor = surface->GetBaseColorFactor();
            parameters.mObject.z = core::as<core::u32>( surface->GetAlphaMask() );
            parameters.mEmission = core::float4{ surface->GetEmissiveFactor() * surface->GetEmissiveStrength(), surface->GetOcclusionStrength() };
            parameters.mUVTransform = surface->GetUvTransform();
            parameters.mUVSets = {
                core::as<core::u32>( surface->GetBaseColorTextureSet() ),
                core::as<core::u32>( surface->GetMetallicRoughnessTextureSet() ),
                core::as<core::u32>( surface->GetNormalTextureSet() ),
                core::as<core::u32>( surface->GetOcclusionTextureSet() )
            };
            parameters.mMaterial.x = core::as<core::u32>( surface->GetEmissiveTextureSet() );
            parameters.mMaterial.y |= surface->IsDoubleSided() ? kDoubleSided : 0u;
            parameters.mMaterial.z = core::as<core::u32>( surface->GetShadingModel() );

            const auto base{ surface->GetTexture( material::MapType::eBaseColor ) };
            const auto emissive{ surface->GetTexture( material::MapType::eEmissive ) };
            if ( base && rhi::GetFormatInfo( base->GetFormat() ).mIsSRGB ) {
                parameters.mMaterial.w |= kBaseColorSRGB;
            }
            if ( emissive && rhi::GetFormatInfo( emissive->GetFormat() ).mIsSRGB ) {
                parameters.mMaterial.w |= kEmissiveSRGB;
            }

            parameters.mSurface = {
                surface->GetMetallicFactor(),
                surface->GetRoughnessFactor(),
                surface->GetTexture( material::MapType::eNormal ) ? surface->GetNormalScale() : 0.0f,
                surface->GetAlphaMask() == material::AlphaMode::eMask ? surface->GetAlphaMaskCutoff() : 0.0f
            };
        }

        auto uniform{ WriteCompatibilityUniform( *mDevice, commands, mUniform, &parameters, sizeof( parameters ) ) };
        rhi::BindingTableHandle table{};
        if ( const auto cached{ mFrameTables.find( surface ) }; cached != mFrameTables.end() ) {
            table = cached->second;
        }
        else {
            table = CreateBindings( surface, environment, frameUniform, *uniform.GetPtr(), shadowMap );
            if ( mUniform ) {
                mFrameTables.try_emplace( surface, table );
            }
        }

        commands.BindPipelineResources( rhi::BindResourcesDescription{}
            .SetBindPoint( rhi::PipelineType::eGraphics )
            .SetPipelineLayout( mLayout.GetPtr() )
            .AddResourceSet( 0, table.GetPtr() ) );
    }
}// namespace mikoto::renderer
