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

#ifndef MIKOTO_WINDOWS_SYSTEM_HH
#define MIKOTO_WINDOWS_SYSTEM_HH

#include <vector>

#include <Platform/Window.hh>

#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct WindowsServiceCreateInfo {

    };

    class WindowsService final : public IService, public Singleton<WindowsService> {
    public:
        explicit WindowsService(WindowsServiceCreateInfo const &serviceCreateInfo);

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto Create(const WindowProperties& properties) -> Window*;

    private:
        auto InitWindowHandling() -> void;
        auto ShutdownWindowHandling() -> void;

    private:
        std::vector<Unique<Window>> m_Windows{};
    };
}


#endif // MIKOTO_WINDOWS_SYSTEM_HH