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

#include <array>
#include <cstdlib>

#include "ozz/base/log.h"

// Provides files abstraction.
#include "ozz/base/io/stream.h"

// Provides serialization/deserialization mechanism.
#include "ozz/base/io/archive.h"

// Uses the skeleton as an example of object to read.
#include "ozz/animation/runtime/skeleton.h"

#include <cstdlib>

#include <Logging/Logger.hh>
#include <Animation/AnimationSystem.hh>

namespace Mikoto {
    int TestCode( int argc, char const* argv[] ) {
        ( void )argc;
        ( void )argv;

        // First check that an argument was provided. We expect it to be a valid
        // filename.
        if ( argc != 2 ) {
            ozz::log::Err() << "Invalid arguments." << std::endl;
            return EXIT_FAILURE;
        }
        // Stores filename.
        const char* filename = argv[1];

        {
            ////////////////////////////////////////////////////////////////////////////
            // The first section opens a file.
            ////////////////////////////////////////////////////////////////////////////

            // Now tries to open the file, which was provided as argument.
            // A file in ozz is a ozz::io::File, which implements ozz::io::Stream
            // interface and complies with std FILE specifications.
            // ozz::io::File follows RAII programming idiom, which ensures that the file
            // will always be closed (by ozz::io::FileStream destructor).
            ozz::io::File file( filename, "rb" );

            // Checks file status, which can be closed if filename is invalid.
            if ( !file.opened() ) {
                ozz::log::Err() << "Cannot open file " << filename << "." << std::endl;
                return EXIT_FAILURE;
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
                ozz::log::Err() << "Archive doesn't contain the expected object type."
                                << std::endl;
                return EXIT_FAILURE;
            }

            // Now the tag has been validated, the object can be read.
            // IArchive uses >> operator to read from the archive to the object.
            // Only objects that implement archive specifications can be used there,
            // along with all native types. Note that pointers aren't supported.
            ozz::animation::Skeleton skeleton;
            archive >> skeleton;

            // Getting out of this scope will destroy "file" object and close the system
            // file.
        }

        return EXIT_SUCCESS;
    }

    AnimationSystem::AnimationSystem( const AnimationSystemCreateInfo & ) {}

    auto AnimationSystem::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing AnimationSystem...");

        // Test code
        std::array<const char*, 2> paths{ "", ""};
        TestCode(paths.size(), paths.data());

        m_IsInitialized = true;
    }

    auto AnimationSystem::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down AnimationSystem..." );
    }

    auto AnimationSystem::Update( float dt ) -> void {
        // Update animations
    }

    auto AnimationSystem::RegisterAnimation() -> UInt64 {
        return 0;
    }
}
