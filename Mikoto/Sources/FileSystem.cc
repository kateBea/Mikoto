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

#include <filesystem>

#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#include <nfd.hpp>

#include <portable-file-dialogs.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Platform.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Platform/PlatformWin32.hh>
#include <shlobj.h>
#include <windows.h>
#endif

namespace mikoto::filesystem {

    using namespace mikoto::core;

#if defined( MIKOTO_PLATFORM_WINDOWS )
    auto OpenAndSelectFile( const eastl::wstring &filePath ) -> void {
        PIDLIST_ABSOLUTE pidl{ ILCreateFromPathW( filePath.c_str() ) };
        if ( pidl ) {
            SHOpenFolderAndSelectItems( pidl, 0, nullptr, 0 );
            ILFree( pidl );
        }
    }
#endif

    auto GetProcessPath() -> Path {
        return Path{ std::filesystem::current_path() };
    }

    auto GetGetAbsolutePath( std::string_view path ) -> Path {
        Path absolutePath{ std::filesystem::absolute( path ) };
        return absolutePath;
    }

    auto CreateIfNotExistsDirectory( const Path &path ) -> bool {
        std::error_code ec{};
        const bool created{ std::filesystem::create_directories( path.GetPathTyped<std::string>(), ec ) };

        if ( ec ) {
            return false;
        }

        return created;
    }

    auto GetFileTypeName( FileType type ) -> eastl::string_view {
        switch ( type ) {
            case FileType::eInvalid: return "File";

            case FileType::eBmp:   return "BMP";
            case FileType::eIco:   return "ICO";
            case FileType::eJpeg:  return "JPEG";
            case FileType::eJpg:   return "JPG";
            case FileType::eJng:   return "JNG";
            case FileType::ePng:   return "PNG";
            case FileType::eTarga: return "Targa";
            case FileType::eTiff:  return "TIFF";
            case FileType::eGif:   return "GIF";
            case FileType::ePsd:   return "PSD";
            case FileType::eHdr:   return "HDR";
            case FileType::eExr:   return "EXR";
            case FileType::eWebp:  return "WebP";
            case FileType::eJxr:   return "JXR";

            case FileType::ePbm:    return "PBM";
            case FileType::ePbmRaw: return "PBM Raw";
            case FileType::ePgm:    return "PGM";
            case FileType::ePgmRaw: return "PGM Raw";
            case FileType::ePpm:    return "PPM";
            case FileType::ePpmRaw: return "PPM Raw";

            case FileType::eKoala: return "Koala";
            case FileType::eIff:   return "IFF";
            case FileType::eMng:   return "MNG";
            case FileType::ePcd:   return "PCD";
            case FileType::ePcx:   return "PCX";
            case FileType::eRas:   return "RAS";
            case FileType::eWbmp:  return "WBMP";
            case FileType::eCut:   return "CUT";
            case FileType::eXbm:   return "XBM";
            case FileType::eXpm:   return "XPM";
            case FileType::eFaxG3: return "FaxG3";
            case FileType::eSgi:   return "SGI";
            case FileType::eJ2k:   return "J2K";
            case FileType::eJp2:   return "JP2";
            case FileType::ePfm:   return "PFM";
            case FileType::ePict:  return "PICT";
            case FileType::eRaw:   return "RAW";

            case FileType::eDds: return "DDS";
            case FileType::eKtx: return "KTX";

            case FileType::eMp4: return "MP4";
            case FileType::eMp3: return "MP3";
            case FileType::eWav: return "WAV";

            case FileType::eGltf:  return "glTF";
            case FileType::eGlb:   return "GLB";
            case FileType::eFbx:   return "FBX";
            case FileType::eObj:   return "OBJ";
            case FileType::eBlend: return "Blend";
            case FileType::eDae:   return "Collada";

            case FileType::eMikoto_Scene:    return "MikotoScene";
            case FileType::eMikoto_Project:  return "MikotoProject";
            case FileType::eMikoto_Material: return "MikotoMaterial";

            case FileType::eSlang: return "SLANG";
            case FileType::eSprv:  return "SPIR-V";
            case FileType::eGlsl:  return "GLSL";
            case FileType::eHlsl:  return "HLSL";
            case FileType::eFrag:  return "FRAG";
            case FileType::eVert:  return "VERT";

            case FileType::eJson:  return "JSON";
            case FileType::eXml:   return "XML";

            case FileType::eIni:   return "INI";
            case FileType::eToml:  return "TOML";
            case FileType::eCmake: return "CMake";

            case FileType::eGitignore: return "Gitignore";

            case FileType::eMarkdown:  return "Markdown";

            default: return "File";
        }
    }

