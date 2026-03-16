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

#include <Logging/Logger.hh>
#include <Filesystem/File.hh>

#include <Library/IO/File.hh>

namespace Mikoto {

    auto File::Load( const Path &path, FileMode openMode ) -> Unique<File> {
        if (openMode & MKT_FILE_OPEN_MODE_TRUNCATE ) {
            if ( std::fstream stream{ path, std::ios::trunc | std::ios::in | std::ios::out } ) {
                return Unique<File>( new File(path, std::move(stream), openMode) ) ;
            }
        }

        // Open the file once to import metadata and its contents
        if (openMode & MKT_FILE_OPEN_MODE_BINARY ) {
            if ( std::fstream stream{ path, std::ios::binary | std::ios::in | std::ios::out } ) {
                return Unique<File>( new File(path, std::move(stream), openMode) ) ;
            }
        }

        return nullptr;
    }

    auto File::Create( const Path &path, FileMode openMode ) -> Unique<File> {
        std::filesystem::path dir{ path };
        dir.remove_filename();

        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories( dir );
        }

        return Load( path ,openMode );
    }

    File::File( const Path& path, std::fstream&& stream, const FileMode openMode )
        : m_Path{ path }, m_PathUtf8{ path.string() }, m_Extension{ path.extension().string() }, m_FileStream{ std::move( stream ) }, m_OpenMode{ openMode } {
        UpdateContentsFromDisk();
    }

    auto File::GetContentsString() const -> const std::string & {
        // I need to make sure the file has been properly updated before i can read its contents
        std::lock_guard lock{ m_FileUpdateMutex };

        return m_Contents;
    }

    auto File::FlushContents() -> void {
        auto openMode{ std::ios_base::out };

        if (IsModeSet( m_OpenMode, MKT_FILE_OPEN_MODE_WRITE )) {
            openMode |= std::ios_base::trunc;
        }

        if (IsModeSet( m_OpenMode, MKT_FILE_OPEN_MODE_APPEND )) {
            openMode |= std::ios_base::app;
        }

        m_FileStream = std::move( std::fstream{ m_PathUtf8, openMode } );

        if ( m_FileStream.is_open() ) {
            m_FileStream << m_Contents;
            m_FileStream.flush();
            m_FileStream.close();
        }
    }

    auto File::UpdateContentsFromDisk() -> void {
        // If the file is closed we need to reopen it (just for reading)
        if (!m_FileStream.is_open() ) {
            m_FileStream.open( m_PathUtf8, std::ios::in );
        }

        LoadContents();
        InferFileSize();

        if ( m_FileStream.is_open() ) {
            m_FileStream.close();

            // Close file again
            m_FileStream = {};
        }

        m_Extension = InferExtensionFromFileSignature( m_Contents );
        m_Type = InferFileType( m_Extension );
    }

    auto File::LoadContents() -> void {
        if ( !m_FileStream.is_open() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to open file [ {} ]!", m_PathUtf8 );
            return;
        }

        m_Contents = std::move( std::string{ std::istreambuf_iterator( m_FileStream ),
                                             std::istreambuf_iterator<std::vector<char>::value_type>() } );
    }

    auto File::InferFileSize() -> void {
        if ( m_FileStream.is_open() ) {
            // seek to the start if we had already read from this file
            m_FileStream.seekg( 0 );
            m_Size = std::distance( std::istreambuf_iterator<char>( m_FileStream ), std::istreambuf_iterator<char>() );
        }
    }

    auto File::GetPath() const -> const std::string& {
        return m_PathUtf8;
    }

    auto File::GetName() const -> std::string {
        return m_Path.filename().string();
    }

    auto File::GetExtension() const -> const std::string& {
        return m_Extension;
    }

    auto File::GetPathView() const -> std::string_view {
        return m_PathUtf8;
    }

    auto File::GetContentsBytes() const -> const void* {
        return m_Contents.c_str();
    }

    auto File::GetType() const -> FileType {
        return m_Type;
    }

    auto File::GetSize() const -> Size {
        return m_Size;
    }

    auto File::SetContents( std::string &&contents ) -> void {
        std::lock_guard lock{ m_FileUpdateMutex };
        m_Contents = std::move(contents);
    }

    auto File::InferFileType( const std::string &extension ) -> FileType {
        const static std::unordered_map<std::string, FileType> values{
            { "png", FileType::PNG_IMAGE_TYPE },
            { "jpeg", FileType::JPEG_IMAGE_TYPE },
            { "jpg", FileType::JPG_IMAGE_TYPE },
            { "bmp", FileType::BMP_IMAGE_TYPE },

            { "mp3", FileType::MP3_AUDIO_TYPE },
            { "wav", FileType::WAV_AUDIO_TYPE },
        };

        if ( const auto result{ values.find( extension ) }; result != values.end() ) {
            return result->second;
        }

        return FileType::UNKNOWN_FILE_TYPE;
    }

    auto File::CompareSignature( const std::string &fileContent, const std::vector<UChar> &signature ) -> bool {
        if ( fileContent.size() < signature.size() ) return false;
        for ( size_t i = 0; i < signature.size(); ++i ) {
            if ( static_cast<UChar>( fileContent[i] ) != signature[i] ) {
                return false;
            }
        }
        return true;
    }

    auto File::InferExtensionFromFileSignature( const std::string &fileContent ) -> std::string {
        std::string result{};

        // https://en.wikipedia.org/wiki/List_of_file_signatures
        static const std::unordered_map<std::string, std::vector<UChar>> signatureMap{
            { "png", { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A } },   // PNG
            { "jpeg", { 0xFF, 0xD8, 0xFF, 0xE0 } },                       // JPEG
            { "jpg", { 0xFF, 0xD8, 0xFF } },                              // JPG
            { "mp3", { 0x49, 0x44, 0x33 } },                              // MP3 with ID3 tag
            { "wav", { 'R', 'I', 'F', 'F' } },                            // WAV
            { "mp4", { 0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70 } },// MP4
        };

        const auto itResult{ std::ranges::find_if( signatureMap,
            [&fileContent]( const std::pair<const std::string, std::vector<UChar>> &pair ) {
            return CompareSignature( fileContent, pair.second );
        } ) };

        if ( itResult != signatureMap.end() ) {
           result = itResult->first;
        }

        return result;
    }
}