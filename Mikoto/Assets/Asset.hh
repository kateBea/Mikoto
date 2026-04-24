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

#ifndef MIKOTO_ASSET_HH
#define MIKOTO_ASSET_HH

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;

    using AssetID = u64;

    MKT_NODISCARD inline auto GetHashedAssetID(const Path& path) -> AssetID {
        return eastl::hash<eastl::string>{}(path.GetPathTyped<eastl::string>());
    }

}
#endif//MIKOTO_ASSET_HH
