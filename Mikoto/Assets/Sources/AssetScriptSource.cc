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

#include <Assets/AssetScriptSource.hh>

namespace mikoto::asset {
    ScriptSource::ScriptSource( const filesystem::Path& path )
        : mFile{ filesystem::FileHandle::New( path ) } {
    }


    auto ScriptSource::GetFile() const -> filesystem::FileHandle {
        return mFile;
    }


    auto ScriptSource::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        return { mFile->GetSize(), 0 };
    }
}
