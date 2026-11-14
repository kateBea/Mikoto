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

#ifndef MIKOTO_PACKAGE_GAME_HH
#define MIKOTO_PACKAGE_GAME_HH

#include <filesystem>

#include <Project/Project.hh>

namespace mikoto::runtime {

    /**
     * @brief Validates and stages a standalone game in a new directory.
     * @details Keeps the project layout under Game/, stages engine Resources and
     * native libraries, rewrites scene asset references and writes game.yaml.
     * The destination must not exist or be inside the project/resources tree.
     * Throws on unsupported external assets, symbolic links or filesystem errors.
     */
    auto PackageGame(
        const project::Project& project,
        const std::filesystem::path& executable,
        const std::filesystem::path& resources,
        const std::filesystem::path& destination ) -> void;
}// namespace mikoto::runtime

#endif// MIKOTO_PACKAGE_GAME_HH
