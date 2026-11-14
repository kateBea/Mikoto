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

#include <Assets/Asset.hh>

#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::asset {
    struct ParticleEffectLoadDescription {
        filesystem::Path mPath{};
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    /** Owns the runtime resource; scenes keep AssetRef/AssetHandle instead of backend objects. */
    class ParticleEffect final : public core::ReferenceCounted<ParticleEffect> {
    public:
        explicit ParticleEffect( core::Ref<renderer::ParticleEmitter> resource );
        ~ParticleEffect() override;
        MKT_NODISCARD auto GetResource() const -> core::Ref<renderer::ParticleEmitter>;
        MKT_NODISCARD auto GetSettings() const -> const renderer::ParticleEmitterSettings&;
        MKT_NODISCARD auto CanEvict() const -> bool;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;
    private:
        core::Ref<renderer::ParticleEmitter> mResource{};
    };

    template<> struct AssetTraits<ParticleEffect> {
        using Asset = ParticleEffect;
        using Description = ParticleEffectLoadDescription;
        static constexpr AssetType kType{ AssetType::eParticle };
    };

    template<>
    struct AssetTraits<renderer::ParticleEmitter> : public AssetTraits<ParticleEffect> {

    };
}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_PARTICLE_EFFECT_HH