    auto GetFileTypeDisplayName( FileType type ) -> eastl::string_view {
        switch ( type ) {
            case FileType::eInvalid: return "File";

            case FileType::eBmp:   return "BMP Image";
            case FileType::eIco:   return "Icon Resource";
            case FileType::eJpeg:  return "JPEG Image";
            case FileType::eJpg:   return "JPG Image";
            case FileType::eJng:   return "JNG Image";
            case FileType::ePng:   return "PNG Image";
            case FileType::eTarga: return "Targa Image";
            case FileType::eTiff:  return "TIFF Image";
            case FileType::eGif:   return "GIF Animation";
            case FileType::ePsd:   return "Photoshop Document";
            case FileType::eHdr:   return "HDR Environment Map";
            case FileType::eExr:   return "OpenEXR Image";
            case FileType::eWebp:  return "WebP Image";
            case FileType::eJxr:   return "JPEG XR Image";

            case FileType::ePbm:    return "PBM Portable Bitmap";
            case FileType::ePbmRaw: return "Raw PBM Bitmap";
            case FileType::ePgm:    return "PGM Portable Graymap";
            case FileType::ePgmRaw: return "Raw PGM Graymap";
            case FileType::ePpm:    return "PPM Portable Pixmap";
            case FileType::ePpmRaw: return "Raw PPM Pixmap";

            case FileType::eKoala: return "Koala Image";
            case FileType::eIff:   return "IFF Interchange Format";
            case FileType::eMng:   return "Multiple-image Network Graphic";
            case FileType::ePcd:   return "Kodak Photo CD";
            case FileType::ePcx:   return "ZSoft PCX Image";
            case FileType::eRas:   return "Sun Raster Image";
            case FileType::eWbmp:  return "Wireless Bitmap";
            case FileType::eCut:   return "Dr. Halo CUT Image";
            case FileType::eXbm:   return "X BitMap";
            case FileType::eXpm:   return "X PixMap";
            case FileType::eFaxG3: return "Raw Fax G3 Document";
            case FileType::eSgi:   return "Silicon Graphics Image";
            case FileType::eJ2k:   return "JPEG 2000 Codestream";
            case FileType::eJp2:   return "JPEG 2000 Image";
            case FileType::ePfm:   return "Portable Float Map";
            case FileType::ePict:  return "Apple Macintosh PICT Image";
            case FileType::eRaw:   return "Camera RAW Image";

            case FileType::eDds: return "DirectDraw Surface Texture";
            case FileType::eKtx: return "Khronos Texture Container";

            case FileType::eMp4: return "MP4 Video Media";
            case FileType::eMp3: return "MP3 Audio Track";
            case FileType::eWav: return "WAVE Audio Waveform";

            case FileType::eGltf:  return "glTF Scene";
            case FileType::eGlb:   return "GLB Binary";
            case FileType::eFbx:   return "FBX Scene";
            case FileType::eObj:   return "Wavefront OBJ Model";
            case FileType::eBlend: return "Blender Project File";
            case FileType::eDae:   return "Collada File";

            case FileType::eMikoto_Scene:    return "Scene";
            case FileType::eMikoto_Project:  return "Project";
            case FileType::eMikoto_Material: return "Material";

            case FileType::eSlang: return "Slang Shader Source";
            case FileType::eSprv:  return "SPIR-V Compiled Shader";
            case FileType::eGlsl:  return "GLSL Shader Source";
            case FileType::eHlsl:  return "HLSL Shader Source";
            case FileType::eFrag:  return "Fragment Shader Stage";
            case FileType::eVert:  return "Vertex Shader Stage";

            case FileType::eJson:  return "JSON File";
            case FileType::eXml:   return "XML Document";

            case FileType::eIni:   return "INI Configuration File";
            case FileType::eToml:  return "TOML Configuration File";
            case FileType::eCmake: return "CMake Build Script";

            case FileType::eGitignore: return "Git Ignore File";

            case FileType::eMarkdown:  return "Markdown Documentation";

            default: return "File";
        }
    }

