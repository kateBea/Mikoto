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

#ifndef MIKOTO_ASSETS_ASSET_SCRIPT_SOURCE_HH
#define MIKOTO_ASSETS_ASSET_SCRIPT_SOURCE_HH

#include <Core/ReferenceCounted.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>

#include <Assets/Asset.hh>

namespace mikoto::asset {

    struct ScriptLoadDescription {
        filesystem::Path mPath{};
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    // Source residency is independent of entity-bound Lua/managed instances.
    // Constructed by streaming workers; loading never executes the script.
    class ScriptSource final : public core::ReferenceCounted<ScriptSource> {
    public:
        explicit ScriptSource( const filesystem::Path& path );

        auto GetFile() const -> filesystem::FileHandle;
        auto GetResidentMemoryUsage() const -> AssetMemoryUsage;
    private:
        filesystem::FileHandle mFile{};
    };

    template<>
    struct AssetTraits<ScriptSource> {
        using Asset = ScriptSource;
        using Description = ScriptLoadDescription;

        static constexpr AssetType kType{ AssetType::eScript };
    };
}
#endif
