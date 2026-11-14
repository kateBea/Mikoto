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


#include <cstddef>

#include <EASTL/array.h>
#include <EASTL/sort.h>
#include <glm/matrix.hpp>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Assets/Importer.hh>
#include <Assets/AssetModel.hh>
#include <Assets/ShaderLibrary.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/SurfaceMaterial.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/CompatibilityRenderer/CompatibilityPassResources.hh>
#include <Renderer/CompatibilityRenderer/Passes/CompatibilityGeometryModule.hh>

namespace mikoto::renderer {
    static constexpr core::u32 kVertexStreamSlot{};

    auto CompatibilityGeometryModule::Initialize( rhi::IGpuDevice* device ) -> void {
        MKT_BEGIN_PROFILER();

        mMaterials.Initialize( device );

        auto* library{ asset::AssetRegistryService::Get()->GetShaderLibrary() };
        const auto vertex{ library->LoadShader( "../compatibility/slang/CompatibilityPBR_Vert.slang", rhi::ShaderType::eVertex ) };
        const auto pixel{ library->LoadShader( "../compatibility/slang/CompatibilityPBR_Frag.slang", rhi::ShaderType::ePixel ) };

        using Vertex = asset::VertexDescription_Std430Alignment;
        const eastl::array streams{
            rhi::VertexBindingDescription{}
                .SetBinding( kVertexStreamSlot )
                .SetStride( sizeof( Vertex ) )
                .SetInputRate( rhi::InputRate::ePerVertex )
        };
        const eastl::array attributes{
            rhi::VertexAttributeDescription{}
                .SetName( "POSITION" ).SetLocation( 0 ).SetBinding( kVertexStreamSlot )
                .SetFormat( rhi::Format::eRGB32_FLOAT ).SetOffset( offsetof( Vertex, mPosition ) ),
            rhi::VertexAttributeDescription{}
                .SetName( "NORMAL" ).SetLocation( 1 ).SetBinding( kVertexStreamSlot )
                .SetFormat( rhi::Format::eRGB32_FLOAT ).SetOffset( offsetof( Vertex, mNormals ) ),
            rhi::VertexAttributeDescription{}
                .SetName( "TEXCOORD" ).SetLocation( 2 ).SetBinding( kVertexStreamSlot )
                .SetFormat( rhi::Format::eRG32_FLOAT ).SetOffset( offsetof( Vertex, mUv0 ) ),
            rhi::VertexAttributeDescription{}
                .SetName( "COLOR" ).SetLocation( 3 ).SetBinding( kVertexStreamSlot )
                .SetFormat( rhi::Format::eRGBA32_FLOAT ).SetOffset( offsetof( Vertex, mColors ) ),
            rhi::VertexAttributeDescription{}
                .SetName( "TEXCOORD" ).SetLocation( 4 ).SetBinding( kVertexStreamSlot )
                .SetFormat( rhi::Format::eRG32_FLOAT ).SetOffset( offsetof( Vertex, mUv1 ) )
        };

        auto input{ device->CreateInputLayout( rhi::InputLayoutCreateDescription{}
            .SetBindings( streams )
            .SetAttributes( attributes )
            .SetShader( vertex ) ) };

        const auto description{ rhi::GraphicsPipelineDescription{}
            .AddColorFormat( kCompatibilitySceneFormat )
            .SetDepthFormat( kCompatibilityDepthFormat )
            .SetTopology( rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( rhi::CullMode::eNone )
            .SetUseReflection( false )
            .SetPipelineLayout( mMaterials.GetLayout() )
            .AddShader( vertex )
            .AddShader( pixel )
            .SetInputLayout( input )
            .SetBlendEnable( false )
            .SetDepthTest( true )
            .SetDepthWrite( true ) };
        mPipeline = device->CreatePipeline( description );

        // glTF BLEND uses source-over composition with opaque depth still active.
        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#alpha-coverage
        auto transparent{ description };
        mTransparentPipeline = device->CreatePipeline( transparent
            .SetBlendEnable( true )
            .SetDepthWrite( false ) );

        auto selection{ description };
        selection.mColorFormats.clear();
        mSelectionPipeline = device->CreatePipeline( selection.AddColorFormat( rhi::Format::eRG32_UINT )
            .AddShader( library->LoadShader( "../compatibility/slang/CompatibilitySelection_Frag.slang", rhi::ShaderType::ePixel ) ) );

        auto shadow{ description };
        shadow.mColorFormats.clear();
        mShadowPipeline = device->CreatePipeline( shadow.AddColorFormat( rhi::Format::eR32_FLOAT )
            .AddShader( library->LoadShader( "../compatibility/slang/CompatibilityShadow_Frag.slang", rhi::ShaderType::ePixel ) ) );

        if ( !input || !mPipeline || !mTransparentPipeline || !mSelectionPipeline || !mShadowPipeline ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize the compatibility geometry pass." );
        }
    }

    auto CompatibilityGeometryModule::Prepare( const scene::Scene& scene, const scene::Camera& camera, core::u32 hoverID ) -> void {
        MKT_BEGIN_PROFILER();

        mOpaque.clear();
        mTransparent.clear();
        mHasHighlights = false;

        const auto& view{ camera.GetViewMatrix() };
        const auto& registry{ scene.GetRegistry() };
        for ( const auto& [entity, tag, transform, mesh]:
            registry.view<scene::TagComponent, scene::TransformComponent, scene::MeshComponent>().each() ) {
            const auto* node{ tag.IsActive() ? mesh.GetMesh() : nullptr };
            if ( !node ) {
                continue;
            }

            auto vertices{ node->GetVertexBuffer() };
            auto indices{ node->GetIndexBuffer() };
            if ( !vertices || !indices ) {
                continue;
            }

            const auto* component{ registry.try_get<scene::MaterialComponent>( entity ) };
            const auto* surface{ component ? dynamic_cast<const material::SurfaceMaterial*>( component->GetMaterialReference().GetPtr() ) : nullptr };
            const auto world{ transform.GetWorldTransform() * node->GetSkinBinding().mBindPoseTransform };
            const auto id{ core::as<core::u32>( tag.GetGuid() ) };
            const auto* highlight{ registry.try_get<scene::HighlightComponent>( entity ) };
            const bool highlighted{ ( highlight && highlight->IsHighlighted() ) || ( hoverID && id == hoverID ) };
            mHasHighlights |= highlighted;

            Draw draw{
                .mVertices = eastl::move( vertices ),
                .mIndices = eastl::move( indices ),
                .mSurface = surface,
                .mWorld = world,
                .mNormalMatrix = glm::transpose( glm::inverse( world ) ),
                .mID = id,
                .mHighlighted = highlighted
            };

            if ( surface && surface->GetAlphaMask() == material::AlphaMode::eBlend ) {
                const auto center{ node->GetRenderData() ? node->GetRenderData()->GetSortCenter() : core::float3{} };
                draw.mDepth = ( view * world * core::float4{ center, 1.0f } ).z;
                mTransparent.push_back( eastl::move( draw ) );
            }
            else {
                mOpaque.push_back( eastl::move( draw ) );
            }
        }
    }

    auto CompatibilityGeometryModule::Record( rhi::ICommandList* commands,
        const CompatibilityEnvironment& environment, rhi::IBuffer& frameUniform, const rhi::TextureHandle& shadowMap ) -> void {
        mMaterials.BeginFrame();
        commands->BindPipeline( mPipeline.GetPtr() );
        for ( const auto& draw: mOpaque ) {
            RecordDraw( commands, draw, environment, frameUniform, shadowMap );
        }

        if ( mTransparent.empty() ) {
            return;
        }

        // Match the forward PBR pass. Alpha-one texels of BLEND meshes write depth
        // before the remaining coverage is composited in view-depth order.
        for ( const auto& draw: mTransparent ) {
            RecordDraw( commands, draw, environment, frameUniform, shadowMap );
        }

        eastl::sort( mTransparent.begin(), mTransparent.end(), []( const Draw& first, const Draw& second ) {
            return first.mDepth == second.mDepth ? first.mID < second.mID : first.mDepth < second.mDepth;
        } );

        commands->BindPipeline( mTransparentPipeline.GetPtr() );
        for ( const auto& draw: mTransparent ) {
            RecordDraw( commands, draw, environment, frameUniform, shadowMap, true );
        }
    }

    auto CompatibilityGeometryModule::RecordSelection( rhi::ICommandList* commands,
        const CompatibilityEnvironment& environment, rhi::IBuffer& frameUniform ) -> void {
        mMaterials.BeginFrame();
        commands->BindPipeline( mSelectionPipeline.GetPtr() );

        for ( const auto& draw: mOpaque ) {
            RecordDraw( commands, draw, environment, frameUniform, {} );
        }
        for ( const auto& draw: mTransparent ) {
            RecordDraw( commands, draw, environment, frameUniform, {} );
        }
    }

    auto CompatibilityGeometryModule::RecordShadows( rhi::ICommandList* commands,
        const CompatibilityEnvironment& environment, rhi::IBuffer& frameUniform ) -> void {
        mMaterials.BeginFrame();
        commands->BindPipeline( mShadowPipeline.GetPtr() );

        for ( const auto& draw: mOpaque ) {
            RecordDraw( commands, draw, environment, frameUniform, {} );
        }
        for ( const auto& draw: mTransparent ) {
            RecordDraw( commands, draw, environment, frameUniform, {} );
        }
    }

    auto CompatibilityGeometryModule::ClearDraws() -> void {
        mMaterials.BeginFrame();
        mOpaque.clear();
        mTransparent.clear();
    }

    auto CompatibilityGeometryModule::RecordDraw( rhi::ICommandList* commands, const Draw& draw,
        const CompatibilityEnvironment& environment, rhi::IBuffer& frameUniform, const rhi::TextureHandle& shadowMap,
        bool transparentPass ) -> void {
        mMaterials.Bind( commands, draw.mSurface, draw.mWorld, draw.mNormalMatrix, environment, frameUniform,
            draw.mID, draw.mHighlighted, shadowMap, transparentPass );

        auto indices{ draw.mIndices };
        auto vertices{ draw.mVertices };
        commands->BindIndexBuffer( indices.GetPtr() );
        commands->BindVertexBuffer( rhi::VertexBufferBinding{}
            .SetBufferBinding( kVertexStreamSlot )
            .SetBuffer( vertices.GetPtr() )
            .SetElementStride( sizeof( asset::VertexDescription_Std430Alignment ) ) );
        commands->DrawIndexed( rhi::DrawArguments{}
            .SetInstanceCount( 1 )
            .SetIndexCount( core::as<core::u32>( draw.mIndices->GetSizeBytes() / sizeof( core::u32 ) ) ) );
    }
}// namespace mikoto::renderer
