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

#include <cfloat>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Application/ParticleProperties.hh>
#include <Application/PreviewEnvironment.hh>

#include <Assets/AssetRegistryService.hh>

#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>
#include <Core/String.hh>

#include <Filesystem/FileSystem.hh>

#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/Utility.hh>

#include <Layers/EditorLayer.hh>

#include <Material/SkyboxMaterial.hh>

#include <Panels/ParticleSimulationPanel.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Particle/ParticleEmitter.hh>
#include <Renderer/Passes/ThumbnailPasses.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace scene;
    using namespace renderer;
    using namespace renderer::rhi;
    using namespace filesystem;

    ParticleSimulationPanel::ParticleSimulationPanel( const ParticleSimulationPanelCreateInfo &info )
        : Panel{ "Particle Simulation", "Panel", info.mGroup, info.mIsToggleable },
          mEditorState{ info.mState } {
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_BLUR_ON, mPanelName );
        mPanelIsVisible = false;
        mEmitters.push_back( ParticleEmitterHandle::New() );
        SelectEmitter( 0 );
    }

    ParticleSimulationPanel::~ParticleSimulationPanel() {
        if ( mRenderer ) {
            mRenderer->Shutdown();
        }
    }

    auto ParticleSimulationPanel::InitializePreview() -> void {
        // The preview runs its own clocks independently of scene playback.
        mScene = eastl::make_unique<scene::Scene>( "Particle Preview" );
        auto *entity{ mScene->CreateEntity( "Preview simulator" ) };
        mSimulator = entity->Get();
        entity->AddComponent<scene::ParticleSimulatorComponent>();
        entity->GetComponent<scene::TransformComponent>().SetTranslation( { 0, renderer::kPreviewStageElevation, 0 } );
        RefreshPreview();

        const auto cube{ mEditorState->GetPrefab( PrefabModelType::eCube ) };

        if ( !cube || cube->GetMeshNodeCount() != 1 || cube->GetMeshNode( 0 ).GetCollisionVertices().empty() ) {
            MKT_THROW_RUNTIME_ERROR( "Particle preview requires the cube prefab." );
        }

        const auto &vertices{ cube->GetMeshNode( 0 ).GetCollisionVertices() };
        core::float3 minimum{ vertices.front() };
        core::float3 maximum{ minimum };

        for ( const auto &vertex: vertices ) {
            minimum = glm::min( minimum, vertex );
            maximum = glm::max( maximum, vertex );
        }

        const core::float3 size{ maximum - minimum };

        if ( glm::any( glm::lessThanEqual( size, core::float3{ 0.0001f } ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Particle preview ground has invalid bounds." );
        }

        auto *ground{ mScene->CreateEntity( scene::EntityCreateInfo{ .mName = "Preview ground", .mModel = cube } ) };
        mGround = ground->Get();
        // Raise the stage with its emitter; the infinite grid remains at Y=0.
        ground->GetComponent<scene::TransformComponent>().SetTransform(
            glm::translate( core::float4x4{ 1 }, core::float3{ 0, renderer::kPreviewStageElevation - 0.06f, 0 } ) *
            glm::scale( core::float4x4{ 1 }, core::float3{ 12, 0.1f, 12 } / size ) *
            glm::translate( core::float4x4{ 1 }, -( minimum + maximum ) * 0.5f ) );
        auto surface{ core::Ref<material::SurfaceMaterial>::New( "Particle preview ground" ) };
        surface->SetBaseColorFactor( { 0.25f, 0.27f, 0.3f, 1 } );
        surface->SetMetallicFactor( 0 );
        surface->SetRoughnessFactor( 0.85f );
        ground->GetComponent<scene::MaterialComponent>().SetMaterial( surface );
        mScene->Update( 0.0f );
        mCamera.SetViewportSize( 1280, 720 );
        auto renderer{ ISceneRenderer::Create()
                .SetName( "Particle Preview" )
                .SetDevice( renderer::RenderSystem::Get()->GetGpuDevice() )
                .SetRenderResolution( renderer::rhi::RenderResolution::e720P )
                .Build() };
        renderer->SetEditorOverlaysEnabled( false );
        renderer->SetEnableSsao( false );
        renderer->SetEnableInfiniteGrid( true );
        renderer->SetSkyboxBlur( mBlur );
        // The preview uses the convolved environment for its soft background;
        // ordinary skybox mipmaps only reduce image resolution.
        renderer->SetRenderBackground( renderer::SceneBackgroundType::ePrefilterMap );
        renderer->SetParticleSimulationMode( mMode );
        renderer->Initialize();
        renderer->SetMainCamera( &mCamera );
        mRenderer = eastl::move( renderer );
    }

    auto ParticleSimulationPanel::RefreshPreview() -> void {
        if ( mScene && mSimulator != entt::null ) {
            auto &simulator{ mScene->GetRegistry().get<scene::ParticleSimulatorComponent>( mSimulator ) };
            simulator.SetEffect( asset::ParticleEffectHandle::New( mEmitters ) );
        }

        mElapsed = 0.0f;
    }

    auto ParticleSimulationPanel::SelectEmitter( core::usize index ) -> void {
        mSelectedEmitter = index;
        mSettings = index < mEmitters.size() ? mEmitters[index]->GetSettings() : ParticleEmitterSettings{};
        mGraph.Initialize( mSettings );
        mGraphContext.Reset();
        mGraphRestore = mGraphFit = true;
        mGraphMessage.clear();
    }

    auto ParticleSimulationPanel::ReplaceEmitter( const ParticleEmitterSettings &settings, const material::ParticleMaterialHandle &material ) -> void {
        auto emitter{ ParticleEmitterHandle::New( ParticleEmitterCreateDescription{
            .mSettings = settings,
            .mMaterial = material } ) };

        if ( mScene && mSimulator != entt::null ) {
            mScene->GetRegistry().get<scene::ParticleSimulatorComponent>( mSimulator ).SetEmitter( mSelectedEmitter, emitter );
        }

        mEmitters[mSelectedEmitter] = eastl::move( emitter );
        mSettings = mEmitters[mSelectedEmitter]->GetSettings();
        mUnsaved = true;
    }

    auto ParticleSimulationPanel::SetRecipe( ParticleEmitterSettings settings ) -> void {
        settings.Sanitize();
        mGraph.mSettings = settings;
        const auto compiled{ ParticleGraphCompiler::Compile( mGraph ) };

        mGraphMessage = compiled.mMessage;
        mUnsaved = true;

        if ( compiled.mSuccess && mSelectedEmitter < mEmitters.size() ) {
            ReplaceEmitter( compiled.mSettings, mEmitters[mSelectedEmitter]->GetMaterial() );
        }
    }

    auto ParticleSimulationPanel::EditEffect( const asset::ParticleEffectHandle &effect ) -> void {
        mEmitters.clear();
        if ( effect ) {
            mEmitters = effect->GetEmitters();
        }

        SelectEmitter( 0 );
        RefreshPreview();
        mPath = {};
        mError.clear();
        mUnsaved = false;
        SetVisible( true );
    }

    auto ParticleSimulationPanel::OpenAsset( const filesystem::Path &path ) -> void {
        try {
            const auto effect{ asset::AssetRegistryService::Get()->LoadAsset<asset::ParticleEffect>( path ) };
            if ( !effect ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot load particle effect." );
            }

            EditEffect( effect );
            mPath = path;
        } catch ( const std::exception &error ) {
            mError = error.what();
            SetVisible( true );
        }
    }

    auto ParticleSimulationPanel::SaveAsset( bool saveAs ) -> void {
        if ( !mEmitters.empty() ) {
            const auto compiled{ ParticleGraphCompiler::Compile( mGraph ) };
            if ( !compiled.mSuccess ) {
                mError = compiled.mMessage;
                return;
            }
        }

        // Save updates the opened asset; new effects and Save As choose a destination.
        auto path{ mPath };
        if ( saveAs || path.IsEmpty() ) {
            const eastl::string defaultName{ mPath.IsEmpty() ? "Effect.mktpart" : mPath.GetFilename() };
            path = SaveFileDialog( defaultName, { { MKT_LOC( "particle_effect_asset" ).c_str(), "mktpart" } } );
        }

        if ( !path.IsEmpty() ) {
            const auto effect{ asset::ParticleEffectHandle::New( mEmitters ) };
            asset::AssetRegistryService::Get()->SaveParticleEffect( *effect.GetPtr(), path );
            mPath = path;
            mUnsaved = false;
        }
    }

    auto ParticleSimulationPanel::DrawEmitterPicker() -> void {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted( MKT_LOC( "particle_simulation_particle_emitter" ).c_str() );
        ImGui::SameLine();

        const auto label{ [&]( core::usize index ) {
            return string::Format( "{} {}", MKT_LOC( "particle_simulation_particle_emitter" ), index + 1 );
        } };

        ImGui::SetNextItemWidth( ImGui::GetFontSize() * 12.0f );
        const auto current{ mEmitters.empty() ? MKT_LOC( "particle_effect_no_emitters" ) : label( mSelectedEmitter ) };
        if ( ImGui::BeginCombo( "##ParticleEmitter", current.c_str() ) ) {
            for ( core::usize index{}; index < mEmitters.size(); ++index ) {
                ImGui::PushID( core::as<core::i32>( index ) );
                if ( ImGui::Selectable( label( index ).c_str(), index == mSelectedEmitter ) ) {
                    SelectEmitter( index );
                }
                ImGui::PopID();
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if ( ImGui::Button( MKT_LOC( "particle_effect_add_emitter" ).c_str() ) ) {
            mEmitters.push_back( ParticleEmitterHandle::New() );
            SelectEmitter( mEmitters.size() - 1 );
            RefreshPreview();
            mUnsaved = true;
        }

        ImGui::SameLine();
        ImGui::BeginDisabled( mEmitters.empty() );
        if ( ImGui::Button( MKT_LOC( "particle_effect_remove_emitter" ).c_str() ) ) {
            mEmitters.erase( mEmitters.begin() + mSelectedEmitter );
            SelectEmitter( mEmitters.empty() ? 0 : eastl::min( mSelectedEmitter, mEmitters.size() - 1 ) );
            RefreshPreview();
            mUnsaved = true;
        }
        ImGui::EndDisabled();
    }

    auto ParticleSimulationPanel::DrawEmitterMaterial() -> void {
        auto material{ mEmitters[mSelectedEmitter]->GetMaterial() };
        if ( DrawParticleEmitterMaterial( material, mEditorState->mSandboxMode ) ) {
            ReplaceEmitter( mSettings, material );
        }
    }

    auto ParticleSimulationPanel::DrawToolbar() -> void {
        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_load", "Load..." ).c_str() ) ) {
            const auto path{ OpenFileDialog( { { MKT_LOC( "particle_effect_asset" ).c_str(), "mktpart" } } ) };

            if ( !path.IsEmpty() ) {
                OpenAsset( path );
            }
        }

        ImGui::SameLine();

        ImGui::BeginDisabled( mEmitters.empty() );
        if ( ImGui::BeginCombo( "##Preset", MKT_LOC( "particle_simulation_presets" ).c_str() ) ) {
            for ( core::i32 preset{}; preset < 4; ++preset ) {
                const char *names[]{ MKT_LOC( "particle_simulation_default" ).c_str(), "Fountain", "Sparks", "Smoke" };

                if ( ImGui::Selectable( names[preset] ) ) {
                    renderer::ParticleEmitterSettings settings{};

                    if ( preset == 1 ) {
                        settings.mSpeed = 5;
                        settings.mAcceleration = { 0, -9.8f, 0 };
                        settings.mLifetime = 1;
                        settings.mConeAngle = 15;
                        settings.mStartSize = 0.06f;
                        settings.mEndSize = 0.03f;
                        settings.mStartColor = { 0.6f, 0.8f, 1, 0.9f };
                        settings.mEndColor = { 0.3f, 0.6f, 1, 0 };
                    }
                    else if ( preset == 2 ) {
                        settings.mSpeed = 4;
                        settings.mAcceleration = { 0, -5, 0 };
                        settings.mLifetime = 0.7f;
                        settings.mConeAngle = 50;
                        settings.mStartSize = 0.035f;
                        settings.mEndSize = 0.01f;
                        settings.mStartColor = { 1, 0.8f, 0.15f, 1 };
                        settings.mEndColor = { 1, 0.15f, 0.01f, 0 };
                    }
                    else if ( preset == 3 ) {
                        settings.mSpeed = 0.6f;
                        settings.mAcceleration = { 0.15f, 0.3f, 0 };
                        settings.mEmissionRate = 25;
                        settings.mLifetime = 4;
                        settings.mDrag = 0.4f;
                        settings.mStartSize = 0.2f;
                        settings.mEndSize = 1.5f;
                        settings.mStartColor = { 0.3f, 0.3f, 0.32f, 0.25f };
                        settings.mEndColor = { 0.5f, 0.5f, 0.52f, 0 };
                    }

                    mGraph.Initialize( settings );
                    mGraphRestore = mGraphFit = true;
                    SetRecipe( settings );
                }
            }

            ImGui::EndCombo();
        }

        ImGui::EndDisabled();
        ImGui::SameLine();

        if ( ImGui::Button( MKT_LOC( "editor_layer_save" ).c_str() ) ) {
            SaveAsset( false );
        }

        ImGui::SameLine();
        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_save_as", "Save as..." ).c_str() ) ) {
            SaveAsset( true );
        }

        mEditorState->ValidateSelection();
        auto *selected{ mEditorState->mSelectedEntity };
        const bool hasSimulator{ selected && selected->HasComponent<scene::ParticleSimulatorComponent>() };
        ImGui::SameLine();
        ImGui::BeginDisabled( !hasSimulator );

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_copy_selected", "Copy selected" ).c_str() ) ) {
            EditEffect( selected->GetComponent<scene::ParticleSimulatorComponent>().GetEffect() );
        }

        ImGui::SameLine();
        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_apply_to_selected", "Apply to selected" ).c_str() ) ) {
            auto &simulator{ selected->GetComponent<scene::ParticleSimulatorComponent>() };
            if ( simulator.GetInstances().size() == mEmitters.size() && simulator.GetEffect() ) {
                for ( core::usize index{}; index < mEmitters.size(); ++index ) {
                    if ( simulator.GetEffect()->GetEmitters()[index] != mEmitters[index] ) {
                        simulator.SetEmitter( index, mEmitters[index] );
                    }
                }
            }
            else {
                simulator.SetEffect( asset::ParticleEffectHandle::New( mEmitters ) );
            }
        }

        ImGui::EndDisabled();
        ImGui::TextDisabled( "%s%s", mPath.IsEmpty() ? MKT_LOC( "particle_effect_unsaved" ).c_str() : mPath.GetC_Str(), mUnsaved ? " *" : "" );
    }

    auto ParticleSimulationPanel::DrawPreview( core::f32 timeStep ) -> void {
        auto &simulator{ mScene->GetRegistry().get<scene::ParticleSimulatorComponent>( mSimulator ) };
        const auto instances{ simulator.GetInstances() };
        const auto isPlaying{ []( const auto &instance ) {
            return instance.GetPlayback() == ParticlePlayback::ePlaying;
        } };
        const bool playing{ eastl::any_of( instances.begin(), instances.end(), isPlaying ) };

        if ( ImGui::Button( playing ? MKT_LOC( "particle_simulation_pause" ).c_str() : MKT_LOC( "particle_simulation_play" ).c_str() ) ) {
            for ( auto &instance: instances ) {
                if ( playing ) {
                    if ( isPlaying( instance ) ) {
                        instance.Pause();
                    }
                }
                else {
                    instance.Play();
                }
            }
        }

        ImGui::SameLine();
        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_restart", "Restart" ).c_str() ) ) {
            for ( auto &instance: instances ) {
                instance.Restart();
            }
            mElapsed = 0;
        }

        ImGui::SameLine();
        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_step", "Step" ).c_str() ) ) {
            mStep = true;
        }

        ImGui::SameLine();
        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_stop", "Stop" ).c_str() ) ) {
            for ( auto &instance: instances ) {
                instance.Stop();
            }
            mElapsed = 0;
        }

        ImGui::SetNextItemWidth( 120 );
        ImGui::SliderFloat( imgui::widget::LocalizedLabel( "particle_simulation_playback_speed", "Playback speed" ).c_str(), &mSpeed, 0.1f, 2.0f, "%.2fx" );
        const auto available{ ImGui::GetContentRegionAvail() };
        const core::f32 width{ available.x };
        const core::f32 height{ available.y };

        if ( width < 1 || height < 1 ) {
            return;
        }
        // Reserve input before rendering so orbit/zoom affect this frame, while
        // only the visible preview advances its clock (no catch-up after hiding).
        const ImVec2 origin{ ImGui::GetCursorScreenPos() };
        ImGui::InvisibleButton( "Orbit preview", { width, height } );
        // Projection uses the displayed aspect, not the fixed backing texture.
        // Presenting that frame across this rectangle restores the correct shape
        // without rebuilding GPU simulation targets whenever the splitter moves.
        mCamera.SetViewportSize( width, height );

        if ( ImGui::IsItemActive() && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) ) {
            mYaw -= ImGui::GetIO().MouseDelta.x * 0.35f;
            mPitch = glm::clamp( mPitch + ImGui::GetIO().MouseDelta.y * 0.35f, -80.0f, 80.0f );
        }

        if ( ImGui::IsItemHovered() ) {
            mDistance = glm::clamp( mDistance - ImGui::GetIO().MouseWheel * 0.5f, 1.0f, 40.0f );
        }

        const core::f32 yaw{ glm::radians( mYaw ) };
        const core::f32 pitch{ glm::radians( mPitch ) };
        const core::float3 target{ 0, renderer::kPreviewStageElevation + mTargetHeight, 0 };
        mCamera.LookAt( target + mDistance * core::float3{ glm::sin( yaw ) * glm::cos( pitch ), glm::sin( pitch ), glm::cos( yaw ) * glm::cos( pitch ) }, target );
        mRenderer->SetParticleSimulationMode( mMode );
        if ( mStep ) {
            for ( auto &instance: instances ) {
                instance.Play();
            }
        }

        const auto step{ mStep ? 1.0f / 60.0f : glm::clamp( timeStep, 0.0f, 0.1f ) * mSpeed };
        const auto emitterWorld{ glm::translate( core::float4x4{ 1.0f }, core::float3{ 0, renderer::kPreviewStageElevation, 0 } ) };
        simulator.Update( step, emitterWorld, 1, mMode );
        if ( mStep || eastl::any_of( instances.begin(), instances.end(), isPlaying ) ) {
            mElapsed += step;
        }

        if ( mStep ) {
            for ( auto &instance: instances ) {
                if ( isPlaying( instance ) ) {
                    instance.Pause();
                }
            }
            mStep = false;
        }

        mRenderer->Render( mScene.get() );

        if ( const auto output{ mRenderer->GetFinalImage() } ) {
            ImGui::GetWindowDrawList()->AddImage( imgui::ImGuiService::Get()->GetTextureID( output ), origin, { origin.x + width, origin.y + height } );
        }
    }

    auto ParticleSimulationPanel::DrawSettings() -> void {
        if ( mEmitters.empty() ) {
            return;
        }

        DrawEmitterMaterial();

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "particle_simulation_info", "Info" ).c_str() ) ) {
            ImGui::TextUnformatted( mMode == renderer::ParticleSimulationMode::eGpu ? MKT_LOC( "particle_simulation_gpu_compute" ).c_str() : MKT_LOC( "particle_simulation_cpu_simulation" ).c_str() );
            ImGui::Text( MKT_LOC( "particle_simulation_preview_time_s" ).c_str(), mElapsed );
            const auto &instance{ mScene->GetRegistry().get<scene::ParticleSimulatorComponent>( mSimulator ).GetInstances()[mSelectedEmitter] };

            if ( mMode == renderer::ParticleSimulationMode::eCpu ) {
                ImGui::Text( MKT_LOC( "particle_simulation_live_dropped" ).c_str(), instance.GetParticles().size(), instance.GetDroppedParticles() );
            }
            else {
                ImGui::TextDisabled( MKT_LOC( "particle_simulation_gpu_counts_are_not_read_back" ).c_str() );
            }

            ImGui::SeparatorText( MKT_LOC( "particle_simulation_spawn" ).c_str() );
            ImGui::Text( MKT_LOC( "particle_simulation_particles_s_burst_capacity" ).c_str(), mSettings.mEmissionRate, mSettings.mBurstCount, mSettings.mMaxParticles );
            ImGui::SeparatorText( MKT_LOC( "particle_simulation_update" ).c_str() );
            ImGui::Text( MKT_LOC( "particle_simulation_lifetime_s_speed_drag" ).c_str(), mSettings.mLifetime, mSettings.mSpeed, mSettings.mDrag );
            ImGui::SeparatorText( MKT_LOC( "particle_simulation_render" ).c_str() );
            ImGui::TextWrapped( MKT_LOC( "particle_simulation_camera_facing_discs_colour_and_size_over_lifetime" ).c_str() );
        }

        auto settings{ mGraph.mSettings };
        ImGui::SeparatorText( MKT_LOC( "particle_simulation_emitter_stack" ).c_str() );
        ImGui::TextWrapped( MKT_LOC( "particle_simulation_changes_restart_the_preview_apply_explicitly_to_update_a_scene" ).c_str() );

        if ( DrawParticleEmitterSettings( settings ) ) {
            SetRecipe( settings );
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "particle_simulation_preview_scene", "Preview scene" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
            if ( BeginParticleSettingsRows( "##PreviewSettings" ) ) {
                core::i32 mode{ mMode == renderer::ParticleSimulationMode::eGpu ? 1 : 0 };

                if ( ImGui::Combo( ParticleControlRow( "particle_simulation_simulation" ).c_str(), &mode, MKT_LOC( "particle_simulation_cpu_gpu_compute_experimental" ).c_str() ) ) {
                    mMode = mode ? renderer::ParticleSimulationMode::eGpu : renderer::ParticleSimulationMode::eCpu;
                    mElapsed = 0;
                }

                if ( imgui::CheckBox( ParticleControlRow( "particle_simulation_infinite_grid" ), mShowGrid ) ) {
                    mRenderer->SetEnableInfiniteGrid( mShowGrid );
                }

                if ( imgui::CheckBox( ParticleControlRow( "particle_simulation_ground" ), mShowGround ) ) {
                    mScene->GetRegistry().get<scene::TagComponent>( mGround ).SetActive( mShowGround );
                }

                if ( imgui::widget::CircleSlider( ParticleControlRow( "particle_simulation_background_blur" ), &mBlur, 0, 6, "%.2f LOD" ) ) {
                    mRenderer->SetSkyboxBlur( mBlur );
                }

                if ( imgui::widget::CircleSlider( ParticleControlRow( "particle_simulation_exposure" ), &mExposure, 0.1f, 4, "%.3f" ) ) {
                    mRenderer->SetExposure( mExposure );
                }

                imgui::widget::CircleSlider( ParticleControlRow( "particle_simulation_orbit_target_height" ), &mTargetHeight, 0, 10, "%.3f" );
                ImGui::EndTable();
            }

            ImGui::TextWrapped( MKT_LOC( "particle_simulation_billboard_discs_the_ground_is_a_visual_reference_not_a" ).c_str() );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_reset_camera", "Reset camera" ).c_str() ) ) {
                mYaw = 35;
                mPitch = 18;
                mDistance = 8;
                mTargetHeight = 1.5f;
            }

            ImGui::TextDisabled( MKT_LOC( "particle_simulation_drag_to_orbit_scroll_to_zoom" ).c_str() );
        }
    }

    auto ParticleSimulationPanel::DrawGraph() -> void {
        namespace ed = ax::NodeEditor;
        bool changed{};

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_open_graph", "Open graph" ).c_str() ) ) {
            const auto path{ OpenFileDialog( { { MKT_LOC( "particle_simulation_particle_graph" ).c_str(), "mktparticlegraph" } } ) };

            if ( !path.IsEmpty() ) {
                std::ifstream file{ std::filesystem::u8path( path.GetC_Str() ) };

                if ( !file ) {
                    mGraphMessage = MKT_LOC( "particle_simulation_cannot_open_particle_graph" ).c_str();
                }
                else if ( mGraph.Deserialize( YAML::Load( file ), mGraphMessage ) ) {
                    mGraphContext.Reset();
                    mGraphRestore = mGraphFit = true;
                    changed = true;
                }
            }
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_save_graph", "Save graph" ).c_str() ) ) {
            const auto path{ SaveFileDialog( "Emitter.mktparticlegraph", { { MKT_LOC( "particle_simulation_particle_graph" ).c_str(), "mktparticlegraph" } } ) };

            if ( !path.IsEmpty() ) {
                std::ofstream file{ std::filesystem::u8path( path.GetC_Str() ) };
                file << YAML::Dump( mGraph.Serialize() );
                file.flush();
                mGraphMessage = file ? MKT_LOC( "particle_simulation_graph_saved" ).c_str() : MKT_LOC( "particle_simulation_could_not_save_graph" ).c_str();
            }
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_compile", "Compile" ).c_str() ) ) {
            changed = true;
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_fit_graph", "Fit graph" ).c_str() ) ) {
            mGraphFit = true;
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_auto_layout", "Auto layout" ).c_str() ) ) {
            for ( auto &n: mGraph.mNodes ) {
                n.mPosition = { float( n.mID - 1 ) * 380, 0 };
            }

            mGraphRestore = mGraphFit = true;
        }

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_add_stage", "Add stage" ).c_str() ) ) {
            ImGui::OpenPopup( "Particle stages" );
        }

        if ( ImGui::BeginPopup( "Particle stages" ) ) {
            for ( unsigned i{}; i < 5; ++i ) {
                auto type{ static_cast<renderer::ParticleNodeType>( i ) };
                bool exists{};

                for ( const auto &n: mGraph.mNodes ) {
                    exists |= n.mType == type;
                }

                if ( ImGui::MenuItem( renderer::ParticleGraph::Name( type ), nullptr, false, !exists ) ) {
                    changed |= mGraph.AddNode( type );
                    mGraphRestore = true;
                }
            }

            ImGui::EndPopup();
        }

        ImGui::TextWrapped( MKT_LOC( "particle_simulation_emission_spawn_update_appearance_billboard_compiles_to_the_existing_emitter" ).c_str() );

        if ( !mGraphMessage.empty() ) {
            ImGui::TextWrapped( "%s", mGraphMessage.c_str() );
        }

        imgui::NodeEditorScope scope{ mGraphContext };
        const auto previous{ ed::GetStyle() };
        imgui::ApplyBlueprintStyle();
        ed::Begin( "ParticleGraph" );
        auto &settings{ mGraph.mSettings };

        for ( auto &node: mGraph.mNodes ) {
            if ( mGraphRestore ) {
                ed::SetNodePosition( node.mID * 16, { node.mPosition.x, node.mPosition.y } );
            }

            ImGui::PushID( static_cast<int>( node.mID ) );
            ed::BeginNode( node.mID * 16 );
            const auto header{ ImGui::GetCursorScreenPos() };
            ImGui::TextUnformatted( renderer::ParticleGraph::Name( node.mType ) );
            ImGui::Dummy( { 300, 4 } );
            auto pin = [&]( bool input ) {
                const auto kind{ input ? ed::PinKind::Input : ed::PinKind::Output };
                ed::BeginPin( node.mID * 16 + ( input ? 1 : 2 ), kind );
                bool connected{};

                for ( const auto &link: mGraph.mLinks ) {
                    connected |= ( input ? link.mDestination : link.mSource ) == node.mID;
                }

                imgui::DrawBlueprintPinIcon( imgui::BlueprintPinShape::eFlow, connected, { .8f, .9f, 1, 1 } );
                ImGui::SameLine();
                ImGui::TextUnformatted( input ? "In" : MKT_LOC( "particle_simulation_out" ).c_str() );
                ed::EndPin();
            };

            if ( node.mType != renderer::ParticleNodeType::eEmission ) {
                pin( true );
            }

            if ( node.mType != renderer::ParticleNodeType::eOutput ) {
                if ( node.mType != renderer::ParticleNodeType::eEmission ) {
                    ImGui::SameLine();
                }

                pin( false );
            }

            ImGui::PushItemWidth( 150 );

            switch ( node.mType ) {
                case renderer::ParticleNodeType::eEmission:
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_rate", "Rate" ).c_str(), &settings.mEmissionRate, 1, 0, 100000 );
                    changed |= ImGui::InputScalar( imgui::widget::LocalizedLabel( "particle_simulation_capacity", "Capacity" ).c_str(), ImGuiDataType_U32, &settings.mMaxParticles );
                    changed |= ImGui::InputScalar( imgui::widget::LocalizedLabel( "particle_simulation_burst", "Burst" ).c_str(), ImGuiDataType_U32, &settings.mBurstCount );
                    changed |= ImGui::InputScalar( imgui::widget::LocalizedLabel( "particle_simulation_seed", "Seed" ).c_str(), ImGuiDataType_U32, &settings.mSeed );
                    changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "particle_simulation_loop", "Loop" ).c_str(), settings.mLooping );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_duration", "Duration" ).c_str(), &settings.mDuration, .05f, .02f, 3600 );
                    break;
                case renderer::ParticleNodeType::eSpawn:
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_lifetime", "Lifetime" ).c_str(), &settings.mLifetime, .05f, .02f, 300 );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_lifetime_variation", "Lifetime variation" ).c_str(), &settings.mLifetimeVariation, .01f, 0, .95f );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_radius", "Radius" ).c_str(), &settings.mRadius, .01f, 0, 1000 );
                    changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "particle_simulation_direction", "Direction" ).c_str(), &settings.mDirection.x, .02f, -1, 1 );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_cone_angle", "Cone angle" ).c_str(), &settings.mConeAngle, 1, 0, 180 );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_speed", "Speed" ).c_str(), &settings.mSpeed, .05f, 0, 1000 );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_speed_variation", "Speed variation" ).c_str(), &settings.mSpeedVariation, .01f, 0, 1 );
                    break;
                case renderer::ParticleNodeType::eUpdate:
                    changed |= ImGui::DragFloat3( imgui::widget::LocalizedLabel( "particle_simulation_acceleration", "Acceleration" ).c_str(), &settings.mAcceleration.x, .05f, -10000, 10000 );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_drag", "Drag" ).c_str(), &settings.mDrag, .01f, 0, 100 );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_angular_speed", "Angular speed" ).c_str(), &settings.mAngularSpeed, .05f, -100, 100 );
                    break;
                case renderer::ParticleNodeType::eAppearance:
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_start_size", "Start size" ).c_str(), &settings.mStartSize, .01f, 0, 1000 );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_end_size", "End size" ).c_str(), &settings.mEndSize, .01f, 0, 1000 );
                    // Numeric RGBA controls avoid popup interaction inside the graph canvas.
                    changed |= ImGui::DragFloat4( imgui::widget::LocalizedLabel( "particle_simulation_start_rgba", "Start RGBA" ).c_str(), &settings.mStartColor.x, .01f, 0, 100 );
                    changed |= ImGui::DragFloat4( imgui::widget::LocalizedLabel( "particle_simulation_end_rgba", "End RGBA" ).c_str(), &settings.mEndColor.x, .01f, 0, 100 );
                    break;
                case renderer::ParticleNodeType::eOutput:
                    ImGui::TextUnformatted( MKT_LOC( "particle_simulation_camera_facing_discs" ).c_str() );
                    changed |= ImGui::DragFloat( imgui::widget::LocalizedLabel( "particle_simulation_edge_softness", "Edge softness" ).c_str(), &settings.mSoftness, .01f, 0, 1 );
                    break;
            }

            ImGui::PopItemWidth();
            ed::EndNode();
            imgui::DrawBlueprintHeader( node.mID * 16, header, { .25f, .42f, .52f, 1 } );
            ImGui::PopID();
            const auto p{ ed::GetNodePosition( node.mID * 16 ) };
            node.mPosition = { p.x, p.y };
        }

        mGraphRestore = false;

        for ( const auto &link: mGraph.mLinks ) {
            ed::Link( link.mSource * 16 + 3, link.mSource * 16 + 2, link.mDestination * 16 + 1, { .7f, .85f, 1, 1 }, 2 );
        }

        if ( ed::BeginCreate() ) {
            ed::PinId a{};
            ed::PinId b{};

            if ( ed::QueryNewLink( &a, &b ) && a && b ) {
                if ( a.Get() % 16 == 1 ) {
                    eastl::swap( a, b );
                }

                if ( a.Get() % 16 == 2 && b.Get() % 16 == 1 && mGraph.CanConnect( a.Get() / 16, b.Get() / 16 ) ) {
                    if ( ed::AcceptNewItem() ) {
                        changed |= mGraph.Connect( a.Get() / 16, b.Get() / 16 );
                    }
                }
                else {
                    ed::RejectNewItem( { 1, .2f, .2f, 1 } );
                }
            }
        }

        ed::EndCreate();

        if ( ed::BeginDelete() ) {
            ed::LinkId link{};

            while ( ed::QueryDeletedLink( &link ) ) {
                if ( ed::AcceptDeletedItem() ) {
                    mGraph.Disconnect( link.Get() / 16 );
                    changed = true;
                }
            }

            ed::NodeId node{};

            while ( ed::QueryDeletedNode( &node ) ) {
                if ( ed::AcceptDeletedItem() ) {
                    mGraph.RemoveNode( node.Get() / 16 );
                    changed = true;
                }
            }
        }

        ed::EndDelete();
        ed::End();
        ed::GetStyle() = previous;

        if ( mGraphFit ) {
            ed::NavigateToContent( 0 );
            mGraphFit = false;
        }

        if ( changed ) {
            const auto result{ ParticleGraphCompiler::Compile( mGraph ) };
            mGraphMessage = result.mMessage;
            mUnsaved = true;

            if ( result.mSuccess ) {
                SetRecipe( result.mSettings );
            }
        }
    }

    auto ParticleSimulationPanel::OnRender( core::f32 timeStep ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 1120, 740 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        mPanelIsFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_ChildWindows );
        mPanelIsHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows );

        try {
            if ( !mRenderer && mError.empty() ) {
                const auto &prefab{ mEditorState->mPrefabs.at( PrefabModelType::eCube ) };

                if ( prefab.mRequest.GetState() == asset::AssetState::eFailed ) {
                    mError = prefab.mRequest.GetError();
                }
                else if ( mEditorState->GetPrefab( PrefabModelType::eCube ) ) {
                    InitializePreview();
                }
                else {
                    ImGui::TextDisabled( "Loading preview geometry..." );
                }
            }

            if ( mRenderer && mError.empty() ) {
                if ( !mEnvironment.IsRequested() ) {
                    mEnvironment.Request( PreviewEnvironment::DaylightPath() );
                }

                if ( mEnvironment.Poll( mEnvironmentTexture ) ) {
                    // The default material constructor selects six cube faces.
                    // Describe the panorama explicitly so validation accepts it
                    // and wakes projection, prefiltering and irradiance passes.
                    auto sky{ core::Ref<material::SkyboxMaterial>::New(
                        material::SkyboxMaterialDescription{}.SetEquirectangular( mEnvironmentTexture ) ) };
                    sky->SetAmbientScale( 1 );
                    mRenderer->SetSkyboxMaterial( sky );
                }

                DrawToolbar();
                DrawEmitterPicker();

                if ( !mEnvironment.GetError().empty() ) {
                    ImGui::TextWrapped( "%s", mEnvironment.GetError().c_str() );

                    if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_retry_street_environment", "Retry street environment" ).c_str() ) ) {
                        mEnvironment.Request( PreviewEnvironment::DaylightPath() );
                    }
                }

                if ( ImGui::BeginTable( "Particle workspace stacked", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp ) ) {
                    ImGui::TableSetupColumn( MKT_LOC( "particle_simulation_preview_and_details" ).c_str(), ImGuiTableColumnFlags_WidthStretch, 0.30f );
                    ImGui::TableSetupColumn( MKT_LOC( "particle_simulation_particle_graph" ).c_str(), ImGuiTableColumnFlags_WidthStretch, 0.70f );
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex( 0 );

                    if ( ImGui::BeginChild( "Preview and details", {}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar ) ) {
                        // Match the material workspace: resize the preview vertically
                        // without taking scrolling space away from the properties.
                        const core::f32 height{ ImGui::GetContentRegionAvail().y };
                        const core::f32 maximum{ eastl::max( 80.0f, height - 120.0f ) };
                        const core::f32 minimum{ eastl::min( 180.0f, maximum ) };
                        mPreviewHeight = glm::clamp( mPreviewHeight, minimum, maximum );
                        ImGui::SetNextWindowSizeConstraints( { 0, minimum }, { FLT_MAX, maximum } );

                        if ( ImGui::BeginChild( "Live preview", { 0, mPreviewHeight }, ImGuiChildFlags_ResizeY | ImGuiChildFlags_Borders ) ) {
                            mPreviewHeight = ImGui::GetWindowHeight();

                            try {
                                DrawPreview( timeStep );
                            } catch ( const std::exception &error ) {
                                mError = error.what();
                            }
                        }

                        ImGui::EndChild();

                        if ( ImGui::BeginChild( "Emitter properties" ) ) {
                            ImGui::PushItemWidth( -150 );

                            try {
                                DrawSettings();
                            } catch ( const std::exception &error ) {
                                mError = error.what();
                            }

                            ImGui::PopItemWidth();
                        }

                        ImGui::EndChild();
                    }

                    ImGui::EndChild();
                    ImGui::TableSetColumnIndex( 1 );

                    if ( ImGui::BeginChild( "Particle graph canvas", {}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar ) && !mEmitters.empty() ) {
                        DrawGraph();
                    }

                    ImGui::EndChild();
                    ImGui::EndTable();
                }
            }
        } catch ( const std::exception &error ) {
            mError = error.what();
        }

        if ( !mError.empty() ) {
            ImGui::TextWrapped( "%s", mError.c_str() );

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "particle_simulation_dismiss_retry", "Dismiss / retry" ).c_str() ) ) {
                mError.clear();
            }
        }

        ImGui::End();
    }
}// namespace mikoto::editor
