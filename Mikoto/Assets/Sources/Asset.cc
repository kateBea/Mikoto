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

#include <random>
#include <filesystem>

#include <EASTL/functional.h>

#include <yaml-cpp/yaml.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Assets/Asset.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;


    auto GetHashedAssetID( const filesystem::Path& path ) -> AssetID {
        MKT_BEGIN_PROFILER();
        return eastl::hash<eastl::string>{}( path.GetAbsolute() );
    }


    auto AssetMetadata::SetID( AssetID id ) -> void {
        if ( mID == id ) {
            return;
        }

        mID = id;
        BumpRevision();
    }

    auto AssetMetadata::GetID() const -> AssetID {
        return mID;
    }

    auto AssetMetadata::SetType( AssetType type ) -> void {
        if ( mType == type ) {
            return;
        }

        mType = type;
        BumpRevision();
    }

    auto AssetMetadata::GetType() const -> AssetType {
        return mType;
    }

    auto AssetMetadata::SetPath( const filesystem::Path& path ) -> void {
        if ( mPath == path ) {
            return;
        }

        mPath = path;
        BumpRevision();
    }

    auto AssetMetadata::GetPath() const -> const filesystem::Path& {
        return mPath;
    }

    auto AssetMetadata::SetMemoryUsage( AssetMemoryUsage memory ) -> void {
        if ( mMemory.mCpuBytes == memory.mCpuBytes && mMemory.mGpuBytes == memory.mGpuBytes ) {
            return;
        }

        mMemory = memory;
        BumpRevision();
    }

    auto AssetMetadata::GetMemoryUsage() const -> AssetMemoryUsage {
        return mMemory;
    }

    auto AssetMetadata::SetDependencies( eastl::vector<AssetID> dependencies ) -> void {
        if ( mDependencies == dependencies ) {
            return;
        }

        mDependencies = eastl::move( dependencies );
        BumpRevision();
    }

    auto AssetMetadata::GetDependencies() const -> const eastl::vector<AssetID>& {
        return mDependencies;
    }

    auto AssetMetadata::SetSourceRevision( core::u64 revision ) -> void {
        if ( mSourceRevision == revision ) {
            return;
        }

        mSourceRevision = revision;
        BumpRevision();
    }

    auto AssetMetadata::GetSourceRevision() const -> core::u64 {
        return mSourceRevision;
    }

    auto AssetMetadata::SetPersistent( bool persistent ) -> void {
        if ( mPersistent == persistent ) {
            return;
        }

        mPersistent = persistent;
        BumpRevision();
    }

    auto AssetMetadata::IsPersistent() const -> bool {
        return mPersistent;
    }

    auto AssetMetadata::AddDependency( AssetID dependency ) -> void {
        for ( const auto existing: mDependencies ) {
            if ( existing == dependency ) {
                return;
            }
        }

        mDependencies.push_back( dependency );
        BumpRevision();
    }


}// namespace mikoto::asset
