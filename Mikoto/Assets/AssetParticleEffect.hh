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

#ifndef MIKOTO_ASSETS_ASSET_PARTICLE_EFFECT_HH
#define MIKOTO_ASSETS_ASSET_PARTICLE_EFFECT_HH

#include <EASTL/vector.h>

#include <yaml-cpp/yaml.h>

#include <Assets/Asset.hh>

#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::asset {

    struct ParticleEffectLoadDescription {
        filesystem::Path mPath{};
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    // TODO: Like other assets impleemtn serialization interface (ISerializable)
    // as all assets can be serialized to disk and deserialized from disk.
    class ParticleEffect final : public core::ReferenceCounted<ParticleEffect> {
    public:
        explicit ParticleEffect( eastl::vector<renderer::ParticleEmitterHandle> emitters );

        auto Serialize( YAML::Emitter& out, const filesystem::Path& effectFile = {} ) const -> void;
        MKT_NODISCARD static auto Deserialize( const YAML::Node& node, const filesystem::Path& effectFile = {} ) -> core::Ref<ParticleEffect>;

        MKT_NODISCARD auto GetEmitters() const -> const eastl::vector<renderer::ParticleEmitterHandle>&;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;

    private:
        eastl::vector<renderer::ParticleEmitterHandle> mEmitters{};
    };

    using ParticleEffectHandle = core::Ref<ParticleEffect>;

    template<> struct AssetTraits<ParticleEffect> {
        using Asset = ParticleEffect;
        using Description = ParticleEffectLoadDescription;
        static constexpr AssetType kType{ AssetType::eParticle };
    };

}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_PARTICLE_EFFECT_HH
