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

#include <limits>
#include <fstream>
#include <filesystem>

#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Filesystem/File.hh>

namespace mikoto::filesystem {

    File::File( const Path& path, bool create )
        : mPath{ path.ToAbsolute() } {
        MKT_BEGIN_PROFILER();

        if ( mPath.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open an empty file path." );
        }

        if ( create ) {
            std::filesystem::create_directories( mPath.GetPathTyped<std::filesystem::path>().parent_path() );
            // Append mode creates missing files without truncating existing user data.
            std::ofstream output{};
            output.exceptions( std::ios::failbit | std::ios::badbit );
            output.open( mPath.GetPathTyped<std::filesystem::path>(), std::ios::binary | std::ios::app );
            output.close();
        }

        LoadContents();
    }

    File::File( core::cstr path, bool create )
        : File{ Path{ path }, create } {
    }

    File::File( const eastl::string& path, bool create )
        : File{ Path{ path }, create } {
    }

    File::File( eastl::string_view path, bool create )
        : File{ Path{ path }, create } {
    }

    auto File::LoadContents() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mPath.IsFile() ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "Cannot read file '{}': not a regular file.", mPath.GetC_Str() ) );
        }

        std::ifstream input{};
        input.exceptions( std::ios::failbit | std::ios::badbit );
        input.open( mPath.GetPathTyped<std::filesystem::path>(), std::ios::binary | std::ios::ate );

        const auto end{ input.tellg() };

        // TODO: Validate range of end()

        // Build a candidate snapshot so a truncated or failed read cannot
        // destroy the last complete contents held by this File.
        eastl::string contents{};
        contents.resize( core::as<core::usize>( end ) );

        input.seekg( 0, std::ios::beg );

        if ( !contents.empty() ) {
            input.read( contents.data(), core::as<std::streamsize>( contents.size() ) );
        }

        input.close();
        mContents.swap( contents );

        UpdateType();
    }

    auto File::UpdateType() -> void {
        MKT_BEGIN_PROFILER();

        // Direct signature checks avoid allocated vectors and ambiguous hash-map
        // iteration order for overlapping signatures such as JPEG/JPG.
        const eastl::string_view contents{ mContents };

        if ( contents.starts_with( eastl::string_view{ "\x89PNG\r\n\x1a\n", 8 } ) ) {
            mType = FileType::ePng;
        }
        else if ( contents.starts_with( eastl::string_view{ "\xff\xd8\xff", 3 } ) ) {
            mType = mPath.GetNormalizedExtension() == "jpeg" ? FileType::eJpeg : FileType::eJpg;
        }
        else if ( contents.starts_with( "#?RADIANCE" ) || contents.starts_with( "#?RGBE" ) ) {
            mType = FileType::eHdr;
        }
        else if ( contents.starts_with( eastl::string_view{ "\xabKTX 20\xbb\r\n\x1a\n", 12 } ) ) {
            mType = FileType::eKtx2;
        }
        else if ( contents.starts_with( "ID3" ) ) {
            mType = FileType::eMp3;
        }
        else if ( contents.size() >= 12 && contents.starts_with( "RIFF" ) && contents.substr( 8, 4 ) == "WAVE" ) {
            mType = FileType::eWav;
        }
        else if ( contents.size() >= 12 && contents.substr( 4, 4 ) == "ftyp" ) {
            mType = FileType::eMp4;
        }
        else {
            mType = InferFileTypeFromExtension( mPath.GetNormalizedExtension() );
        }
    }

    auto File::RefreshIfDirty() -> void {
        MKT_BEGIN_PROFILER();

        if ( mDiskContentsDirty->load( eastl::memory_order_relaxed ) ) {
            UpdateContentsFromDisk();
        }
    }

    auto File::GetName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mPath.GetFilename();
    }

    auto File::GetPath() const -> const Path& {
        MKT_BEGIN_PROFILER();
        return mPath;
    }

    auto File::GetDirectory() const -> Path {
        MKT_BEGIN_PROFILER();
        return mPath.GetDirectory();
    }

    auto File::HasContents() const -> bool {
        MKT_BEGIN_PROFILER();
        return !GetContentsString().empty();
    }

    auto File::GetContentsBytes() -> void* {
        MKT_BEGIN_PROFILER();
        return mContents.data();
    }

    auto File::GetContentsBytes() const -> const void* {
        MKT_BEGIN_PROFILER();
        return GetContentsString().data();
    }

    auto File::GetContentsString() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mContents;
    }

    auto File::IsExtension( eastl::string_view extension ) const -> bool {
        MKT_BEGIN_PROFILER();

        if ( extension.empty() ) {
            return false;
        }

        if ( extension.front() == '.' ) {
            extension.remove_prefix( 1 );
        }

        eastl::string_view fileExtension{ mPath.GetExtension() };

        if ( fileExtension.front() == '.' ) {
            fileExtension.remove_prefix( 1 );
        }

        return string::Equal( fileExtension, extension );
    }

    auto File::IsExtension( std::initializer_list<const eastl::string_view> extensions ) const -> bool {
        MKT_BEGIN_PROFILER();

        if ( extensions.empty() ) {
            return false;
        }

        for ( const auto& ext: extensions ) {
            if ( IsExtension( ext ) ) {
                return true;
            }
        }

        return false;
    }

    auto File::GetSize() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return GetContentsString().size();
    }

    auto File::GetType() const -> FileType {
        MKT_BEGIN_PROFILER();
        return mType;
    }

    auto File::GetExtension() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mPath.GetExtension();
    }

    auto File::FlushContents() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mFileUpdateMutex };

        std::ofstream output{};
        output.exceptions( std::ios::failbit | std::ios::badbit );
        output.open( mPath.GetPathTyped<std::filesystem::path>(), std::ios::binary | std::ios::trunc );

        if ( !mContents.empty() ) {
            output.write( mContents.data(), core::as<std::streamsize>( mContents.size() ) );
        }

        output.close();
    }

    auto File::SetContents( eastl::string&& contents ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mFileUpdateMutex };

        mDiskContentsDirty->store( false, eastl::memory_order_relaxed );
        mContents = eastl::move( contents );

        UpdateType();
    }

    auto File::Write( const void* ptr, core::usize sizeBytes ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mFileUpdateMutex };

        eastl::string contents{};
        if ( sizeBytes != 0 ) {
            contents.assign( core::as<core::cstr>( ptr ), sizeBytes );
        }

        std::ofstream output{};
        output.exceptions( std::ios::failbit | std::ios::badbit );
        output.open( mPath.GetPathTyped<std::filesystem::path>(), std::ios::binary | std::ios::trunc );

        if ( !contents.empty() ) {
            output.write( contents.data(), core::as<std::streamsize>( contents.size() ) );
        }

        output.close();
        mContents.swap( contents );

        UpdateType();
    }

    auto File::Write( eastl::string_view data ) -> void {
        MKT_BEGIN_PROFILER();
        Write( data.data(), data.size() );
    }

    auto File::UpdateContentsFromDisk() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mFileUpdateMutex };

        // Consume before reading: an event during the read remains pending.
        mDiskContentsDirty->store( false, eastl::memory_order_relaxed );

        try {
            LoadContents();
        } catch ( ... ) {
            mDiskContentsDirty->store( true, eastl::memory_order_relaxed );
            throw;
        }
    }

}// namespace mikoto::filesystem