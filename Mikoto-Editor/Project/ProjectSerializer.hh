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

#ifndef MIKOTO_PROJECT_SERIALIZER_HH
#define MIKOTO_PROJECT_SERIALIZER_HH

#include <Project/Project.hh>
#include <Core/Serializer.hh>

namespace Mikoto {
    class ProjectSerializer final : public ISerializer<Project> {
    public:
        auto Serialize( const Project& obj, const Path& savePath ) -> void override;
        auto Deserialize( const Path& loadPath ) -> Unique<Project> override;
    };
}


#endif//MIKOTO_PROJECT_SERIALIZER_HH
