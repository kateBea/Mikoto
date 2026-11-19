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

#include <Core/Exception.hh>

#include <Assets/AssetParticleEffect.hh>

#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::asset {

    ParticleEffect::ParticleEffect( core::Ref<renderer::ParticleEmitter> resource )
        : mResource{ eastl::move( resource ) } {
        if ( !mResource ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot construct ParticleEffect without a resource." );
        }
    }

    auto ParticleEffect::GetResource() const -> core::Ref<renderer::ParticleEmitter> {
        return mResource;
    }

    auto ParticleEffect::GetSettings() const -> const renderer::ParticleEmitterSettings& {
        return mResource->GetSettings();
    }

    auto ParticleEffect::CanEvict() const -> bool {
        return mResource->GetRefCount() == 1;
    }

    auto ParticleEffect::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        return { sizeof( *this ), 0 };
    }
}// namespace mikoto::asset
