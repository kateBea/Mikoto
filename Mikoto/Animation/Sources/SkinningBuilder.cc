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

#include <mutex>

#include <EASTL/array.h>

#include <nlohmann/json.hpp>

#include <ozz/base/io/archive.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Assets/AssetRegistryService.hh>

#include <Animation/Utility.hh>
#include <Animation/SkinningBuilder.hh>

namespace mikoto::animation {

    using namespace core;
    using namespace asset;
    using namespace filesystem;

    static auto DescribeAnimationArchive( const filesystem::Path& path ) -> nlohmann::json {
        MKT_BEGIN_PROFILER();
        filesystem::File file{ path };
        return { { "file", path.GetFilename().data() }, { "size", file.GetSize() }, { "hash", HashAnimationData( 0, file.GetContentsBytes(), file.GetSize() ) } };
    }

    template<typename T>
    MKT_NODISCARD static auto ReadArchive( const filesystem::Path& directory, const nlohmann::json& entry ) -> ozz::unique_ptr<T> {
        MKT_BEGIN_PROFILER();
        const auto filename{ entry.at( "file" ).get<std::string>() };
        const filesystem::Path relative{ filename.c_str() };

        if ( relative.IsAbsolutePath() || relative.GetFilename() != filename.c_str() || relative.GetExtension() != ".ozz" ) {
            return nullptr;
        }

        const filesystem::Path path{ filesystem::PathBuilder{}
            .SetPath( directory )
            .SetPath( relative )
            .Build() };

        if ( !path.IsFile() ) {
            return nullptr;
        }

        File file{ path };

        if ( file.GetSize() < 16 || file.GetSize() > static_cast<core::usize>( std::numeric_limits<core::i32>::max() ) ||
             file.GetSize() != entry.at( "size" ).get<core::usize>() ||
             HashAnimationData( 0, file.GetContentsBytes(), file.GetSize() ) != entry.at( "hash" ).get<core::u64>() ) {
            return nullptr;
        }

        // Verify the complete bytes before handing them to Ozz: its archive
        // reader asserts on truncated files. File handles Unicode Windows paths;
        // the Ozz stream therefore never has to reopen a narrow native filename.
        ozz::io::MemoryStream stream{};

        if ( stream.Write( file.GetContentsBytes(), file.GetSize() ) != file.GetSize() || stream.Seek( 0, ozz::io::Stream::kSet ) != 0 ) {
            return nullptr;
        }

        ozz::io::IArchive archive{ &stream };

        if ( !archive.TestTag<T>() ) {
            return nullptr;
        }

        auto result{ ozz::make_unique<T>() };
        archive >> *result;

        return result;
    }

    MKT_NODISCARD static auto ReadAnimationCache(
        const filesystem::Path& directory,
        const nlohmann::json& manifest ) -> eastl::unique_ptr<SkinningDescription> {
        MKT_BEGIN_PROFILER();

        auto skeleton{ ReadArchive<ozz::animation::Skeleton>( directory, manifest.at( "skeleton" ) ) };

        if ( !skeleton || skeleton->num_joints() == 0 ) {
            return nullptr;
        }

        const core::i32 joints{ skeleton->num_joints() };
        auto result{ eastl::make_unique<SkinningDescription>() };
        result->mSkeleton = eastl::make_unique<Skeleton>( eastl::move( skeleton ) );

        const auto& clips{ manifest.at( "clips" ) };

        // A validated skeleton with zero clips is a complete deformable asset.
        // Still require the clips array so a malformed manifest is not accepted.
        if ( !clips.is_array() || clips.size() > 8192 ) {
            return nullptr;
        }

        for ( const auto& clip: clips ) {
            auto animation{ ReadArchive<ozz::animation::Animation>( directory, clip ) };

            if ( !animation || animation->num_tracks() != joints ) {
                return nullptr;
            }

            const auto name{ clip.at( "name" ).get<std::string>() };

            if ( !result->mAnimations.try_emplace( name.c_str(), eastl::make_unique<SkinnedAnimation>( eastl::move( animation ) ) ).second ) {
                return nullptr;
            }
        }

        return result;
    }

