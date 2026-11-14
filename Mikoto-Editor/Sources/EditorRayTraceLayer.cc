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


#include <cstring>
#include <exception>

#include <imgui.h>

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <Core/LocalizationService.hh>

#include <Memory/BufferSpan.hh>

#include <Filesystem/FileService.hh>

#include <Renderer/Rhi/Descriptor.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>

#include <Layers/EditorRayTraceLayer.hh>


namespace mikoto::editor {
    using namespace core;
    using namespace renderer;
    using namespace renderer::rhi;

    // https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html
    // https://docs.vulkan.org/spec/latest/chapters/accelstructures.html
    // https://docs.vulkan.org/samples/latest/samples/extensions/ray_tracing_basic/README.html
    EditorRayTraceLayer::EditorRayTraceLayer( platform::IWindow* window )
        : mName{ "Hardware Ray Tracing Showcase" }, mWindow{ window } {
    }

    auto EditorRayTraceLayer::OnCreate() -> void {
        mDevice = renderer::RenderSystem::Get()->GetGpuDevice();

        if ( !mDevice->SupportsRayTracing() ) {
            mError = MKT_LOC( "editor_ray_trace_layer_hardware_ray_tracing_is_unavailable_select_a_dxr_capable_d3d12" ).c_str();
            return;
        }

        try {
            CreateScene();

            auto source{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/RayTracingShowcase.slang" ) };
            const auto shader{ [&]( renderer::rhi::ShaderType stage, const char* entry ) {
                return mDevice->CreateShader( renderer::rhi::ShaderModuleCreateDescription{}
                    .SetContents( source )
                    .SetModuleName( source->GetName() )
                    .SetModulePath( source->GetPath() )
                    .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
                    .SetStage( stage )
                    .SetEntryPoint( entry ) );
            } };

            // Geometry is an ordinary binding table; the TLAS uses a bindless slot.
            const auto visibility{ ShaderFlagsBits::RayGeneration | ShaderFlagsBits::Miss | ShaderFlagsBits::ClosestHit };
            mGeometryLayout = mDevice->CreateBindingLayout( BindingLayoutDescription{}
                .SetRegisterSpace( 0 )
                .SetShaderVisibility( visibility )
                .AddItem( { .mSlot = 1, .mType = ResourceType::eTexture_UAV } )
                .AddItem( { .mSlot = 2, .mType = ResourceType::eRawBuffer_SRV } )
                .AddItem( { .mSlot = 3, .mType = ResourceType::eRawBuffer_SRV } ) );
            auto sceneLayout{ mDevice->CreateDescriptorTableLayout( DescriptorTableLayoutDescription{}
                .SetRegisterSpace( 1 )
                .SetVisibility( visibility )
                .AddBindlessItem( BindlessLayoutItem::AccelerationStructure( MKT_ACCELERATION_STRUCTURE_BINDING, 4096 ) ) ) };

            mSceneTable = mDevice->CreateDescriptorTable( sceneLayout );

            mSceneIndex = mDevice->WriteDescriptorTable( mSceneTable,
                BindingTableItem::AccelerationStructure( MKT_ACCELERATION_STRUCTURE_BINDING, mTopLevel.GetPtr() ) );

            renderer::rhi::RayTracingPipelineDescription pipeline{};
            pipeline.mPipelineLayout = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{}
                .AddBindingLayout( mGeometryLayout )
                .AddBindingLayout( sceneLayout )
                .SetPushConstantSize( 80 )
                .SetPushConstantsVisibility( visibility ) );

            pipeline.mRayGeneration = shader( renderer::rhi::ShaderType::eRayGeneration, "RayGeneration" );
            pipeline.mMiss = shader( renderer::rhi::ShaderType::eMiss, "Miss" );
            pipeline.mClosestHit = shader( renderer::rhi::ShaderType::eClosestHit, "ClosestHit" );

            mPipeline = mDevice->CreatePipeline( pipeline );

            mShaderTable = mDevice->CreateShaderTable( ShaderTableDescription{}
                .SetPipeline( mPipeline.GetPtr() ) );

            mCommands = mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics );

            mCommands->SetEnableAutomaticBarriers( true );
        } catch ( const std::exception& error ) {
            mError = error.what();
        }
    }

    auto EditorRayTraceLayer::CreateScene() -> void {
        // Shared float4 position/material stream. The pipeline pulls geometry
        // from these same buffers; no duplicate geometry or vertex input layout.
        eastl::vector<glm::vec4> vertices{};
        eastl::vector<core::u32> indices{};
        const auto quad{ [&]( glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, float material ) {
            const core::u32 base{ static_cast<core::u32>( vertices.size() ) };
            vertices.push_back( { a, material } );
            vertices.push_back( { b, material } );
            vertices.push_back( { c, material } );
            vertices.push_back( { d, material } );

            for ( const core::u32 i: { 0u, 1u, 2u, 0u, 2u, 3u } ) {
                indices.push_back( base + i );
            }
        } };
        quad( { -8, 0, -8 }, { -8, 0, 8 }, { 8, 0, 8 }, { 8, 0, -8 }, 0 );
        const glm::vec3 center{ -1.6f, 1.0f, 0.0f };
        const glm::vec3 corners[]{ center + glm::vec3{ -1, -1, -1 }, center + glm::vec3{ 1, -1, -1 },
            center + glm::vec3{ 1, 1, -1 }, center + glm::vec3{ -1, 1, -1 },
            center + glm::vec3{ -1, -1, 1 }, center + glm::vec3{ 1, -1, 1 },
            center + glm::vec3{ 1, 1, 1 }, center + glm::vec3{ -1, 1, 1 } };
        const core::u32 faces[6][4]{ { 0, 1, 2, 3 }, { 4, 7, 6, 5 }, { 0, 4, 5, 1 }, { 3, 2, 6, 7 }, { 1, 5, 6, 2 }, { 0, 3, 7, 4 } };

        for ( const auto& face: faces ) {
            quad( corners[face[0]], corners[face[1]], corners[face[2]], corners[face[3]], 1 );
        }

        constexpr core::u32 rings{ 32 };
        constexpr core::u32 segments{ 64 };
        const core::u32 base{ static_cast<core::u32>( vertices.size() ) };

        for ( core::u32 y{}; y <= rings; ++y ) {
            const float latitude{ glm::pi<float>() * static_cast<float>( y ) / rings };

            for ( core::u32 x{}; x <= segments; ++x ) {
                const float longitude{ glm::two_pi<float>() * static_cast<float>( x ) / segments };
                vertices.push_back( { 1.5f + glm::sin( latitude ) * glm::cos( longitude ),
                    1.05f + glm::cos( latitude ), glm::sin( latitude ) * glm::sin( longitude ), 2.0f } );
            }
        }

        for ( core::u32 y{}; y < rings; ++y ) {
            for ( core::u32 x{}; x < segments; ++x ) {
                const core::u32 a{ base + y * ( segments + 1 ) + x };
                const core::u32 b{ a + segments + 1 };

                if ( y != 0 ) {
                    indices.push_back( a );
                    indices.push_back( a + 1 );
                    indices.push_back( b );
                }

                if ( y + 1 != rings ) {
                    indices.push_back( a + 1 );
                    indices.push_back( b + 1 );
                    indices.push_back( b );
                }
            }
        }

        const auto buffer{ [&]( const void* data, core::usize bytes ) {
            return mDevice->CreateBuffer( renderer::rhi::BufferCreateDescription{}
                .SetByteSize( bytes )
                .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::AccelStructureBuildInput )
                .SetResourceType( renderer::rhi::ResourceType::eRawBuffer_SRV )
                .SetInitialData( memory::BufferSpanHandle::New( data, bytes ) ) );
        } };
        mVertices = buffer( vertices.data(), vertices.size() * sizeof( vertices[0] ) );
        mIndices = buffer( indices.data(), indices.size() * sizeof( indices[0] ) );

        // Build the mesh acceleration structure before referencing it from an instance.
        renderer::rhi::AccelStructureCreateDescription geometry{};
        geometry.mVertices = mVertices;
        geometry.mIndices = mIndices;
        geometry.mVertexCount = static_cast<core::u32>( vertices.size() );
        geometry.mVertexStride = sizeof( glm::vec4 );
        geometry.mIndexCount = static_cast<core::u32>( indices.size() );
        mBottomLevel = mDevice->CreateAccelStructure( geometry );

        renderer::rhi::AccelStructureCreateDescription scene{};
        scene.mType = renderer::rhi::AccelStructureType::eTopLevel;
        renderer::rhi::RayTracingInstance instance{};
        instance.mBottomLevel = mBottomLevel;
        scene.mInstances.push_back( instance );
        mTopLevel = mDevice->CreateAccelStructure( scene );
    }

    auto EditorRayTraceLayer::Resize( core::u32 width, core::u32 height ) -> void {
        if ( mOutput && mOutput->GetWidth() == width && mOutput->GetHeight() == height ) {
            return;
        }

        auto replacement{ mDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( renderer::rhi::Format::eRGBA8_UNORM )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::ShaderResource | renderer::rhi::TextureUsageFlagsBits::UnorderedAccess ) ) };
        // Replacing a table leaves old descriptors alive through command-list ownership.
        auto bindings{ mDevice->CreateBindingTable( BindingTableDescription{}
            .AddItem( BindingTableItem::TextureUAV( 1, replacement.GetPtr() ) )
            .AddItem( BindingTableItem::RawBufferSRV( 2, mVertices.GetPtr() ) )
            .AddItem( BindingTableItem::RawBufferSRV( 3, mIndices.GetPtr() ) ),
            mGeometryLayout ) };
        mGeometryBindings = eastl::move( bindings );
        mOutput = eastl::move( replacement );
        mOutput->SetDebugName( "Hardware ray-tracing output" );
    }

    auto EditorRayTraceLayer::OnUpdate( float timeStep ) -> void {
        ImGui::SetNextWindowSize( { 1000, 720 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( imgui::widget::LocalizedLabel( "editor_ray_trace_layer_hardware_ray_tracing", "Hardware Ray Tracing" ).c_str() ) ) {
            ImGui::End();
            return;
        }

        if ( !mError.empty() ) {
            ImGui::TextWrapped( "%s", mError.c_str() );
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted( MKT_LOC( "editor_ray_trace_layer_hardware_triangle_traversal_blas_tlas_ray_generation_miss_closest_hit" ).c_str() );
        imgui::CheckBox( imgui::widget::LocalizedLabel( "editor_ray_trace_layer_orbit", "Orbit" ).c_str(), mOrbit );
        ImGui::SameLine();
        imgui::CheckBox( imgui::widget::LocalizedLabel( "editor_ray_trace_layer_ray_traced_shadows", "Ray-traced shadows" ).c_str(), mShadows );
        ImGui::SameLine();
        imgui::CheckBox( imgui::widget::LocalizedLabel( "editor_ray_trace_layer_ray_traced_reflections", "Ray-traced reflections" ).c_str(), mReflections );
        ImGui::SliderFloat( imgui::widget::LocalizedLabel( "editor_ray_trace_layer_distance", "Distance" ).c_str(), &mDistance, 3.0f, 18.0f );
        ImGui::SliderFloat( imgui::widget::LocalizedLabel( "editor_ray_trace_layer_field_of_view", "Field of view" ).c_str(), &mFieldOfView, 25.0f, 90.0f );
        ImGui::SliderFloat( imgui::widget::LocalizedLabel( "editor_ray_trace_layer_exposure", "Exposure" ).c_str(), &mExposure, 0.1f, 4.0f );
        const auto available{ ImGui::GetContentRegionAvail() };

        if ( available.x >= 1.0f && available.y >= 1.0f ) {
            try {
                const float scale{ glm::min( 1.0f, 2048.0f / glm::max( available.x, available.y ) ) };
                const auto width{ static_cast<core::u32>( glm::max( 1.0f, available.x * scale ) ) };
                const auto height{ static_cast<core::u32>( glm::max( 1.0f, available.y * scale ) ) };
                Resize( width, height );

                if ( mOrbit ) {
                    mAngle += glm::min( timeStep, 0.1f ) * 0.2f;
                }

                const glm::vec3 origin{ glm::sin( mAngle ) * mDistance, mDistance * 0.5f, glm::cos( mAngle ) * mDistance };
                const auto forward{ glm::normalize( glm::vec3{ 0, 0.8f, 0 } - origin ) };
                const auto right{ glm::normalize( glm::cross( forward, glm::vec3{ 0, 1, 0 } ) ) };
                const auto up{ glm::cross( right, forward ) };
                struct CameraParameters {
                    core::float4 mOrigin{};
                    core::float4 mForward{};
                    core::float4 mRight{};
                    core::float4 mUp{};
                    core::float3 mSettings{};
                    core::u32 mSceneIndex{};
                };
                const CameraParameters camera{
                    .mOrigin = { origin, glm::tan( glm::radians( mFieldOfView ) * 0.5f ) },
                    .mForward = { forward, mExposure },
                    .mRight = { right, static_cast<float>( width ) / height },
                    .mUp = { up, mShadows ? 1.0f : 0.0f },
                    .mSettings = { mReflections ? 1.0f : 0.0f, 0, 0 },
                    .mSceneIndex = core::as<core::u32>( mSceneIndex )
                };
                mCommands->Open( { .mScopeName = "Hardware Ray Tracing Showcase" } );

                if ( mBuildPending ) {
                    mCommands->Build( mTopLevel.GetPtr() );
                }

                mCommands->SetTransition( { mOutput.GetPtr(), ResourceStates::eUnorderedAccess } );
                mCommands->SetTransition( { mVertices.GetPtr(), ResourceStates::eShaderResource } );
                mCommands->SetTransition( { mIndices.GetPtr(), ResourceStates::eShaderResource } );

                auto layout{ mPipeline->GetPipelineLayout() };
                const auto visibility{ ShaderFlagsBits::RayGeneration | ShaderFlagsBits::Miss | ShaderFlagsBits::ClosestHit };
                mCommands->BindPipeline( mPipeline.GetPtr() );
                mCommands->BindShaderTable( mShaderTable.GetPtr() );
                mCommands->BindPipelineResources( BindResourcesDescription{}
                    .SetPipelineLayout( layout.GetPtr() )
                    .SetBindPoint( PipelineType::eRayTracing )
                    .AddResourceSet( 0, mGeometryBindings.GetPtr() )
                    .AddResourceSet( 1, mSceneTable.GetPtr() ) );
                mCommands->SetPushConstants( layout.GetPtr(), &camera, sizeof( camera ), visibility );
                mCommands->DispatchRays( { .mWidth = width, .mHeight = height } );
                mCommands->SetTransition( { mOutput.GetPtr(), ResourceStates::eShaderResource } );
                mCommands->Close();
                auto submission{ renderer::rhi::SubmitInfo{}.AddCommandList( mCommands ) };
                renderer::RenderSystem::Get()->BatchSubmission( eastl::move( submission ), renderer::rhi::QueueType::eGraphics );
                mBuildPending = false;
                ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( mOutput ), available );
            } catch ( const std::exception& error ) {
                mError = error.what();
            }
        }

        ImGui::End();
    }

    auto EditorRayTraceLayer::OnDestroy() -> void {
        if ( mDevice ) {
            mDevice->WaitIdle();
        }

        mCommands.Reset();
        mGeometryBindings.Reset();
        mGeometryLayout.Reset();

        if ( mSceneTable && mSceneIndex != kInvalidBindingItemIndex ) {
            mSceneTable->ReleaseIndex( mSceneIndex, ResourceType::eRayTracingAccelStruct, MKT_ACCELERATION_STRUCTURE_BINDING );
            mSceneIndex = kInvalidBindingItemIndex;
        }

        mSceneTable.Reset();
        mShaderTable.Reset();
        mPipeline.Reset();
        mTopLevel.Reset();
        mBottomLevel.Reset();
        mOutput.Reset();
        mVertices.Reset();
        mIndices.Reset();
    }

    auto EditorRayTraceLayer::OnRender( float ) -> void {
    }

    auto EditorRayTraceLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto EditorRayTraceLayer::OnEvent( platform::Event& ) -> void {
    }
}
