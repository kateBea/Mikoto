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

#include <Common/String.hh>

#include <Animation/SkinningBuilder.hh>

#include "Logging/Assert.hh"
#include "Memory/Allocator.hh"

namespace Mikoto {

    SkinningBuilder::SkinningBuilder( const Path& filename )
        : m_Filename{  filename }
    {}

    auto SkinningBuilder::Build(ozz::animation::offline::OzzImporter& importer) -> bool {
        const std::string filename{ StringUtil::Format( R"(--file="{}")", m_Filename.string() )  };
        std::array args{ "executable", filename.c_str() };

        // Before we do this we check first if the animation have been loaded previously
        importer( args.size(), args.data() );


        // Load the generated files and read skeleton and animations
        return true;
    }

    auto SkinningBuilder::GetAnimations() -> AnimationList & {
        return m_Animations;
    }

}// namespace Mikoto
