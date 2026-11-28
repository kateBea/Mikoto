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

#include <EASTL/algorithm.h>

#include <Application/ParticleProperties.hh>
#include <Application/PropertyWidgets.hh>

#include <Assets/AssetRegistryService.hh>

#include <Core/Exception.hh>
#include <Core/LocalizationService.hh>
#include <Core/String.hh>

#include <Filesystem/FileSystem.hh>

#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/ImGuiService.hh>
#include <ImGui/ImGuiWidget.hh>

namespace mikoto::editor {

    MKT_NODISCARD auto BeginParticleSettingsRows( core::cstr id ) -> bool {
        if ( !ImGui::BeginTable( id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings ) ) {
            return false;
        }

        ImGui::TableSetupColumn( "##Label", ImGuiTableColumnFlags_WidthStretch, 2.0f );
        ImGui::TableSetupColumn( "##Value", ImGuiTableColumnFlags_WidthStretch, 3.0f );

        return true;
    }

    MKT_NODISCARD auto ParticleControlRow( core::cstr key ) -> eastl::string {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextWrapped( "%s", MKT_LOC( key ).c_str() );

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth( -FLT_MIN );

        return string::Format( "##{}", key );
    }

    auto DrawParticleEmitterSettings( renderer::ParticleEmitterSettings &settings ) -> bool {
        bool changed{};

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "particle_simulation_emission", "Emission" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) &&
             BeginParticleSettingsRows( "##EmissionSettings" ) ) {
            core::i32 capacity{ core::as<core::i32>( settings.mMaxParticles ) };
            core::i32 burst{ core::as<core::i32>( settings.mBurstCount ) };

            changed |= imgui::widget::CircleInt( ParticleControlRow( "particle_simulation_capacity" ), &capacity, 1, renderer::kMaxEmitterParticles );
            changed |= imgui::widget::CircleInt( ParticleControlRow( "particle_simulation_initial_burst" ), &burst, 0, renderer::kMaxEmitterParticles );

            settings.mMaxParticles = core::as<core::u32>( eastl::max( 1, capacity ) );
            settings.mBurstCount = core::as<core::u32>( eastl::max( 0, burst ) );

            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_particles_second" ), &settings.mEmissionRate, { 0, 100000 }, "%.3f", true );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_lifetime" ), &settings.mLifetime, { 0.02f, 300 }, "%.3f", true );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_lifetime_variation" ), &settings.mLifetimeVariation, { 0, 0.95f }, "%.3f", true );

            changed |= imgui::CheckBox( ParticleControlRow( "particle_simulation_loop" ), settings.mLooping );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_duration" ), &settings.mDuration, { 0.02f, 3600 }, "%.3f", true );
            changed |= ImGui::InputScalar( ParticleControlRow( "particle_simulation_seed" ).c_str(), ImGuiDataType_U32, &settings.mSeed );

            ImGui::EndTable();
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "particle_simulation_spawn_and_motion", "Spawn and motion" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) &&
             BeginParticleSettingsRows( "##SpawnAndMotionSettings" ) ) {
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_radius" ), &settings.mRadius, { 0, 1000 }, "%.3f", true );
            changed |= DrawFloatVectorProperty( ParticleControlRow( "particle_simulation_direction" ).c_str(), &settings.mDirection.x, 3, { -1, 1 }, "%.3f", false );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_cone_angle" ), &settings.mConeAngle, { 0, 180 }, "%.3f", true );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_speed" ), &settings.mSpeed, { 0, 1000 }, "%.3f", true );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_speed_variation" ), &settings.mSpeedVariation, { 0, 1 }, "%.3f", true );

            changed |= DrawFloatVectorProperty( ParticleControlRow( "particle_simulation_acceleration" ).c_str(), &settings.mAcceleration.x, 3, { -10000, 10000 }, "%.3f", false );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_drag" ), &settings.mDrag, { 0, 100 }, "%.3f", true );

            ImGui::EndTable();
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "particle_simulation_appearance_over_lifetime", "Appearance over lifetime" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) &&
             BeginParticleSettingsRows( "##AppearanceSettings" ) ) {
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_start_diameter" ), &settings.mStartSize, { 0, 1000 }, "%.3f", true );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_end_diameter" ), &settings.mEndSize, { 0, 1000 }, "%.3f", true );

            changed |= ImGui::ColorEdit4( ParticleControlRow( "particle_simulation_start_colour" ).c_str(), &settings.mStartColor.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float );
            changed |= ImGui::ColorEdit4( ParticleControlRow( "particle_simulation_end_colour" ).c_str(), &settings.mEndColor.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float );

            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_angular_speed" ), &settings.mAngularSpeed, { -100, 100 }, "%.3f", true );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_simulation_edge_softness" ), &settings.mSoftness, { 0.01f, 1 }, "%.3f", true );

            ImGui::EndTable();
        }

        if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "particle_animation", "Animation" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) &&
             BeginParticleSettingsRows( "##AnimationSettings" ) ) {
            changed |= imgui::CheckBox( ParticleControlRow( "particle_fade_enabled" ), settings.mFadeEnabled );

            ImGui::BeginDisabled( !settings.mFadeEnabled );

            // Display lifetime percentages while keeping the emitter recipe normalized.
            auto fadeIn{ settings.mFadeIn * 100.0f };
            auto fadeOut{ settings.mFadeOut * 100.0f };

            changed |= DrawFloatProperty( ParticleControlRow( "particle_fade_in" ), &fadeIn, { 0, 100 }, "%.1f %%", true );
            changed |= DrawFloatProperty( ParticleControlRow( "particle_fade_out" ), &fadeOut, { 0, 100 }, "%.1f %%", true );

            settings.mFadeIn = fadeIn * 0.01f;
            settings.mFadeOut = fadeOut * 0.01f;

            ImGui::EndDisabled();
            ImGui::EndTable();
        }

        return changed;
    }

    auto DrawParticleMaterial( material::ParticleMaterial &material ) -> bool {
        bool changed{};

        if ( BeginParticleSettingsRows( "##ParticleMaterialProperties" ) ) {
            auto color{ material.GetColor() };

            if ( ImGui::ColorEdit4( ParticleControlRow( "material_editor_particle_tint" ).c_str(), &color.x ) ) {
                material.SetColor( color );
                changed = true;
            }

            bool bloom{ material.IsBloomEnabled() };
            if ( imgui::CheckBox( ParticleControlRow( "particle_material_bloom" ).c_str(), bloom ) ) {
                material.SetBloomEnabled( bloom );
                changed = true;
            }
            ImGui::BeginDisabled( !bloom );
            float intensity{ material.GetBloomIntensity() };
            if ( DrawFloatProperty( ParticleControlRow( "particle_material_bloom_intensity" ).c_str(), &intensity, { 0.0f, 64.0f }, "%.2f", false ) ) {
                material.SetBloomIntensity( intensity );
                changed = true;
            }
            ImGui::EndDisabled();
            ImGui::EndTable();
        }
        
        ImGui::TextUnformatted( MKT_LOC( "particle_effect_billboard_texture" ).c_str() );

        bool chooseTexture{};
        bool clearTexture{};
        filesystem::Path droppedPath{};

        const auto currentPath{ material.GetTexturePath() };
        const auto texture{ material.GetTexture() };
        const auto preview{ texture ? texture : asset::AssetRegistryService::Get()->GetDummyTexture() };

        // Both the thumbnail and filename accept the same content-browser texture payload.
        const auto acceptDrop{ [&]() {
            if ( ImGui::BeginDragDropTarget() ) {
                if ( const auto *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_IMAGE" ) } ) {
                    droppedPath = filesystem::Path{ static_cast<core::cstr>( payload->Data ) };
                }

                ImGui::EndDragDropTarget();
            }
        } };

        const auto spacing{ ImGui::GetStyle().ItemSpacing.x };
        const auto previewSize{ ImGui::GetFrameHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y };

        if ( BeginParticleSettingsRows( "##ParticleTextureSlot" ) ) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            {
                const imgui::ImGuiScopedStyleVar padding{ ImGuiStyleVar_FramePadding, ImVec2{ 2, 2 } };
                chooseTexture = imgui::PushImageButton( "##ParticleTexturePreview",
                    imgui::ImGuiService::Get()->GetTextureID( preview ), { previewSize - 4, previewSize - 4 } );
            }

            acceptDrop();

            ImGui::TableNextColumn();

            const auto caption{ currentPath.IsEmpty() ? MKT_LOC( "material_editor_choose_texture" ) : eastl::string{ currentPath.GetFilename() } };
            const auto clearWidth{ ImGui::GetFrameHeight() };
            const auto chooseWidth{ eastl::max( 1.0f, ImGui::GetContentRegionAvail().x - clearWidth - spacing ) };

            chooseTexture |= ImGui::Button( string::Format( "{}###ChooseParticleTexture", caption ).c_str(), { chooseWidth, 0 } );
            acceptDrop();

            if ( !currentPath.IsEmpty() ) {
                imgui::widget::ItemTooltip( "%s", currentPath.GetC_Str() );
            }

            ImGui::SameLine();
            ImGui::BeginDisabled( currentPath.IsEmpty() );

            clearTexture = imgui::widget::IconButton( "##ClearParticleTexture", ICON_MD_CLOSE, { ImGui::GetFrameHeight(), ImGui::GetFrameHeight() } );
            imgui::widget::ItemTooltip( "%s", MKT_LOC( "inspector_remove_texture" ).c_str() );

            ImGui::EndDisabled();
            ImGui::EndTable();
        }

        // Finish the ImGui table before asset loading can throw.
        if ( chooseTexture ) {
            droppedPath = filesystem::OpenFileDialog( { { MKT_LOC( "material_editor_texture" ).c_str(), "png,jpg,jpeg,tga,bmp,ktx2" } } );
        }

        if ( !droppedPath.IsEmpty() ) {
            auto loaded{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>(
                droppedPath, renderer::rhi::TextureDimension::eTexture2D ) };

            if ( !loaded.IsReady() ) {
                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "material_editor_texture_could_not_be_loaded" ).c_str() );
            }

            material.SetTexture( eastl::move( loaded ) );
            changed = true;
        }

        if ( clearTexture ) {
            material.SetTexture( {} );
            changed = true;
        }

        return changed;
    }

    auto DrawParticleEmitterMaterial( material::ParticleMaterialHandle &material, bool sandbox ) -> bool {
        bool hasChanged{};
        bool choose{};
        bool create{};
        bool clear{};

        filesystem::Path materialPath{};

        const auto acceptDrop{ [&]() {
            if ( ImGui::BeginDragDropTarget() ) {
                if ( const auto *payload{ ImGui::AcceptDragDropPayload( "CONTENT_BROWSER_TEXT_MATERIAL" ) } ) {
                    if ( payload->DataSize > 0 && static_cast<core::cstr>( payload->Data )[payload->DataSize - 1] == '\0' ) {
                        materialPath = filesystem::Path{ static_cast<core::cstr>( payload->Data ) };
                    }
                }

                ImGui::EndDragDropTarget();
            }
        } };

        ImGui::SeparatorText( MKT_LOC( "material_editor_particle_material" ).c_str() );

        if ( BeginParticleSettingsRows( "##EmitterMaterialActions" ) ) {
            ( void )ParticleControlRow( "material_editor_name" );
            ImGui::TextWrapped( "%s", material ? material->GetName().c_str() : MKT_LOC( "texture_render_no_material" ).c_str() );
            acceptDrop();

            ( void )ParticleControlRow( "material_editor_material" );

            const auto clearWidth{ ImGui::GetFrameHeight() };
            const auto chooseWidth{ eastl::max( 1.0f, ImGui::GetContentRegionAvail().x - clearWidth - ImGui::GetStyle().ItemSpacing.x ) };

            // The material's internal name can be identical in several assets.
            // Show the assigned file while keeping this picker/drop target's ID stable.
            const auto source{ material ? material->GetSourcePath() : filesystem::Path{} };
            const auto label{ material
                                  ? source.IsEmpty() ? material->GetName() : eastl::string{ source.GetFilename() }
                                  : MKT_LOC( "volume_choose_material" ) };
            const auto buttonLabel{ string::Format( "{}###EmitterMaterial", label ) };

            choose = ImGui::Button( buttonLabel.c_str(), { chooseWidth, 0 } );
            if ( material ) {
                imgui::widget::ItemTooltip( "%s", source.IsEmpty() ? material->GetName().c_str() : source.GetC_Str() );
            }

            acceptDrop();
            ImGui::SameLine();
            ImGui::BeginDisabled( !material );

            clear = imgui::widget::IconButton( "##ClearEmitterMaterial", ICON_MD_CLOSE, { clearWidth, clearWidth } );
            imgui::widget::ItemTooltip( "%s", MKT_LOC( "inspector_clear" ).c_str() );
            ImGui::EndDisabled();

            ( void )ParticleControlRow( "particle_effect_create_material" );
            create = ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_new", "New##EmitterMaterial" ).c_str(), { -1, 0 } );

            ImGui::EndTable();
        }

        // Finish the table before the shared picker/drop load can throw.
        if ( choose ) {
            materialPath = filesystem::OpenFileDialog( { { MKT_LOC( "material_editor_mikoto_material" ).c_str(), "mktmtl" } } );
        }

        if ( !materialPath.IsEmpty() ) {
            const auto asset{ asset::AssetRegistryService::Get()->LoadAsset<asset::Material>( materialPath ) };
            const auto loaded{ asset ? asset->GetResource() : material::MaterialHandle{} };

            if ( !dynamic_cast<const material::ParticleMaterial *>( loaded.GetPtr() ) ) {
                MKT_THROW_RUNTIME_ERROR( MKT_LOC( "particle_effect_invalid_material" ).c_str() );
            }

            material = loaded.As<material::ParticleMaterial>();
            hasChanged = true;
        }

        if ( create ) {
            material = material::ParticleMaterialHandle::New( material::ParticleMaterialDescription{} );
            hasChanged = true;
        }

        if ( clear ) {
            material = {};
            hasChanged = true;
        }

        if ( material ) {
            hasChanged |= DrawParticleMaterial( *material );

            bool save{};
            bool saveAs{};

            if ( BeginParticleSettingsRows( "##EmitterMaterialPersistence" ) ) {
                ImGui::BeginDisabled( sandbox );

                ( void )ParticleControlRow( "inspector_save_material" );
                save = ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_save", "Save##EmitterMaterial" ).c_str(), { -1, 0 } );

                ( void )ParticleControlRow( "material_editor_save_as" );
                saveAs = ImGui::Button( imgui::widget::LocalizedLabel( "material_editor_save_as", "SaveAs##EmitterMaterial" ).c_str(), { -1, 0 } );

                ImGui::EndDisabled();
                ImGui::EndTable();
            }

            if ( save || saveAs ) {
                auto file{ material->GetSourcePath() };

                if ( saveAs || file.IsEmpty() ) {
                    file = filesystem::SaveFileDialog( "ParticleMaterial.mktmtl", { { MKT_LOC( "material_editor_mikoto_material" ).c_str(), "mktmtl" } } );
                }

                if ( !file.IsEmpty() ) {
                    material->Serialize( file );
                }
            }
        }

        return hasChanged;
    }
}// namespace mikoto::editor
