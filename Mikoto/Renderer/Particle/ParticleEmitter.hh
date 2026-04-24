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

#ifndef MIKOTOROOT_PARTICLE_EMITTER_HH
#define MIKOTOROOT_PARTICLE_EMITTER_HH

namespace mikoto::renderer {

    class ParticleEmitter final /*: public IResource*/ {
        // float EmissionRate{};
        // float Lifetime{};
        // float Speed{};
        // UInt32 MaxParticles{};

        // Other properties (texture it uses, etc)
    };

    // It is important to have an instance because
    // this is what will control the particles for every scene object
    // Imagine we might have a fire effect, this will can be shared with all torches for example
    // instead of having a separate specific type of emitter for all of them
    class ParticleEmitterInstance final {
        // const ParticleEmitter* Emitter{};
        //
        // std::vector<Particle> Particles{};
        // float EmissionAccumulator{};
    };
}


#endif//MIKOTOROOT_PARTICLE_EMITTER_HH