    auto InferFileTypeFromExtension( eastl::string_view extension ) -> FileType {
        if ( extension.empty() ) {
            return FileType::eInvalid;
        }

        if ( extension.front() == '.' ) {
            extension.remove_prefix( 1 );
        }

        if ( extension == "bmp" )  return FileType::eBmp;
        if ( extension == "ico" )  return FileType::eIco;
        if ( extension == "jpeg" ) return FileType::eJpeg;
        if ( extension == "jpg" )  return FileType::eJpg;
        if ( extension == "jng" )  return FileType::eJng;
        if ( extension == "png" )  return FileType::ePng;
        if ( extension == "tga" || extension == "targa" ) return FileType::eTarga;
        if ( extension == "tiff" || extension == "tif" )   return FileType::eTiff;
        if ( extension == "gif" )  return FileType::eGif;
        if ( extension == "psd" )  return FileType::ePsd;
        if ( extension == "hdr" )  return FileType::eHdr;
        if ( extension == "exr" )  return FileType::eExr;
        if ( extension == "webp" ) return FileType::eWebp;
        if ( extension == "jxr" )  return FileType::eJxr;

        if ( extension == "pbm" )    return FileType::ePbm;
        if ( extension == "pbmraw" ) return FileType::ePbmRaw;
        if ( extension == "pgm" )    return FileType::ePgm;
        if ( extension == "pgmraw" ) return FileType::ePgmRaw;
        if ( extension == "ppm" )    return FileType::ePpm;
        if ( extension == "ppmraw" ) return FileType::ePpmRaw;

        if ( extension == "koa" || extension == "koala" ) return FileType::eKoala;
        if ( extension == "iff" || extension == "lbm" )   return FileType::eIff;
        if ( extension == "mng" )  return FileType::eMng;
        if ( extension == "pcd" )  return FileType::ePcd;
        if ( extension == "pcx" )  return FileType::ePcx;
        if ( extension == "ras" )  return FileType::eRas;
        if ( extension == "wbmp" ) return FileType::eWbmp;
        if ( extension == "cut" )  return FileType::eCut;
        if ( extension == "xbm" )  return FileType::eXbm;
        if ( extension == "xpm" )  return FileType::eXpm;
        if ( extension == "g3" || extension == "fax" ) return FileType::eFaxG3;
        if ( extension == "sgi" || extension == "rgb" ) return FileType::eSgi;
        if ( extension == "j2k" || extension == "j2c" ) return FileType::eJ2k;
        if ( extension == "jp2" )  return FileType::eJp2;
        if ( extension == "pfm" )  return FileType::ePfm;
        if ( extension == "pct" || extension == "pict" ) return FileType::ePict;
        if ( extension == "raw" || extension == "cr2" || extension == "nef" ) return FileType::eRaw;

        if ( extension == "dds" )  return FileType::eDds;
        if ( extension == "ktx" )  return FileType::eKtx;

        if ( extension == "mp4" || extension == "m4v" ) return FileType::eMp4;
        if ( extension == "mp3" )  return FileType::eMp3;
        if ( extension == "wav" )  return FileType::eWav;

        if ( extension == "slang" )             return FileType::eSlang;
        if ( extension == "spv" || extension == "spirv" ) return FileType::eSprv;
        if ( extension == "glsl" )              return FileType::eGlsl;
        if ( extension == "hlsl" )              return FileType::eHlsl;
        if ( extension == "frag" )              return FileType::eFrag;
        if ( extension == "vert" )              return FileType::eVert;

        if ( extension == "gltf" )  return FileType::eGltf;
        if ( extension == "glb" )   return FileType::eGlb;
        if ( extension == "fbx" )   return FileType::eFbx;
        if ( extension == "obj" )   return FileType::eObj;
        if ( extension == "blend" ) return FileType::eBlend;
        if ( extension == "dae" )   return FileType::eDae;

        if ( extension == "json" )  return FileType::eJson;
        if ( extension == "xml" )   return FileType::eXml;

        if ( extension == "ini" )               return FileType::eIni;
        if ( extension == "toml" )              return FileType::eToml;

        if ( extension == "md" || extension == "markdown" ) return FileType::eMarkdown;

        if ( extension == kMikotoSceneExtension ) return FileType::eMikoto_Scene;
        if ( extension == kMikotoProjectExtension ) return FileType::eMikoto_Project;
        if ( extension == kMikotoMaterialExtension ) return FileType::eMikoto_Material;

        return FileType::eInvalid;
    }


