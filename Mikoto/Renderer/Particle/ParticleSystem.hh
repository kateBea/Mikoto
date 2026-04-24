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

#ifndef MIKOTO_PARTICLE_SYSTEM_HH
#define MIKOTO_PARTICLE_SYSTEM_HH

#include <Core/Subsystem.hh>
#include <Core/Singleton.hh>

namespace mikoto::renderer {

    struct ParticleSystemCreateInfo {

    };

    class ParticleSystem final : public core::ISubsystem, public core::Singleton<ParticleSystem> {
    public:

        explicit ParticleSystem(const ParticleSystemCreateInfo& info);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;
        auto Update( float ts ) -> void override;

    private:

    };
}


#endif//MIKOTOROOT_PARTICLE_SYSTEM_HH
