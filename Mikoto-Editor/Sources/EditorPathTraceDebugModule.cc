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

#include <exception>

#include <imgui.h>

#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Assets/AssetRegistryService.hh>

#include <Material/SurfaceMaterial.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>

#include <Layers/EditorPathTraceDebugModule.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace renderer;
    using namespace renderer::rhi;
    using namespace scene;

    EditorPathTraceDebugModule::EditorPathTraceDebugModule()
        : mName{ "Path Tracing Showcase" } {
    }

    auto EditorPathTraceDebugModule::OnCreate() -> void {
        core::cstr stage{ "loading showcase assets" };

        try {
            auto* device{ renderer::RenderSystem::Get()->GetGpuDevice() };

            if ( !device->SupportsRayTracing() ) {
                MKT_THROW_RUNTIME_ERROR( "This showcase requires Vulkan ray tracing or D3D12 DXR." );
            }

            const auto cube{ asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( filesystem::Path{ "Resources/Prefabs/cube/gltf/scene.gltf" } ) };
            const auto sphere{ asset::AssetRegistryService::Get()->LoadAsset<asset::Model>( filesystem::Path{ "Resources/Prefabs/sphere/gltf/scene.gltf" } ) };
            stage = "creating showcase scene";
            mScene = eastl::make_unique<scene::Scene>( "Path Tracing Cornell Room" );
            const auto shape{ [&]( core::cstr name, bool round, core::float3 center, core::float3 size, core::float3 color, float metal, float rough, float emission ) {
                const auto model{ round ? sphere : cube };

                if ( !model || model->GetMeshNodeCount() != 1 ) {
                    MKT_THROW_RUNTIME_ERROR( "Path tracing showcase requires the cube and sphere prefabs." );
                }

                const auto& points{ model->GetMeshNode( 0 ).GetCollisionVertices() };

                if ( points.empty() ) {
                    MKT_THROW_RUNTIME_ERROR( "Showcase prefab has no bounds." );
                }

                core::float3 minimum{ points.front() };
                core::float3 maximum{ points.front() };

                for ( const auto& point: points ) {
                    minimum = glm::min( minimum, point );
                    maximum = glm::max( maximum, point );
                }

                auto* entity{ mScene->CreateEntity( scene::EntityCreateInfo{ .mName = name, .mModel = model } ) };
                auto& transform{ entity->GetComponent<scene::TransformComponent>() };
                const core::float3 scale{ size / glm::max( maximum - minimum, core::float3{ 0.0001f } ) };
                transform.SetTransform( core::float4x4{ 1.0f } );
                transform.SetUniformSale( false );
                transform.SetScale( scale );
                transform.SetTranslation( center - ( minimum + maximum ) * 0.5f * scale );

                // Each test shape owns its material so its optical response is isolated.
                auto material{ asset::AssetRegistryService::Get()->CreateMaterial( material::SurfaceMaterialDescription{} ) };
                auto physical{ material.As<material::SurfaceMaterial>() };
                physical->SetBaseColorFactor( core::float4{ color, 1.0f } );
                physical->SetMetallicFactor( metal );
                physical->SetRoughnessFactor( rough );
                physical->SetEmissiveFactor( color * emission );
                physical->SetEmissiveStrength( 1.0f );
                entity->GetComponent<scene::MaterialComponent>().SetMaterial( material );
            } };

            // Enclose the subjects so indirect light and colour bleeding are visible.
            shape( "Floor", false, { 0, -0.1f, 0 }, { 6, 0.2f, 6 }, core::float3{ 0.75f }, 0, 0.8f, 0 );
            shape( "Ceiling", false, { 0, 5.1f, 0 }, { 6, 0.2f, 6 }, core::float3{ 0.75f }, 0, 0.8f, 0 );
            shape( "Back", false, { 0, 2.5f, -3 }, { 6, 5, 0.2f }, core::float3{ 0.75f }, 0, 0.8f, 0 );
            shape( "Red wall", false, { -3, 2.5f, 0 }, { 0.2f, 5, 6 }, { 0.7f, 0.04f, 0.03f }, 0, 0.8f, 0 );
            shape( "Green wall", false, { 3, 2.5f, 0 }, { 0.2f, 5, 6 }, { 0.04f, 0.65f, 0.06f }, 0, 0.8f, 0 );
            shape( "Area emitter", false, { 0, 4.85f, -0.3f }, { 2, 0.1f, 2 }, core::float3{ 1 }, 0, 0.8f, 12 );

            shape( "Glossy sphere", true, { -1.2f, 1.1f, -0.8f }, core::float3{ 2.2f }, { 0.9f, 0.75f, 0.5f }, 1, 0.15f, 0 );
            shape( "Diffuse cube", false, { 1.15f, 0.85f, 0.3f }, core::float3{ 1.7f }, core::float3{ 0.8f }, 0, 0.65f, 0 );

            mCamera.SetViewportSize( 1280.0f, 720.0f );
            mCamera.LookAt( { 0, 2.5f, 9 }, { 0, 2.5f, 0 } );

            stage = "creating scene renderer";
            mRenderer = ISceneRenderer::Create()
                .SetDevice( device )
                .SetRenderResolution( renderer::rhi::RenderResolution::e720P )
                .Build();
            auto settings{ mRenderer->GetSettings() };
            settings.mRayTracing.mPathTracing = true;
            settings.mRayTracing.mPathMaxBounces = 8;
            mRenderer->ApplySettings( settings );

            stage = "compiling path tracing frame graph";
            mRenderer->Initialize();
            mRenderer->SetMainCamera( &mCamera );
            mRenderer->SetGamma( 2.2f );
            mRenderer->SetEnvironmentLightingEnabled( false );

            stage = "updating showcase scene";
            mScene->Update( 0.0f );
        } catch ( const std::exception& error ) {
            mError = error.what();
            MKT_FILE_LOGGER_ERROR( "Path tracing showcase ({}): {}", stage, error.what() );
        }
    }

    auto EditorPathTraceDebugModule::OnUpdate( float ) -> void {
        if ( !imgui::BeginWindow( imgui::widget::LocalizedLabel( "editor_path_trace_debug_module_path_tracing_showcase", "Path Tracing Showcase" ).c_str() ) ) {
            ImGui::End();
            return;
        }

        if ( !mError.empty() ) {
            ImGui::TextWrapped( "%s", mError.c_str() );
        }
        else if ( mRenderer ) {
            if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "editor_path_trace_debug_module_camera_orbit", "Camera orbit" ).c_str(), &mAngle, -0.3f, 0.3f ) ) {
                mCamera.LookAt( { glm::sin( mAngle ) * 9.0f, 2.5f, glm::cos( mAngle ) * 9.0f }, { 0, 2.5f, 0 } );
            }

            if ( ImGui::SliderFloat( imgui::widget::LocalizedLabel( "editor_path_trace_debug_module_exposure", "Exposure" ).c_str(), &mExposure, 0.1f, 5.0f ) ) {
                mRenderer->SetExposure( mExposure );
            }

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "editor_path_trace_debug_module_restart", "Restart" ).c_str() ) ) {
                mRenderer->ResetPathAccumulation();
            }

            ImGui::SameLine();
            ImGui::Text( MKT_LOC( "editor_path_trace_debug_module_samples" ).c_str(), mRenderer->GetPathSampleCount() );

            try {
                mRenderer->Render( mScene.get() );
                const auto& error{ mRenderer->GetRayTracingError() };

                if ( !error.empty() ) {
                    ImGui::TextWrapped( "%s", error.c_str() );
                }
                else {
                    auto output{ mRenderer->GetFinalImage( renderer::FinalImageType::eTonemap_Output ) };
                    const ImVec2 available{ ImGui::GetContentRegionAvail() };
                    const float width{ glm::min( available.x, available.y * 16.0f / 9.0f ) };

                    if ( output && width > 0.0f ) {
                        ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( output ), { width, width * 9.0f / 16.0f } );
                    }
                }
            } catch ( const std::exception& error ) {
                mError = error.what();
                MKT_FILE_LOGGER_ERROR( "Path tracing showcase: {}", error.what() );
            }
        }

        ImGui::End();
    }

    auto EditorPathTraceDebugModule::OnDestroy() -> void {
        renderer::RenderSystem::Get()->GetGpuDevice()->WaitIdle();

        if ( mRenderer ) {
            mRenderer->Shutdown();
            mRenderer.reset();
        }

        mScene.reset();
    }


    auto EditorPathTraceDebugModule::OnRender( float ) -> void {
    }

    auto EditorPathTraceDebugModule::GetName() const -> eastl::string_view {
        return mName;
    }

    auto EditorPathTraceDebugModule::OnEvent( platform::Event& ) -> void {
    }
}
