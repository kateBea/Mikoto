//    Copyright 2025 ケイト
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

#ifndef MIKOTO_ROOT_HH
#define MIKOTO_ROOT_HH

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

    class Root final {
    public:

        static auto Init(const RootConfig& config) -> void;
        static auto Shutdown() -> void;

        static auto UpdateState(float timeStep) -> void;

        template<typename ServiceType, typename... Args>
        MKT_NODISCARD static auto RegisterService(Args&&... args) -> bool {
            try {
                if (ServiceType *service{
                s_Services.Register<ServiceType>( std::forward<Args>(args)... ) }) {
                    service->Init();
                }
            } catch(std::exception& e) {
                MKT_CORE_LOGGER_ERROR( "Failed to register service. Error: {}", e.what() );
            }

            return true;
        }

        DISABLE_COPY_AND_MOVE_FOR(Root);

    private:
        static inline Registry<IService> s_Services{};
    };

}

#endif //MIKOTO_ROOT_HH
