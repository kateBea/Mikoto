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

#ifndef MIKOTO_FILESYSTEM_FILE_SYSTEM_HH
#define MIKOTO_FILESYSTEM_FILE_SYSTEM_HH

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

namespace mikoto::filesystem {

    inline constexpr eastl::string_view kMikotoSceneExtension{ "mktscn" };
    inline constexpr eastl::string_view kMikotoProjectExtension{ "mktprj" };
    inline constexpr eastl::string_view kMikotoMaterialExtension{ "mktmtl" };
    inline constexpr eastl::string_view kMikotoParticleExtension{ "mktpart" };

    enum class FileType {
        eInvalid = -1,

        // Application
        eExe,

        // --- Common images ---
        eBmp,
        eIco,
        eJpeg,
        eJpg,
        eJng,
        ePng,
        eTarga,
        eTiff,
        eGif,
        ePsd,
        eHdr,
        eExr,
        eWebp,
        eJxr,

        // --- Portable / Netpbm ---
        ePbm,
        ePbmRaw,
        ePgm,
        ePgmRaw,
        ePpm,
        ePpmRaw,

        // --- Other image formats ---
        eKoala,
        eIff,
        eMng,
        ePcd,
        ePcx,
        eRas,
        eWbmp,
        eCut,
        eXbm,
        eXpm,
        eFaxG3,
        eSgi,
        eJ2k,
        eJp2,
        ePfm,
        ePict,
        eRaw,

        // --- GPU / engine-relevant ---
        eDds,
        eKtx,

        // --- Media ---
        eMp4,
        eMp3,
        eWav,

        // Font
        eTtf,
        eOtf,
        eWoff,

        // Shader
        eSlang,
        eSprv,
        eGlsl,
        eHlsl,

        eFrag,
        eVert,

        // Data transfer format
        eJson,
        eXml,

        // Config files
        eIni,
        eToml,
        eCmake,

        // Github
        eGitignore,

        // 3D Models
        eGltf,
        eGlb,
        eFbx,
        eObj,
        eBlend,
        eDae,

        // Text
        eMarkdown,

        // Code
        eC_Source,
        eC_Header,
        eCpp_Source,
        eCpp_Header,
        ePython,
        eLua,
        eCSharp,

        // Mikoto formats
        eMikoto_Scene,
        eMikoto_Project,
        eMikoto_Material,
        eMikoto_Particle,

        eKtx2,
        eJavaScript,
        ePdf,
        ePlainText,
        eSubtitle,
        eCSharpProject,
    };

    auto OpenInExplorer( const Path& path ) -> void;

    // Asks the desktop to open a UTF-8 URL with its registered handler.
    auto OpenInBrowser( eastl::string_view url ) -> void;

    // An empty editor path selects VS Code; "Visual Studio" selects the Windows
    // solution launcher; otherwise use the chosen executable.
    MKT_NODISCARD auto OpenInTextEditor( const Path& path, const Path& editor = {} ) -> bool;

    MKT_NODISCARD auto IsRegularFile(const Path& path) -> bool;
    MKT_NODISCARD auto IsDirectory(const Path& path) -> bool;

    // Process current working directory (absolute)
    MKT_NODISCARD auto GetProcessPath() -> Path;

    MKT_NODISCARD auto StripFileName(eastl::string_view path) -> eastl::string;

    MKT_NODISCARD auto GetGetAbsolutePath( std::string_view path ) -> Path;
    MKT_NODISCARD auto GetGetAbsolutePath( const Path& path ) -> Path;
    MKT_NODISCARD auto GetGetAbsolutePathString( std::string_view path ) -> std::string;
    MKT_NODISCARD auto GetGetAbsolutePathString( const Path& path ) -> std::string;

    // Creates parent directories if they do not exist
    MKT_NODISCARD auto CreateIfNotExistsDirectory(const Path& path) -> bool;

    MKT_NODISCARD auto GetFileTypeName(FileType type) -> eastl::string_view;
    MKT_NODISCARD auto GetFileTypeDisplayName(FileType type) -> eastl::string_view;
    MKT_NODISCARD auto InferFileTypeFromExtension(eastl::string_view extension) -> FileType;

    MKT_NODISCARD auto IsAbsolutePath(const Path& path) -> bool;
    MKT_NODISCARD auto IsRelativePath(const Path& path) -> bool;

    MKT_NODISCARD auto IsAudioFile( const std::filesystem::path& path ) -> bool;
    MKT_NODISCARD auto IsFontFile( const std::filesystem::path& path ) -> bool;
    MKT_NODISCARD auto IsVideoFile( const std::filesystem::path& path ) -> bool;

    struct FileDialogPair {
        eastl::string mDescription{};
        eastl::string mPattern{};
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

    MKT_NODISCARD auto OpenFolderDialog() -> Path;

    MKT_NODISCARD auto OpenFileDialog( std::initializer_list<FileDialogPair> filters ) -> Path;
    MKT_NODISCARD auto SaveFileDialog( eastl::string_view defaultName, std::initializer_list<FileDialogPair> filters ) -> Path;

    // 1s timeout default, hangs calling thread on the popup for timeout milliseconds or until OK button is clicked
    auto DisplayPopUp( eastl::string_view title, eastl::string_view message, PopUpChoice choice, PopUpIcon icon, core::i32 timeOut = 1000 ) -> void;
}

#endif// MIKOTO_FILESYSTEM_FILE_SYSTEM_HH
