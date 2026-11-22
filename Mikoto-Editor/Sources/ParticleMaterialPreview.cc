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

#include <glm/gtc/matrix_transform.hpp>

#include <Assets/AssetParticleEffect.hh>

#include <Material/SkyboxMaterial.hh>
#include <Material/SurfaceMaterial.hh>

#include <Scene/Component.hh>

#include <Application/ParticleMaterialPreview.hh>

namespace mikoto::editor {

    ParticleMaterialPreview::ParticleMaterialPreview( renderer::rhi::IGpuDevice* device, const asset::ModelHandle& ground ) {
        mScene = eastl::make_unique<scene::Scene>( "Particle Material Preview" );

        auto* simulator{ mScene->CreateEntity( "Preview emitter" ) };
        mSimulator = simulator->Get();
        simulator->AddComponent<scene::ParticleSimulatorComponent>();

        if ( ground ) {
            auto* entity{ mScene->CreateEntity( scene::EntityCreateInfo{ .mName = "Preview ground", .mModel = ground } ) };
            mGround = entity->Get();
            entity->GetComponent<scene::TransformComponent>().SetTransform(
                glm::translate( core::float4x4{ 1 }, core::float3{ 0, -0.08f, 0 } ) *
                glm::scale( core::float4x4{ 1 }, core::float3{ 10, 0.1f, 10 } ) );

            auto surface{ material::SurfaceMaterialHandle::New( "Particle preview ground" ) };
            surface->SetBaseColorFactor( { 0.16f, 0.18f, 0.22f, 1 } );
            surface->SetRoughnessFactor( 0.85f );
            entity->GetComponent<scene::MaterialComponent>().SetMaterial( surface );
        }

        auto* light{ mScene->CreateEntity( "Preview key light" ) };
        mLight = light->Get();
        light->AddComponent<scene::LightComponent>();

        mPostProcess = core::Ref<material::PostProcessMaterial>::New( "Particle preview bloom" );
        mScene->CreateEntity( "Preview post process" )->AddComponent<scene::PostProcessMaterialComponent>( mPostProcess );
        mScene->Update( 0.0f );

        mCamera.SetViewportSize( 1280, 720 );

        mRenderer = renderer::ISceneRenderer::Create()
            .SetName( "Particle Material Preview" )
            .SetDevice( device )
            .SetRenderResolution( renderer::rhi::RenderResolution::e720P )
            .Build();

        mRenderer->SetEditorOverlaysEnabled( false );
        mRenderer->SetEnableSsao( false );
        mRenderer->SetEnableInfiniteGrid( true );
        mRenderer->SetParticleSimulationMode( renderer::ParticleSimulationMode::eCpu );
        mRenderer->SetRenderBackground( renderer::SceneBackgroundType::ePrefilterMap );

        mRenderer->Initialize();
        mRenderer->SetMainCamera( &mCamera );
    }

    ParticleMaterialPreview::~ParticleMaterialPreview() {
        if ( mRenderer ) {
            mRenderer->Shutdown();
        }
    }

    auto ParticleMaterialPreview::Render(
        const material::ParticleMaterialHandle& material,
        const renderer::ThumbnailPreviewSettings& settings,
        const asset::AssetHandle<asset::Texture>& environment,
        core::f32 timeStep,
        core::u32 width,
        core::u32 height ) -> renderer::rhi::TextureHandle {
        auto& simulator{ mScene->GetRegistry().get<scene::ParticleSimulatorComponent>( mSimulator ) };

        if ( mMaterial != material ) {
            renderer::ParticleEmitterSettings recipe{};

            recipe.mEmissionRate = 45;
            recipe.mLifetime = 1.6f;

            recipe.mSpeed = 1.3f;
            recipe.mAcceleration = { 0, 0.1f, 0 };
            recipe.mDrag = 0.25f;

            recipe.mStartColor = { 1, 1, 1, 1 };
            recipe.mEndColor = { 1, 1, 1, 1 };
            recipe.mStartSize = 0.22f;
            recipe.mEndSize = 0.35f;

            simulator.SetEmitter( 0, renderer::ParticleEmitterHandle::New(
                renderer::ParticleEmitterCreateDescription{ .mSettings = recipe, .mMaterial = material } ) );
            mMaterial = material;

            // Populate the first preview immediately. Later edits keep its playback clock.
            for ( core::u32 step{}; step < 60; ++step ) {
                simulator.Update( 1.0f / 60.0f, core::float4x4{ 1 }, 1, renderer::ParticleSimulationMode::eCpu );
            }
        }

        if ( mEnvironment != settings.mEnvironment ) {
            mEnvironment = settings.mEnvironment;

            if ( mEnvironment ) {
                mSkybox = core::Ref<material::SkyboxMaterial>::New(
                    material::SkyboxMaterialDescription{}.SetEquirectangular( environment ) );
                mRenderer->SetSkyboxMaterial( mSkybox );
            }
        }

        if ( mSkybox ) {
            mSkybox->SetAmbientScale( settings.mEnvironmentIntensity );
        }

        mPostProcess->SetBloomSettings( settings.mBloom );
        mRenderer->SetExposure( settings.mExposure );
        mRenderer->SetSkyboxBlur( settings.mBackgroundBlur );
        mRenderer->SetEnableInfiniteGrid( settings.mInfiniteGrid );

        if ( mGround != entt::null ) {
            mScene->GetRegistry().get<scene::TagComponent>( mGround ).SetActive( settings.mGround );
        }

        auto& light{ mScene->GetRegistry().get<scene::LightComponent>( mLight ) };
        const auto yaw{ glm::radians( settings.mLightYaw ) };
        const auto elevation{ glm::radians( settings.mLightElevation ) };

        light.SetActiveType( renderer::LightType::eDirectional );

        auto& directional{ light.Get<renderer::DirectionalLight>() };
        directional.SetDirection(
            glm::sin( yaw ) * glm::cos( elevation ),
            -glm::sin( elevation ),
            glm::cos( yaw ) * glm::cos( elevation ) );
        directional.SetIntensity( settings.mLightIntensity );
        directional.SetIsShadowCaster( settings.mShadows );

        mCamera.SetViewportSize( width, height );

        const auto cameraYaw{ glm::radians( settings.mYaw ) };
        const auto pitch{ glm::radians( settings.mPitch ) };
        const core::float3 target{ 0, 1.0f, 0 };
        const core::float3 cameraOffset{
            glm::sin( cameraYaw ) * glm::cos( pitch ),
            glm::sin( pitch ),
            glm::cos( cameraYaw ) * glm::cos( pitch )
        };

        mCamera.LookAt( target + settings.mDistance * cameraOffset, target );

        simulator.Update( glm::clamp( timeStep, 0.0f, 0.1f ), core::float4x4{ 1 }, 1, renderer::ParticleSimulationMode::eCpu );
        mRenderer->Render( mScene.get() );

        return mRenderer->GetFinalImage();
    }

}// namespace mikoto::editor
