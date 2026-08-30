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

#ifndef MIKOTO_PROJECT_HH
#define MIKOTO_PROJECT_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Serializable.hh>
#include <Core/ReferenceCounted.hh>

#include <Filesystem/Path.hh>

namespace mikoto::editor {

    class Project final : public core::ISerializable {
    public:
        explicit Project( eastl::string_view name);

        auto Serialize( const filesystem::Path &filename ) const -> void override;
        auto Deserialize( const filesystem::Path &filename ) const -> void override;

        auto Serialize( filesystem::FileHandle file ) const -> void override;
        auto Deserialize( filesystem::FileHandle file ) const -> void override;

        auto SetName( eastl::string_view name ) -> void;
        MKT_NODISCARD auto GetName() const -> eastl::string_view;

    private:
        eastl::string mName{};
    };

    using ProjectHandle = core::Ref<Project>;
}

#endif //MIKOTO_PROJECT_HH
