//
// Created by zanet on 1/26/2025.
//

#ifndef AUDIOSYSTEM_HH
#define AUDIOSYSTEM_HH

#include <Core/Engine.hh>

namespace Mikoto {
    class AudioSystem final : public IEngineSystem {
    public:
        explicit AudioSystem() = default;
        explicit AudioSystem(const EngineConfig& options) {

        }

        ~AudioSystem() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;
    };

}



#endif //AUDIOSYSTEM_HH
