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

#include <Project/Project.hh>

namespace mikoto::editor {

    Project::Project( eastl::string_view name )
        : mName{ name }
    {

    }

    auto Project::Serialize( const filesystem::Path &filename ) const -> void {

    }

    auto Project::Deserialize( const filesystem::Path &filename ) const -> void {

    }

    auto Project::Serialize( filesystem::FileHandle file ) const -> void {

    }

    auto Project::Deserialize( filesystem::FileHandle file ) const -> void {

    }

    auto Project::SetName( eastl::string_view name ) -> void {
        if (name.empty()) {
            return;
        }

        mName = name;
    }

    auto Project::GetName() const -> eastl::string_view {
        return mName;
    }
}// namespace Mikoto