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

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <Logging/Logger.hh>

#include <Theme/Theme.hh>
#include <Theme/ThemeManager.hh>

namespace mikoto::editor {

    auto ThemeManager::Initialize() -> void {

        mIsInitialized = true;
    }

    auto ThemeManager::Shutdown() -> void {
        if (!mIsInitialized) {
            return;
        }

        mIsInitialized = false;
    }

    auto ThemeManager::ApplyTheme( std::string_view name ) -> void {

    }

    auto ThemeManager::LoadTheme() -> Theme * {
        // Open file dialog and deserialize the theme

        return nullptr;
    }

    auto ThemeManager::SaveTheme( Theme *theme ) -> void {
        // Open file dialog and serialize the theme
    }

    auto ThemeManager::GetTheme( eastl::string_view name ) -> Theme * {
        return nullptr;
    }
}