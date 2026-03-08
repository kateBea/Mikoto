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


#include <array>

#include "ozz/base/log.h"

// Provides files abstraction.
#include "ozz/base/io/stream.h"

// Provides serialization/deserialization mechanism.
#include "ozz/base/io/archive.h"

// Uses the skeleton as an example of object to read.
#include "ozz/animation/runtime/skeleton.h"

#include <cstdlib>

#include <Common/String.hh>

#include <Logging/Logger.hh>

#include <Assets/Importer.hh>

#include <Filesystem/FileSystem.hh>
#include <Animation/SkinningBuilder.hh>

#include "Logging/Assert.hh"
#include "Memory/Allocator.hh"

namespace Mikoto {

    SkinningBuilder::SkinningBuilder( const Path& filename )
        : m_Filename{  filename }
    {}

    auto SkinningBuilder::Build(ozz::animation::offline::OzzImporter& importer) -> bool {
        const std::string filename{ StringUtil::Format( R"(--file={})", m_Filename.string() )  };
        std::array args{ "executable", filename.c_str() };

        // Before we do this we check first if the animation have been loaded previously
        importer( args.size(), args.data() );

        // Skeleton
        PathBuilder builderSkeletonPath{};

        std::string skeletonPath{ builderSkeletonPath
            .WithPath( Filesystem::GetProcessPath().string() )
            .WithPath( "skeleton.ozz" )
            .Build()
            .string() };

        // Now tries to open the file, which was provided as argument.
        // A file in ozz is a ozz::io::File, which implements ozz::io::Stream
        // interface and complies with std FILE specifications.
        // ozz::io::File follows RAII programming idiom, which ensures that the file
        // will always be closed (by ozz::io::FileStream destructor).
        ozz::io::File file( skeletonPath.c_str(), "rb" );

        // Checks file status, which can be closed if filename is invalid.
        if ( !file.opened() ) {
            MKT_CORE_LOGGER_ERROR( "Cannot open file {}.", skeletonPath );
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
            return false;
        }

        // Now the tag has been validated, the object can be read.
        // IArchive uses >> operator to read from the archive to the object.
        // Only objects that implement archive specifications can be used there,
        // along with all native types. Note that pointers aren't supported.
        ozz::animation::Skeleton skeleton{};
        archive >> skeleton;

        m_Skeleton = std::move( skeleton );

        // Animation
        const auto& animationNames{ importer.GetAnimationNames() };
        for (const auto& animationName : animationNames) {
            // Animators name their animations howver they want, the importer saves them with using properly formated file name
            std::string animFileName{ importer.BuildFilename( "*.ozz", animationName.c_str() ) };

            PathBuilder builder{};
            std::string animPath{ builder
                .WithPath( Filesystem::GetProcessPath().string() )
                .WithPath( animFileName )
                .Build().string() };
            // Now tries to open the file, which was provided as argument.
            // A file in ozz is a ozz::io::File, which implements ozz::io::Stream
            // interface and complies with std FILE specifications.
            // ozz::io::File follows RAII programming idiom, which ensures that the file
            // will always be closed (by ozz::io::FileStream destructor).
            ozz::io::File file( animPath.c_str(), "rb" );

            // Checks file status, which can be closed if filename is invalid.
            if ( !file.opened() ) {
                MKT_CORE_LOGGER_ERROR( "Cannot open file {}.", animPath );
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
            if ( !archive.TestTag<ozz::animation::Animation>() ) {
                MKT_CORE_LOGGER_ERROR( "Archive doesn't contain the expected object type." );
                continue;
            }

            // Now the tag has been validated, the object can be read.
            // IArchive uses >> operator to read from the archive to the object.
            // Only objects that implement archive specifications can be used there,
            // along with all native types. Note that pointers aren't supported.
            ozz::animation::Animation animation{};
            archive >> animation;

            m_Animations.emplace_back( OzzAnimationInfo{ 
                .Name{ animationName }, 
                .Animation{ ozz::make_unique<ozz::animation::Animation>( std::move( animation ) ) }
            } );
        }

        // Load the generated files and read skeleton and animations
        return true;
    }

    auto SkinningBuilder::FillModelData( ModelData& data ) -> void {
        // Skeleton
        data.SceneSkeleton = Skeleton{ ozz::make_unique<ozz::animation::Skeleton>( std::move( m_Skeleton ) ) };

        for (auto& anim : m_Animations) {
            data.Animations.try_emplace( anim.Name, std::move(anim.Animation) );
        }
    }
}
