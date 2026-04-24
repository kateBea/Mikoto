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

// #include <exception>
//
// #include <nlohmann/json.hpp>
//
// #include <EASTL/array.h>
//
// #include <ozz/base/io/stream.h>
// #include <ozz/base/io/archive.h>
// #include <ozz/animation/runtime/skeleton.h>
// #include <ozz/animation/runtime/animation.h>
//
// #include <Core/Core.hh>
// #include <Core/Types.hh>
// #include <Core/String.hh>
//
// #include <Logging/Logger.hh>
//
// #include <Filesystem/FileService.hh>
// #include <Filesystem/FileSystem.hh>
//
// #include <Assets/Importer.hh>
// #include <Assets/AssetsService.hh>
//
// #include <Animation/SkinningBuilder.hh>
//
// namespace mikoto::animation {
//
//     using namespace mikoto::core;
//     using namespace mikoto::asset;
//     using namespace mikoto::filesystem;
//
//     SkinningBuilder::SkinningBuilder( const Path& filename )
//         : mFilename{  filename }
//     {}
//
//     auto SkinningBuilder::Build(ozz::animation::offline::OzzImporter& importer, const Path& filepath) -> bool {
//         // Specify log level
// #if !defined(NDEBUG)
//         const eastl::string loggingLevel{ string::Format(R"(--log_level={})", "verbose") };
// #else
//         const eastl::string loggingLevel{ StringUtil::Format(R"(--log_level={})", "standard") };
// #endif
//
//         eastl::string configJSON{};
//
//         const eastl::string modelAnimationsPath{ string::Format(  "{}/Animations", GetHashedAssetID( filepath) ) };
//
//         const eastl::string& assetCacheBasePath{ AssetsService::Get()->GetAssetCacheBasePath() };
//         const eastl::string cachedAnimationsBasePath{ string::Format( "{}/{}", assetCacheBasePath, modelAnimationsPath ) };
//
//         // We first open the file that contains the base configuration for any animation
//         if (const File* configFile{ FileService::Get()->LoadFile( "Resources/AnimationConfiguration.json" ) }) {
//             try {
//                 nlohmann::json data{ nlohmann::json::parse( configFile->GetContentsString() ) };
//                 data["skeleton"]["filename"] = string::Format( "{}/{}/skeleton.ozz", assetCacheBasePath, modelAnimationsPath );
//
//                 // Ensure animations directory exists
//                 (void)filesystem::CreateIfNotExistsDirectory( string::Format( "{}/{}", assetCacheBasePath, modelAnimationsPath ) );
//
//                 auto& animations{ data.at("animations") };
//                 for ( auto& animation : animations) {
//                     animation["filename"] = string::Format( "{}/{}/*.ozz", assetCacheBasePath, modelAnimationsPath );
//                 }
//
//                 configJSON = data.dump( 2 );
//             } catch (std::exception& exception) {
//                 MKT_CORE_LOGGER_ERROR( "Error skinned animation JSON parse: e.what() {}", exception.what() );
//             }
//         }
//
//         const std::string configJSONParameter{ string::Format( R"(--config={})", configJSON )  };
//         const std::string filenameParameter{ string::Format( R"(--file={})", mFilename.string() )  };
//
//         eastl::array args{ "executable", filenameParameter.c_str(), loggingLevel.c_str(), configJSONParameter.c_str() };
//
//         // Import the model animations and skeleton
//         // TODO: only if the corresponding ozz files have not been created yet
//         importer( args.size(), args.data() );
//
//         // Skeleton
//         PathBuilder builderSkeletonPath{};
//
//         std::string skeletonPath{ builderSkeletonPath
//             .WithPath( cachedAnimationsBasePath )
//             .WithPath( "skeleton.ozz" )
//             .Build()
//             .string() };
//
//         // Now tries to open the file, which was provided as argument.
//         // A file in ozz is a ozz::io::File, which implements ozz::io::Stream
//         // interface and complies with std FILE specifications.
//         // ozz::io::File follows RAII programming idiom, which ensures that the file
//         // will always be closed (by ozz::io::FileStream destructor).
//         ozz::io::File file( skeletonPath.c_str(), "rb" );
//
//         // Checks file status, which can be closed if filename is invalid.
//         if ( !file.opened() ) {
//             MKT_CORE_LOGGER_ERROR( "Cannot open file {}.", skeletonPath );
//         }
//
//         ////////////////////////////////////////////////////////////////////////////
//         // The next section deserializes an object from the file.
//         ////////////////////////////////////////////////////////////////////////////
//
//         // Now the file is opened. we can actually read from it. This uses ozz
//         // archive mechanism.
//         // The first step is to instantiate an read-capable (ozz::io::IArchive)
//         // archive object, in opposition to write-capable (ozz::io::OArchive)
//         // archives.
//         // Archives take as argument stream objects, which must be valid and opened.
//         ozz::io::IArchive archive( &file );
//
//         // Before actually reading the object from the file, we need to test that
//         // the archive (at current seek position) contains the object type we
//         // expect.
//         // Archives uses a tagging system that allows to mark and detect thetype of
//         // the next object to deserialize. Here we expect a skeleton, so we test for
//         // a skeleton tag.
//         // Tagging is not mandatory for all object types. It's usually only used for
//         // high level object types (skeletons, animations...), but not low level
//         // ones (math objects, native types...).
//         if ( !archive.TestTag<ozz::animation::Skeleton>() ) {
//             MKT_CORE_LOGGER_ERROR( "Archive doesn't contain the expected object type." );
//             return false;
//         }
//
//         // Now the tag has been validated, the object can be read.
//         // IArchive uses >> operator to read from the archive to the object.
//         // Only objects that implement archive specifications can be used there,
//         // along with all native types. Note that pointers aren't supported.
//         ozz::animation::Skeleton skeleton{};
//         archive >> skeleton;
//
//         mSkeleton = std::move( skeleton );
//
//         // Animation
//         const auto& animationNames{ importer.GetAnimationNames() };
//         for (const auto& animationName : animationNames) {
//             // Animators name their animations however they want, the importer saves them with using properly formated file name
//             std::string animFileName{ importer.BuildFilename( "*.ozz", animationName.c_str() ) };
//
//             PathBuilder builder{};
//             std::string animPath{ builder
//                 .WithPath( cachedAnimationsBasePath )
//                 .WithPath( animFileName )
//                 .Build().string() };
//             // Now tries to open the file, which was provided as argument.
//             // A file in ozz is a ozz::io::File, which implements ozz::io::Stream
//             // interface and complies with std FILE specifications.
//             // ozz::io::File follows RAII programming idiom, which ensures that the file
//             // will always be closed (by ozz::io::FileStream destructor).
//             ozz::io::File ozzAnimationFile( animPath.c_str(), "rb" );
//
//             // Checks file status, which can be closed if filename is invalid.
//             if ( !ozzAnimationFile.opened() ) {
//                 MKT_CORE_LOGGER_ERROR( "Cannot open file {}.", animPath );
//                 continue;
//             }
//
//             ////////////////////////////////////////////////////////////////////////////
//             // The next section deserializes an object from the file.
//             ////////////////////////////////////////////////////////////////////////////
//
//             // Now the file is opened. we can actually read from it. This uses ozz
//             // archive mechanism.
//             // The first step is to instantiate an read-capable (ozz::io::IArchive)
//             // archive object, in opposition to write-capable (ozz::io::OArchive)
//             // archives.
//             // Archives take as argument stream objects, which must be valid and opened.
//             ozz::io::IArchive ozzAnimationArchive( &ozzAnimationFile );
//
//             // Before actually reading the object from the file, we need to test that
//             // the archive (at current seek position) contains the object type we
//             // expect.
//             // Archives uses a tagging system that allows to mark and detect thetype of
//             // the next object to deserialize. Here we expect a skeleton, so we test for
//             // a skeleton tag.
//             // Tagging is not mandatory for all object types. It's usually only used for
//             // high level object types (skeletons, animations...), but not low level
//             // ones (math objects, native types...).
//             if ( !ozzAnimationArchive.TestTag<ozz::animation::Animation>() ) {
//                 MKT_CORE_LOGGER_ERROR( "Archive doesn't contain the expected object type." );
//                 continue;
//             }
//
//             // Now the tag has been validated, the object can be read.
//             // IArchive uses >> operator to read from the archive to the object.
//             // Only objects that implement archive specifications can be used there,
//             // along with all native types. Note that pointers aren't supported.
//             ozz::animation::Animation animation{};
//             ozzAnimationArchive >> animation;
//
//             mAnimations.emplace_back( OzzAnimationInfo{
//                 .mName{ animationName },
//                 .mAnimation{ ozz::make_unique<ozz::animation::Animation>( std::move( animation ) ) }
//             } );
//         }
//
//         // Load the generated files and read skeleton and animations
//         return true;
//     }
//
//     auto SkinningBuilder::FillModelData( ModelData& data ) -> void {
//         // Skeleton
//         data.SceneSkeleton = Skeleton{ ozz::make_unique<ozz::animation::Skeleton>( std::move( mSkeleton ) ) };
//
//         for (auto& anim : mAnimations) {
//             data.Animations.try_emplace( anim.mName, std::move(anim.mAnimation) );
//         }
//     }
// }
