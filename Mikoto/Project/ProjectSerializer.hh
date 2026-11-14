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

#ifndef MIKOTO_PROJECT_PROJECT_SERIALIZER_HH
#define MIKOTO_PROJECT_PROJECT_SERIALIZER_HH

#include <Core/Core.hh>
#include <Core/Serializer.hh>

#include <Filesystem/Path.hh>

namespace mikoto::project {

    /** @brief Saves serializable project data and loads project instances from disk. */
    class ProjectSerializer final : public core::ISerializer {
    public:
        /**
         * @brief Writes the supplied project's serializable state to disk.
         * @param object Object whose state is saved; must not be null.
         * @param savePath Destination project file; must not be empty.
         * @throws core::RuntimeException If no object or destination is supplied.
         */
        auto Serialize(const core::ISerializable* object, const filesystem::Path& savePath) -> void override;

        /**
         * @brief Creates a project and restores its state from disk.
         * @param loadPath Project file to read.
         * @returns Loaded project, or an empty reference when loadPath is empty.
         * @throws std::exception If loading or decoding the project fails.
         */
        MKT_NODISCARD auto Deserialize(const filesystem::Path& loadPath) -> core::Ref<core::ISerializable> override;

        ~ProjectSerializer() override = default;
    };

}// namespace mikoto::project

#endif// MIKOTO_PROJECT_PROJECT_SERIALIZER_HH
