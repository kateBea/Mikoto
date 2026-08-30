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

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <entt/entt.hpp>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>
#include <Scene/SceneSerializer.hh>

namespace mikoto::scene {

    auto SceneSerializer::Serialize( const ISerializable* serializable, const Path& saveFilePath ) -> void {
        FileHandle outputFile{ FileService::Get()->CreateNewFile( saveFilePath ) };
        if ( outputFile.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Could not create or open file '{}' required for scene serialization.", saveFilePath.GetC_Str() );
            return;
        }

        serializable->Serialize( outputFile );
    }

    auto SceneSerializer::Deserialize( const Path& saveFilePath ) -> Ref<ISerializable> {
        FileHandle inputFile{ FileService::Get()->LoadFile( saveFilePath ) };

        if ( inputFile.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Could not open file '{}' required for scene serialization", saveFilePath.GetC_Str() );
            return Ref<ISerializable>::CreateEmpty();
        }

        Ref<ISerializable> result{ Ref<Scene>::Spawn() };
        result->Deserialize( inputFile );

        return result;
    }
}// namespace Mikoto
