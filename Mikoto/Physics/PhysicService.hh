//
// Created by zanet on 1/26/2025.
//

#ifndef PHYSICSSYSTEM_HH
#define PHYSICSSYSTEM_HH

#include <Common/Service.hh>

namespace Mikoto {
    struct PhysicServiceCreateInfo {

    };

    class PhysicService final : public IService<PhysicService> {
    public:
        explicit PhysicService(const PhysicServiceCreateInfo& options);

        ~PhysicService() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;
        auto Update(float dt) -> void;
    };

}

#endif //PHYSICSSYSTEM_HH
