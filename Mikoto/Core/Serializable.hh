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

#ifndef MIKOTO_SERIALIZABLE_HH
#define MIKOTO_SERIALIZABLE_HH

#include <Filesystem/Path.hh>

namespace mikoto::core {

    class ISerializable {
    public:
        virtual ~ISerializable() = default;

        virtual auto Serialize(const filesystem::Path& filename) -> void = 0;
        virtual auto Deserialize(const filesystem::Path& filename) -> void = 0;
    };
}

#endif//MIKOTO_SERIALIZABLE_HH
