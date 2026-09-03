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

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <ankerl/unordered_dense.h>

#include <Filesystem/File.hh>
#include <Logging/Logger.hh>
#include <filesystem>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    auto File::InferExtensionFromFileSignature( const eastl::string &fileContent ) -> eastl::string {
        eastl::string result{};

        // https://en.wikipedia.org/wiki/List_of_file_signatures
        static const ankerl::unordered_dense::map<eastl::string, eastl::vector<uchar>> kSignatureMap{
            { "png", { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A } },   // PNG
            { "jpeg", { 0xFF, 0xD8, 0xFF, 0xE0 } },                       // JPEG
            { "hdr", { '#', '?', 'R', 'A', 'D', 'I', 'A', 'N', 'C', 'E' } }, // HDR
            { "jpg", { 0xFF, 0xD8, 0xFF } },                              // JPG
            { "mp3", { 0x49, 0x44, 0x33 } },                              // MP3 with ID3 tag
            { "wav", { 'R', 'I', 'F', 'F' } },                            // WAV
            { "mp4", { 0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70 } },// MP4
        };

        const auto itResult{ std::ranges::find_if( kSignatureMap,
                                                   [&fileContent]( const auto &pair ) {
                                                       return CompareSignature( fileContent, pair.second );
                                                   } ) };

        if ( itResult != kSignatureMap.end() ) {
            result = itResult->first;
        }

        return result;
    }

    auto File::CompareSignature( const eastl::string &fileContent, const eastl::vector<uchar> &signature ) -> bool {
        if ( fileContent.size() < signature.size() ) {
            return false;
        }

        for ( size_t i{}; i < signature.size(); ++i ) {
            if ( as<uchar>( fileContent[i] ) != signature[i] ) {
                return false;
            }
        }

        return true;
    }

    File::File( const Path &path, bool create )
        : mPath{ path } {
        if (!create) {
            LoadContents();
        } else {
            mFileStream = std::fstream{ mPath.GetC_Str(), std::ios::out | std::ios::trunc | std::ios::binary };
            mFileStream.close();
        }

        mType = InferFileType( InferExtensionFromFileSignature(mContents) );
    }

    File::File( const eastl::string &path, bool create )
        : mPath{ path } {
        if (!create) {
            LoadContents();
        } else {
            mFileStream = std::fstream{ mPath.GetC_Str(), std::ios::out | std::ios::trunc | std::ios::binary };
            mFileStream.close();
        }

        mType = InferFileType( InferExtensionFromFileSignature(mContents) );
    }

    File::File( eastl::string_view path, bool create )
        : mPath{ path } {
        if (!create) {
            LoadContents();
        } else {
            mFileStream = std::fstream{ mPath.GetC_Str(), std::ios::out | std::ios::trunc | std::ios::binary };
            mFileStream.close();
        }

        mType = InferFileType( InferExtensionFromFileSignature(mContents) );
    }

    auto File::LoadContents() -> void {
        mFileStream = std::fstream{ mPath.GetC_Str(), std::ios::in | std::ios::binary };

        if ( !mFileStream.is_open() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to open file [ {} ]!", mPath.GetC_Str() );
            return;
        }

        mFileStream.seekg( 0, std::ios::end );
        const auto size{ as<size_t>( mFileStream.tellg() ) };
        mFileStream.seekg( 0, std::ios::beg );

        mContents.resize( size );

        if ( size > 0 ) {
            mFileStream.read( mContents.data(), as<std::streamsize>( size ) );
        }

        // Close stream
        mFileStream.close();
    }

    auto File::InferFileType( eastl::string_view extension ) -> FileType {
        const static ankerl::unordered_dense::map<eastl::string, FileType> values{
            { "png", FileType::ePng },
            { "jpeg", FileType::eJpeg },
            { "jpg", FileType::eJpg },
            { "bmp", FileType::eBmp },
            { "hdr", FileType::eHdr },
            { "tiff", FileType::eTiff },

            { "mp4", FileType::eMp4 },

            { "mp3", FileType::eMp3 },
            { "wav", FileType::eWav },
        };

        if ( const auto result{ values.find( eastl::string{ extension } ) }; result != values.end() ) {
            return result->second;
        }

        return FileType::eInvalid;
    }

    auto File::GetName() const -> eastl::string_view {
        return mPath.GetFilename();
    }

    auto File::GetPath() const -> const Path & {
        return mPath;
    }

    auto File::GetDirectory() const -> Path {
        return mPath.GetDirectory();
    }

    auto File::HasContents() const -> bool {
        return !mContents.empty();
    }

    auto File::GetContentsBytes() -> void * {
        return mContents.data();
    }

    auto File::GetContentsBytes() const -> const void * {
        return mContents.data();
    }

    auto File::GetContentsString() const -> const eastl::string & {
        return mContents;
    }

    auto File::GetSize() const -> usize {
        return mContents.size();
    }

    auto File::GetType() const -> FileType {
        return mType;
    }

    auto File::GetExtension() const -> eastl::string_view {
        return mPath.GetExtension();
    }

    auto File::FlushContents() -> void {
        mFileStream = std::fstream{ mPath.GetC_Str(), std::ios::out | std::ios::trunc | std::ios::binary };
        mFileStream.write(as<const char*>(mContents.data()), as<std::streamsize>( mContents.size() ));
        mFileStream.flush();
        mFileStream.close();
    }

    auto File::SetContents( eastl::string &&contents ) -> void {
        // Open to overwrite
        mContents = std::move( contents );
    }

    auto File::Write( const void *ptr, usize sizeBytes ) -> void {
        // Open to overwrite
        mFileStream = std::fstream{ mPath.GetC_Str(), std::ios::out | std::ios::trunc | std::ios::binary };
        mFileStream.write(as<const char*>(ptr), as<std::streamsize>( sizeBytes ));
        mFileStream.flush();
        mFileStream.close();
    }

    auto File::UpdateContentsFromDisk() -> void {
        LoadContents();
    }
}// namespace mikoto::filesystem