    auto SkinningBuilder::SetPath( const filesystem::Path& path ) -> SkinningBuilder& {
        MKT_BEGIN_PROFILER();
        mFilename = path;
        return *this;
    }

    auto SkinningBuilder::SetImporter( ozz::animation::offline::OzzImporter& importer ) -> SkinningBuilder& {
        MKT_BEGIN_PROFILER();
        mImporter = &importer;
        return *this;
    }

    auto SkinningBuilder::SetSourceFingerprint( core::u64 fingerprint ) -> SkinningBuilder& {
        MKT_BEGIN_PROFILER();
        mSourceFingerprint = fingerprint;
        return *this;
    }

    auto SkinningBuilder::SetImportAnimations( bool enabled ) -> SkinningBuilder& {
        MKT_BEGIN_PROFILER();
        mImportAnimations = enabled;
        return *this;
    }

    auto SkinningBuilder::ReadSource( const filesystem::Path& path, core::u64 fingerprint ) -> SkinningImportSource {
        filesystem::File configuration{ "Resources/AnimationConfiguration.json" };
        filesystem::File source{ path };
        fingerprint = HashAnimationData( fingerprint, source.GetContentsBytes(), source.GetSize() );
        fingerprint = HashAnimationData( fingerprint, configuration.GetContentsBytes(), configuration.GetSize() );
        return { fingerprint, configuration.GetContentsString() };
    }

    auto SkinningBuilder::SetSource( const SkinningImportSource& source ) -> SkinningBuilder& {
        mSource = &source;
        return *this;
    }

