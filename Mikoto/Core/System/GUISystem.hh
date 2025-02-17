//
// Created by zanet on 1/26/2025.
//

#ifndef GUISYSTEM_HH
#define GUISYSTEM_HH


#include <Core/Engine.hh>

namespace Mikoto {
    class GUISystem final : public IEngineSystem {
    public:
        explicit GUISystem(const EngineConfig& options) {

        }

        ~GUISystem() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update() -> void override;
    };

}


#endif //GUISYSTEM_HH
