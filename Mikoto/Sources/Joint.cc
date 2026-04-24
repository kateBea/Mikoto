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

#include <string>

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Animation/Joint.hh>

namespace mikoto::animation {

    using namespace mikoto::core;

    Joint::Joint( const eastl::string& name, i32 ID )
        : mName{ name },
          mID{ ID }
    {
        MKT_ASSERT( mID != kInvalidJointID, "Not a valid Joint ID" );
    }

    auto Joint::SetParentID( i32 ID ) -> void {
        mParentID = ID;
    }

    auto Joint::GetID() const -> i32 {
        return mID;
    }

    auto Joint::GetParentID() const -> i32 {
        return mParentID;
    }

    auto Joint::GetBoneName() const -> const eastl::string & {
        return mName;
    }

    auto Joint::PrintBoneInfo() const -> void {
        MKT_COLOR_PRINT_FORMATTED_FLUSH(
                MKT_FMT_COLOR_BLUE_VIOLET,
                "Printing joint vertex contribution\n" );

        for ( const auto& [meshName, contributionMap]: mVertexWeights ) {
            // Because console output is limited, also log to file
            MKT_FILE_LOGGER_DEBUG( "Joint [{}] contribution to mesh [{}]", mName, meshName );
            MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_YELLOW, "Joint [{}] contribution to mesh [{}]\n", mName, meshName );

            for (const auto& [vertex, weight] : contributionMap) {
                MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_YELLOW, "\tVertex [{}] Weight [{}]\n", vertex, weight );
                MKT_FILE_LOGGER_DEBUG( "\tVertex [{}] Weight [{}]", vertex, weight );
            }
        }
    }

    auto Joint::SetWeights( eastl::string_view meshName, u64 vertex, float weight ) -> void {
        mVertexWeights[std::string{ meshName.data() }][vertex] = weight;
    }
}