    auto SkinningBuilder::Build() -> eastl::unique_ptr<SkinningDescription> {
        MKT_BEGIN_PROFILER();

        if ( !mImporter ) {
            return eastl::make_unique<SkinningDescription>();
        }

        // Callers importing several rigs share the expensive byte fingerprint.
        // Keep standalone importers working without requiring a separate setup step.
        const auto ownedSource{ mSource ? SkinningImportSource{} : ReadSource( mFilename ) };
        const auto& source{ mSource ? *mSource : ownedSource };
        const core::u64 fingerprint{ HashAnimationData( source.mFingerprint, &mSourceFingerprint, sizeof( mSourceFingerprint ) ) };

        // Bump the schema when importer behavior changes. Runtime archive versions
        // also invalidate the cache when the Ozz dependency changes format.
        constexpr core::u32 schema{ 5 };
        const auto sourcePath{ mFilename.GetAbsolute() };
        const nlohmann::json identity{
            { "schema", schema },
            { "source", sourcePath.c_str() },
            { "fingerprint", fingerprint },
            { "import_animations", mImportAnimations },
            { "skeleton_version", ozz::io::internal::Version<const ozz::animation::Skeleton>::kValue },
            { "animation_version", ozz::io::internal::Version<const ozz::animation::Animation>::kValue }
        };

        const auto identityText{ identity.dump() };
        const core::u64 cacheKey{ HashAnimationData( 0, identityText.data(), identityText.size() ) };
        const filesystem::Path directory{ filesystem::PathBuilder{}
            .SetPath( asset::AssetRegistryService::Get()->GetAssetCacheBasePath() )
            .SetPath( string::Format( "{}/Animations/{:016x}", GetHashedAssetID( mFilename ), cacheKey ) )
            .Build() };

        const filesystem::Path manifestPath{ filesystem::PathBuilder{}
            .SetPath( directory )
            .SetPath( "manifest.json" )
            .Build() };

        const auto tryCached{ [&]() -> eastl::unique_ptr<SkinningDescription> {
            if ( !manifestPath.IsFile() ) {
                return nullptr;
            }

            try {
                File file{ manifestPath };
                const auto manifest{ nlohmann::json::parse( file.GetContentsString().c_str() ) };

                if ( manifest.at( "identity" ) != identity ) {
                    return nullptr;
                }

                return ReadAnimationCache( directory, manifest );
            } catch ( const std::exception& ) {
                return nullptr;
            }
        } };

        // Validated cache reads need no global importer lock. Each archive is
        // verified from a private byte snapshot before Ozz reads it.
        if ( auto cached{ tryCached() } ) {
            MKT_CORE_LOGGER_DEBUG( "Reusing Ozz animation cache for {}", sourcePath );
            return cached;
        }

        // Import2ozz owns process-global command-line options. Serialize cache
        // misses, then check again in case another worker finished this asset.
        static std::mutex importerMutex{};
        const std::lock_guard lock{ importerMutex };

        if ( auto cached{ tryCached() } ) {
            MKT_CORE_LOGGER_DEBUG( "Reusing Ozz animation cache for {}", sourcePath );
            return cached;
        }

        ( void )filesystem::CreateIfNotExistsDirectory( directory );
        auto config{ nlohmann::json::parse( source.mConfiguration.c_str() ) };
        const filesystem::Path skeletonPath{ filesystem::PathBuilder{}
            .SetPath( directory )
            .SetPath( "skeleton.ozz" )
            .Build() };

        config["skeleton"]["filename"] = skeletonPath.GetC_Str();

        if ( !mImportAnimations ) {
            // Skeleton-only imports are valid, not a missing-animation error.
            config["animations"] = nlohmann::json::array();
        }

        for ( auto& animation: config.at( "animations" ) ) {
            animation["filename"] = string::Format( "{}/clip_*.ozz", directory.GetC_Str() ).c_str();
        }

        const auto configArg{ string::Format( "--config={}", config.dump().c_str() ) };
        const auto fileArg{ string::Format( "--file={}", sourcePath ) };

        eastl::array args{ "mikoto-animation-import", fileArg.c_str(), "--log_level=standard", configArg.c_str() };

        if ( ( *mImporter )( static_cast<core::i32>( args.size() ), args.data() ) != 0 ) {
            MKT_CORE_LOGGER_ERROR( "Ozz animation import failed for {}", sourcePath );
            return eastl::make_unique<SkinningDescription>();
        }

        nlohmann::json manifest{
            { "identity", identity },
            { "skeleton", DescribeAnimationArchive( skeletonPath ) },
            { "clips", nlohmann::json::array() }
        };

        ankerl::unordered_dense::set<eastl::string> filenames{};

        for ( const auto& name: mImporter->GetAnimationNames() ) {
            const auto filename{ mImporter->BuildFilename( "clip_*.ozz", name.c_str() ) };
            eastl::string foldedFilename{ filename.c_str() };

            foldedFilename.make_lower();

            if ( !filenames.insert( foldedFilename ).second ) {
                MKT_THROW_RUNTIME_ERROR( "Animation names produce colliding Ozz cache filenames" );
            }

            const Path path{ filesystem::PathBuilder{}
                .SetPath( directory )
                .SetPath( filename )
                .Build() };

            // A configuration may filter clips. Its fingerprint has a separate
            // directory, so excluded clips cannot be inherited from an older import.
            if ( !path.IsFile() ) {
                continue;
            }

            auto entry{ DescribeAnimationArchive( path ) };
            entry["name"] = name.c_str();
            manifest["clips"].push_back( eastl::move( entry ) );
        }

        auto result{ ReadAnimationCache( directory, manifest ) };

        if ( !result ) {
            MKT_THROW_RUNTIME_ERROR( "Imported Ozz animation data is incomplete or incompatible" );
        }

        // Publish the manifest last. Interrupted imports and stale clip files do
        // not become cache hits; only the complete named set is ever reloaded.
        try {
            FileHandle file{ FileService::Get()->CreateNewFile( manifestPath ) };

            if ( file.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot open Ozz animation manifest for writing." );
            }

            file->Write( manifest.dump( 2 ).c_str() );
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_WARN( "Cannot publish Ozz animation cache: {}", error.what() );
        }

        return result;
    }
}// namespace mikoto::animation