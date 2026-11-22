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

#ifndef MIKOTO_APPLICATION_PARTICLE_PROPERTIES_HH
#define MIKOTO_APPLICATION_PARTICLE_PROPERTIES_HH

#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::editor {

    MKT_NODISCARD auto BeginParticleSettingsRows( core::cstr id ) -> bool;
    MKT_NODISCARD auto ParticleControlRow( core::cstr key ) -> eastl::string;

    // Share authoring controls so the particle workspace and inspector expose the same settings.
    auto DrawParticleEmitterSettings( renderer::ParticleEmitterSettings& settings ) -> bool;
    auto DrawParticleEmitterMaterial( material::ParticleMaterialHandle& material, bool sandbox ) -> bool;

    auto DrawParticleMaterial( material::ParticleMaterial& material ) -> bool;

}// namespace mikoto::editor

#endif// MIKOTO_APPLICATION_PARTICLE_PROPERTIES_HH