    auto DisplayPopUp( eastl::string_view title, eastl::string_view message, PopUpChoice choice, PopUpIcon icon, core::i32 timeOut ) -> void {
        constexpr auto kChoiceConvert{
            []( PopUpChoice choice ) -> pfd::choice {
                switch ( choice ) {
                    case PopUpChoice::eOk:
                        return pfd::choice::ok;
                    case PopUpChoice::eOkCancel:
                        return pfd::choice::ok_cancel;
                    case PopUpChoice::eYesNo:
                        return pfd::choice::yes_no;
                    case PopUpChoice::eYesNoCancel:
                        return pfd::choice::yes_no_cancel;
                    case PopUpChoice::eRetryCancel:
                        return pfd::choice::retry_cancel;
                    case PopUpChoice::eAbortRetryIgnore:
                        return pfd::choice::abort_retry_ignore;
                }

                return pfd::choice::ok;
            }
        };

        constexpr auto kIconConvert{
            []( PopUpIcon choice ) -> pfd::icon {
                switch ( choice ) {
                    case PopUpIcon::eError:
                        return pfd::icon::error;
                    case PopUpIcon::eInfo:
                        return pfd::icon::info;
                    case PopUpIcon::eQuestion:
                        return pfd::icon::question;
                    case PopUpIcon::eWarning:
                        return pfd::icon::warning;
                }

                return pfd::icon::info;
            }
        };

        auto m{ pfd::message( title.data(),
                              message.data(),
                              kChoiceConvert( choice ),
                              kIconConvert( icon ) ) };

        ( void )m.ready( timeOut );
    }

    auto OpenFolderDialog() -> Path {
        NFD::Guard nfdGuard{};
        NFD::UniquePath outPath{};

        nfdresult_t result{ NFD::PickFolder( outPath ) };
        if ( result == NFD_OKAY ) {
            MKT_CORE_LOGGER_INFO( "Success on save folder dialog: {}", outPath.get() );
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "User canceled open folder dialog." );
        } else {
            MKT_CORE_LOGGER_ERROR( "Error open folder dialog {}", NFD::GetError() );
        }

        return Path{ outPath.get() };
    }

    auto OpenFileDialog( std::initializer_list<FileDialogPair> filters ) -> Path {
        constexpr usize kDefaultFilterLimit{ 10 };
        eastl::fixed_vector<nfdfilteritem_t, kDefaultFilterLimit> filterItems{};
        for ( const auto& [filterName, filterExtensions]: filters ) {
            filterItems.emplace_back( nfdfilteritem_t{ filterName.data(), filterExtensions.data() } );
        }

        NFD::Guard nfdGuard{};
        NFD::UniquePath outPath{};

        const Path currentWorkingDir{ GetProcessPath() };
        const nfdresult_t result{ NFD::OpenDialog( outPath,
            filterItems.data(),
            filterItems.size(),
            currentWorkingDir.GetC_Str() ) };

        if ( result == NFD_OKAY ) {
            MKT_CORE_LOGGER_INFO( "Success on open file dialog: {}", outPath.get() );
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "User canceled open file dialog" );
        } else {
            MKT_CORE_LOGGER_ERROR( "Error open file dialog: {}", NFD::GetError() );
        }

        return Path{ outPath.get() };
    }

    auto SaveFileDialog( eastl::string_view defaultName, std::initializer_list<FileDialogPair> filters ) -> Path {
        constexpr usize kDefaultFilterLimit{ 10 };
        eastl::fixed_vector<nfdfilteritem_t, kDefaultFilterLimit> filterItems{};
        for ( const auto& [filterName, filterExtensions]: filters ) {
            filterItems.emplace_back( nfdfilteritem_t{ filterName.data(), filterExtensions.data() } );
        }

        NFD::Guard nfdGuard{};
        NFD::UniquePath outPath{};

        const nfdresult_t result{ NFD::SaveDialog(
            outPath,
            filterItems.data(),
            filterItems.size(),
            nullptr,
            defaultName.data() ) };

        if ( result == NFD_OKAY ) {
            MKT_CORE_LOGGER_INFO( "Success on save file dialog: {}", outPath.get() );
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "User canceled save file dialog" );
        } else {
            MKT_CORE_LOGGER_ERROR( "Error save file dialog: {}", NFD::GetError() );
        }

        return Path{ outPath.get() };
    }

    auto OpenInExplorer( const Path &path ) -> void {
#if defined( MIKOTO_PLATFORM_WINDOWS )
        eastl::wstring widePath{ path.GetPathTyped<eastl::wstring>() };
        if ( std::filesystem::is_regular_file( path.GetPathTyped<std::filesystem::path>() ) ) {
            OpenAndSelectFile( widePath );
        } else {
            ShellExecuteW( nullptr, L"open", widePath.c_str(), nullptr, nullptr, SW_SHOWDEFAULT );
        }
#endif
    }
}// namespace mikoto::filesystem