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

#ifndef MIKOTO_FILESYSTEM_HH
#define MIKOTO_FILESYSTEM_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

namespace mikoto::filesystem {

    auto OpenInExplorer( const Path& path ) -> void;

    MKT_NODISCARD auto IsRegularFile(const Path& path) -> bool;
    MKT_NODISCARD auto IsDirectory(const Path& path) -> bool;

    // Process current working directory
    MKT_NODISCARD auto GetProcessPath() -> Path;

    MKT_NODISCARD auto StripFileName(eastl::string_view path) -> eastl::string;

    MKT_NODISCARD auto GetGetAbsolutePath(std::string_view path) -> Path;
    MKT_NODISCARD auto GetGetAbsolutePath(const Path& path) -> Path;
    MKT_NODISCARD auto GetGetAbsolutePathString(std::string_view path) -> std::string;
    MKT_NODISCARD auto GetGetAbsolutePathString(const Path& path) -> std::string;

    // Creates parent directories if they do not exist
    MKT_NODISCARD auto CreateIfNotExistsDirectory(const Path& path) -> bool;

    MKT_NODISCARD auto IsAbsolutePath(const Path& path) -> bool;
    MKT_NODISCARD auto IsRelativePath(const Path& path) -> bool;

    struct FileDialogPair {
        eastl::string mDescription{};
        eastl::string mFilePattern{};
    };

    enum class PopUpChoice {
        eOk = 0,
        eOkCancel,
        eYesNo,
        eYesNoCancel,
        eRetryCancel,
        eAbortRetryIgnore,
    };

    enum class PopUpIcon {
        eInfo = 0,
        eWarning,
        eError,
        eQuestion,
    };

    MKT_NODISCARD auto SaveFileDialog( eastl::string_view fileName,  std::initializer_list<FileDialogPair> filters ) -> Path;

    MKT_NODISCARD auto OpenFolderDialog() -> eastl::string;
    MKT_NODISCARD auto OpenFileDialog( const FileDialogPair &filter ) -> eastl::string;
    MKT_NODISCARD auto OpenFileDialog( std::initializer_list<FileDialogPair> filters ) -> eastl::string;

    // 1s timeout default, hangs calling thread on the popup for timeout milliseconds or until OK button is clicked
    auto DisplayPopUp( eastl::string_view title, eastl::string_view message, PopUpChoice choice, PopUpIcon icon, core::i32 timeOut = 1000 ) -> void;
}

#endif //MIKOTO_FILESYSTEM_HH