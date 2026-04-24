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

#ifndef MIKOTO_THEME_MANAGER_HH
#define MIKOTO_THEME_MANAGER_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Theme/Theme.hh>
#include <Theme/ThemeSerializer.hh>

namespace mikoto::editor {
    using namespace mikoto::core;

    class ThemeManager final : public IService, public Singleton<ThemeManager> {
    public:
        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto ApplyTheme(std::string_view name) -> void;

        auto LoadTheme() -> Theme*;
        auto SaveTheme(Theme* theme) -> void;

        MKT_NODISCARD auto GetTheme(eastl::string_view name) -> Theme*;

        ~ThemeManager() override = default;

    private:
        eastl::unique_ptr<ThemeSerializer> m_Serializer{};
        ankerl::unordered_dense::map<eastl::string, eastl::unique_ptr<Theme>> m_Themes{};
    };
}


#endif //MIKOTO_THEME_MANAGER_HH