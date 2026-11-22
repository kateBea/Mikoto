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

#include <filesystem>

#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Assets/AssetParticleEffect.hh>
#include <Assets/AssetRegistryService.hh>

#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::asset {

    using namespace mikoto::core;

    ParticleEffect::ParticleEffect( eastl::vector<renderer::ParticleEmitterHandle> emitters )
        : mEmitters{ eastl::move( emitters ) } {
        
    }

    auto ParticleEffect::Serialize( YAML::Emitter& out, const filesystem::Path& effectFile ) const -> void {
        MKT_BEGIN_PROFILER();

        const auto directory{ effectFile.IsEmpty() ? 
            std::filesystem::path{} :
            effectFile.ToAbsolute().GetPathTyped<std::filesystem::path>().parent_path() };

        out << YAML::BeginMap << YAML::Key << "Version" << YAML::Value << 1;
        out << YAML::Key << "Emitters" << YAML::Value << YAML::BeginSeq;

        for ( const auto& emitter: mEmitters ) {
            out << YAML::BeginMap << YAML::Key << "Settings" << YAML::Value;
            emitter->GetSettings().Serialize( out );

            if ( const auto& material{ emitter->GetMaterial() } ) {
                const auto& source{ material->GetSourcePath() };

                if ( source.IsEmpty() ) {
                    MKT_THROW_RUNTIME_ERROR( "Save the particle material before saving the effect." );
                }

                const auto absolute{ source.ToAbsolute().GetPathTyped<std::filesystem::path>() };
                const auto relative{ directory.empty() ? std::filesystem::path{} : absolute.lexically_relative( directory ) };
                const auto utf8{ ( relative.empty() ? absolute : relative ).generic_u8string() };

                out << YAML::Key << "MaterialPath" << YAML::Value
                    << eastl::string{ reinterpret_cast<const char*>( utf8.data() ), utf8.size() }.c_str();
            }

            out << YAML::EndMap;
        }

        out << YAML::EndSeq << YAML::EndMap;
    }

    auto ParticleEffect::Deserialize( const YAML::Node& node, const filesystem::Path& effectFile ) -> ParticleEffectHandle {
        eastl::vector<renderer::ParticleEmitterHandle> emitters{};
        const auto entries{ node["Emitters"] };

        // The original effect and scene formats stored one emitter's settings directly.
        if ( !entries ) {
            emitters.push_back( renderer::ParticleEmitterHandle::New( renderer::ParticleEmitterCreateDescription{
                .mSettings = renderer::ParticleEmitterSettings::Deserialize( node )
            } ) );
            return ParticleEffectHandle::New( eastl::move( emitters ) );
        }

        if ( node["Version"].as<core::u32>( 0 ) != 1 || !entries.IsSequence() ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid particle effect version or emitter list." );
        }

        emitters.reserve( entries.size() );
        for ( const auto& entry: entries ) {
            if ( !entry.IsMap() || !entry["Settings"].IsMap() ) {
                MKT_THROW_RUNTIME_ERROR( "Missing particle emitter settings." );
            }

            renderer::ParticleEmitterCreateDescription description{
                .mSettings = renderer::ParticleEmitterSettings::Deserialize( entry["Settings"] )
            };

            if ( const auto materialPath{ entry["MaterialPath"] } ) {
                auto path{ std::filesystem::u8path( materialPath.as<std::string>() ) };

                if ( path.is_relative() && !effectFile.IsEmpty() ) {
                    path = effectFile.ToAbsolute().GetPathTyped<std::filesystem::path>().parent_path() / path;
                }

                const auto asset{ AssetRegistryService::Get()->LoadAsset<Material>( filesystem::Path{ path.lexically_normal() } ) };
                const auto material{ asset ? asset->GetResource() : material::MaterialHandle{} };

                if ( !dynamic_cast<const material::ParticleMaterial*>( material.GetPtr() ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Particle emitter material must be a valid particle material asset." );
                }

                description.mMaterial = material.As<material::ParticleMaterial>();
            }

            emitters.push_back( renderer::ParticleEmitterHandle::New( description ) );
        }

        return ParticleEffectHandle::New( eastl::move( emitters ) );
    }

    auto ParticleEffect::GetEmitters() const -> const eastl::vector<renderer::ParticleEmitterHandle>& {
        return mEmitters;
    }

    auto ParticleEffect::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        return { sizeof( *this ) + mEmitters.capacity() * sizeof( renderer::ParticleEmitterHandle ) +
            mEmitters.size() * sizeof( renderer::ParticleEmitter ), 0 };
    }
}// namespace mikoto::asset
