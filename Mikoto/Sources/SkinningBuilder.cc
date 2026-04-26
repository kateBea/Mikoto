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

#include <exception>

#include <nlohmann/json.hpp>

#include <EASTL/array.h>

#include <ozz/base/io/stream.h>
#include <ozz/base/io/archive.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Assets/Importer.hh>
#include <Assets/AssetsService.hh>

#include <Animation/SkinningBuilder.hh>

namespace mikoto::animation {

    using namespace mikoto::core;
    using namespace mikoto::asset;
    using namespace mikoto::filesystem;

    auto SkinningBuilder::SetPath( const Path& path ) -> SkinningBuilder& {
        mFilename = path;
        return *this;
    }

    auto SkinningBuilder::SetImporter( ozz::animation::offline::OzzImporter& importer ) -> SkinningBuilder& {
        mImporter = MKT_ADDRESSOF( importer );
        return *this;
    }

    auto SkinningBuilder::Build() -> eastl::unique_ptr<SkinningDescription> {
        auto result{ eastl::make_unique<SkinningDescription>() };

        // Specify log level
#if !defined(NDEBUG)
        const eastl::string loggingLevel{ string::Format(R"(--log_level={})", "verbose") };
#else
        const eastl::string loggingLevel{ string::Format(R"(--log_level={})", "standard") };
#endif

        eastl::string configJson{};

        const Path animationsBasePath{ string::Format(  "{}/{}", GetHashedAssetID( mFilename ), kAnimationsCachePath ) };

        // Get the directory where this asset is cached
        const Path& assetsCacheBasePath{ AssetsService::Get()->GetAssetCacheBasePath() };
        const eastl::string currentAssetCachePath{ string::Format( "{}/{}", assetsCacheBasePath.GetC_Str(), animationsBasePath.GetC_Str() ) };

        // We first open the file that contains the base configuration for any animation
        // As it contains the parameters necessary to load any animation
        if (FileHandle configFile{ FileService::Get()->LoadFile( "Resources/AnimationConfiguration.json" ) }) {
            try {
                // Set the path to the skeleton.ozz file. HashedAssetID/Animations/Skeleton.ozz
                nlohmann::json data{ nlohmann::json::parse( configFile->GetContentsString().c_str() ) };
                data["skeleton"]["filename"] = string::Format( "{}/{}/skeleton.ozz", assetsCacheBasePath.GetC_Str(), animationsBasePath.GetC_Str() ).c_str();

                // Ensure animations directory exists
                (void)filesystem::CreateIfNotExistsDirectory( string::Format( "{}/{}", assetsCacheBasePath.GetC_Str(), animationsBasePath.GetC_Str() ) );

                // Set the path to the *.ozz animation files. HashedAssetID/Animations/*.ozz
                auto& animations{ data.at("animations") };
                for ( auto& animation : animations) {
                    animation["filename"] = string::Format( "{}/{}/*.ozz", assetsCacheBasePath.GetC_Str(), animationsBasePath.GetC_Str() ).c_str();
                }

                // Set config infor (formatted to be able
                // to read it properly while debugging)
                configJson = data.dump( 2 ).c_str();
            } catch (std::exception& exception) {
                MKT_CORE_LOGGER_ERROR( "Error skinned animation JSON parse: e.what() {}", exception.what() );
            }
        }

        const eastl::string configJsonParameter{ string::Format( R"(--config={})", configJson )  };
        const eastl::string filenameParameter{ string::Format( R"(--file={})", mFilename.GetC_Str() )  };

        eastl::array args{ "executable", filenameParameter.c_str(), loggingLevel.c_str(), configJsonParameter.c_str() };

        // if (animationsBasePath.IsDirectoryEmpty()) {
        //     (*mImporter)( args.size(), args.data() );
        // }

        auto exitResult{ (*mImporter)( args.size(), args.data() ) };
        if (exitResult == EXIT_FAILURE) {
            MKT_CORE_LOGGER_ERROR( "Failed attempt to load animations with importer" );
            return result;
        }

        ////////////////////////////////////////////////////////////////////////////
        // Skeleton
        ////////////////////////////////////////////////////////////////////////////
        auto skeletonPath{ PathBuilder{}
            .SetPath( currentAssetCachePath )
            .SetPath( "skeleton.ozz" )
            .Build() };

        // Now tries to open the file, which was provided as argument.
        // A file in ozz is a ozz::io::File, which implements ozz::io::Stream
        // interface and complies with std FILE specifications.
        // ozz::io::File follows RAII programming idiom, which ensures that the file
        // will always be closed (by ozz::io::FileStream destructor).
        ozz::io::File file( skeletonPath.GetC_Str(), "rb" );

        // Checks file status, which can be closed if filename is invalid.
        if ( !file.opened() ) {
            MKT_CORE_LOGGER_ERROR( "Cannot open file {}.", skeletonPath.GetC_Str() );
        }

        ////////////////////////////////////////////////////////////////////////////
        // The next section deserializes an object from the file.
        ////////////////////////////////////////////////////////////////////////////

        // Now the file is opened. we can actually read from it. This uses ozz
        // archive mechanism.
        // The first step is to instantiate an read-capable (ozz::io::IArchive)
        // archive object, in opposition to write-capable (ozz::io::OArchive)
        // archives.
        // Archives take as argument stream objects, which must be valid and opened.
        ozz::io::IArchive archive( &file );

        // Before actually reading the object from the file, we need to test that
        // the archive (at current seek position) contains the object type we
        // expect.
        // Archives uses a tagging system that allows to mark and detect thetype of
        // the next object to deserialize. Here we expect a skeleton, so we test for
        // a skeleton tag.
        // Tagging is not mandatory for all object types. It's usually only used for
        // high level object types (skeletons, animations...), but not low level
        // ones (math objects, native types...).
        if ( !archive.TestTag<ozz::animation::Skeleton>() ) {
            MKT_CORE_LOGGER_ERROR( "Archive doesn't contain the expected object type." );
            return result;
        }

        // Now the tag has been validated, the object can be read.
        // IArchive uses >> operator to read from the archive to the object.
        // Only objects that implement archive specifications can be used there,
        // along with all native types. Note that pointers aren't supported.
        ozz::animation::Skeleton skeleton{};
        archive >> skeleton;
        result->mSkeleton = eastl::make_unique<Skeleton>(ozz::make_unique<ozz::animation::Skeleton>( std::move( skeleton ) ) );

        ////////////////////////////////////////////////////////////////////////////
        // Animations
        ////////////////////////////////////////////////////////////////////////////
        const auto& animationNames{ mImporter->GetAnimationNames() };
        for (const auto& animationName : animationNames) {
            // Animators name their animations however they want, the importer saves them with using properly formated file name
            std::string animFileName{ mImporter->BuildFilename( "*.ozz", animationName.c_str() ) };

            Path animPath{ PathBuilder{}
                .SetPath( currentAssetCachePath )
                .SetPath( animFileName )
                .Build() };

            // Now tries to open the file, which was provided as argument.
            // A file in ozz is a ozz::io::File, which implements ozz::io::Stream
            // interface and complies with std FILE specifications.
            // ozz::io::File follows RAII programming idiom, which ensures that the file
            // will always be closed (by ozz::io::FileStream destructor).
            ozz::io::File ozzAnimationFile( animPath.GetC_Str(), "rb" );

            // Checks file status, which can be closed if filename is invalid.
            if ( !ozzAnimationFile.opened() ) {
                MKT_CORE_LOGGER_ERROR( "Cannot open file {}.", animPath.GetC_Str() );
                continue;
            }

            ////////////////////////////////////////////////////////////////////////////
            // The next section deserializes an object from the file.
            ////////////////////////////////////////////////////////////////////////////

            // Now the file is opened. we can actually read from it. This uses ozz
            // archive mechanism.
            // The first step is to instantiate an read-capable (ozz::io::IArchive)
            // archive object, in opposition to write-capable (ozz::io::OArchive)
            // archives.
            // Archives take as argument stream objects, which must be valid and opened.
            ozz::io::IArchive ozzAnimationArchive( &ozzAnimationFile );

            // Before actually reading the object from the file, we need to test that
            // the archive (at current seek position) contains the object type we
            // expect.
            // Archives uses a tagging system that allows to mark and detect the type of
            // the next object to deserialize. Here we expect a skeleton, so we test for
            // a skeleton tag.
            // Tagging is not mandatory for all object types. It's usually only used for
            // high level object types (skeletons, animations...), but not low level
            // ones (math objects, native types...).
            if ( !ozzAnimationArchive.TestTag<ozz::animation::Animation>() ) {
                MKT_CORE_LOGGER_ERROR( "Archive doesn't contain the expected object type." );
                continue;
            }

            // Now the tag has been validated, the object can be read.
            // IArchive uses >> operator to read from the archive to the object.
            // Only objects that implement archive specifications can be used there,
            // along with all native types. Note that pointers aren't supported.
            ozz::animation::Animation animation{};
            ozzAnimationArchive >> animation;

            result->mAnimations[animationName.c_str()] =
                eastl::make_unique<SkinnedAnimation>(
                    ozz::make_unique<ozz::animation::Animation>( std::move( animation ) ) );
        }

        return result;
    }
}
