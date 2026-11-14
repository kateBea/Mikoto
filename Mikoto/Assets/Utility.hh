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

#ifndef MIKOTO_ASSETS_UTILITY_HH
#define MIKOTO_ASSETS_UTILITY_HH

#include <EASTL/functional.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

#include <Assets/Asset.hh>

namespace mikoto::asset {

    /**
     * @brief Configures the shared import/texture-compression CPU budget before its first use.
     * Zero leaves three logical processors available to the editor, with a minimum of one worker.
     * @param count Requested worker count; zero selects the default hardware budget.
     */
    auto SetImportWorkerCount( core::u32 count ) -> void;

    /**
     * @brief Queries the resolved import-worker budget.
     * @returns Configured worker count without starting the pool.
     */
    MKT_NODISCARD auto GetImportWorkerCount() -> core::u32;

    /**
     * @brief Processes independent indexed results and joins before returning.
     * Nested work executes inline: texture jobs may compress blocks without spawning another
     * pool or blocking all workers waiting for their own children. The caller owns all inputs
     * and preallocates result slots; jobs must not append to shared containers.
     * @param count Number of independent jobs, indexed from zero through count - 1.
     * @param job Callback for one index; inputs and result slots must remain valid until completion.
     */
    auto RunImportJobs( core::usize count, const eastl::function<void( core::usize )>& job ) -> void;

    /**
     * @brief Checks whether an extension or file type identifies a supported image.
     * @param path Source file path.
     * @returns True for a supported image extension or type.
     */
    MKT_NODISCARD auto IsFileImage( const filesystem::Path& path ) -> bool;

    /**
     * @brief Checks whether an extension or file type identifies a supported image.
     * @param type Filesystem file type to classify.
     * @returns True for a supported image extension or type.
     */
    MKT_NODISCARD auto IsFileImage( filesystem::FileType type ) -> bool;

    /**
     * @brief Checks whether an extension or file type identifies a supported model.
     * @param path Source file path.
     * @returns True for a supported model extension or type.
     */
    MKT_NODISCARD auto IsFileModel( const filesystem::Path& path ) -> bool;

    /**
     * @brief Checks whether an extension or file type identifies a supported model.
     * @param type Filesystem file type to classify.
     * @returns True for a supported model extension or type.
     */
    MKT_NODISCARD auto IsFileModel( filesystem::FileType type ) -> bool;

    /**
     * @brief Checks for Mikoto's scene-file extension.
     * @param path Source file path.
     * @returns True for a Mikoto scene path.
     */
    MKT_NODISCARD auto IsFileScene( const filesystem::Path& path ) -> bool;

    /**
     * @brief Checks for a supported audio-file extension.
     * @param path Source file path.
     * @returns True for a supported audio path.
     */
    MKT_NODISCARD auto IsFileAudio( const filesystem::Path& path ) -> bool;

    /**
     * @brief Checks for a supported font-file extension.
     * @param path Source file path.
     * @returns True for a supported font path.
     */
    MKT_NODISCARD auto IsFileFont( const filesystem::Path& path ) -> bool;

    /**
     * @brief Checks for Mikoto's material-file extension.
     * @param path Source file path.
     * @returns True for a material path.
     */
    MKT_NODISCARD auto IsFileMaterial( const filesystem::Path& path ) -> bool;

}// namespace mikoto::asset

#endif// MIKOTO_ASSETS_UTILITY_HH
