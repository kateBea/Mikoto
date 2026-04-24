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

#ifndef MIKOTO_WINDOWS_SYSTEM_HH
#define MIKOTO_WINDOWS_SYSTEM_HH

#include <EASTL/memory.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>
#include <Platform/Window.hh>

namespace mikoto::platform {

    using namespace mikoto::core;

    struct WindowsServiceCreateInfo {};

    class WindowsService final : public IService, public Singleton<WindowsService> {
    public:
        explicit WindowsService(WindowsServiceCreateInfo const &serviceCreateInfo);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto Create(const WindowProperties& properties) -> Window*;

    private:
        auto InitWindowHandling() -> void;
        auto ShutdownWindowHandling() -> void;

    private:
        eastl::vector<eastl::unique_ptr<Window>> mWindows{};
    };
}

#endif // MIKOTO_WINDOWS_SYSTEM_HH