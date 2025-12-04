//
// Created by zanet on 10/1/2025.
//

#ifndef ROOT_HH
#define ROOT_HH

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Library/Data/Registry.hh>
#include <Platform/Window.hh>

namespace Mikoto {

    struct RootConfig {
        bool LockFrameRate{ false };

        Window* TargetWindow{ nullptr };
        GraphicsAPI TargetApi{ GraphicsAPI::VULKAN_API };
    };

    // handles services lifetime and dependencies
    class Root {
    public:

        static auto Init(const RootConfig& config) -> void;
        static auto Shutdown() -> void;

        static auto UpdateState(float timeStep) -> void;

        DISABLE_COPY_AND_MOVE_FOR(Root);

    private:

        static inline Registry<IService> s_Services{};
    };

}


#endif //ROOT_HH
