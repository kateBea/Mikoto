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

#include <filesystem>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>

#include <Filesystem/Path.hh>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    MKT_NODISCARD static auto ToStdPath( eastl::string_view path ) -> std::filesystem::path {
        MKT_BEGIN_PROFILER();

        if ( path.empty() ) {
            return {};
        }

        if ( path.find( '\0' ) != eastl::string_view::npos ) {
            MKT_THROW_RUNTIME_ERROR( "Paths cannot contain embedded null characters." );
        }

        return std::filesystem::path{ reinterpret_cast<const char8_t*>( path.data() ), reinterpret_cast<const char8_t*>( path.data() ) + path.size() };
    }

    MKT_NODISCARD static auto ToEastlString( const std::filesystem::path& path ) -> eastl::string {
        MKT_BEGIN_PROFILER();
        const auto utf8{ path.u8string() };
        return eastl::string{ reinterpret_cast<core::cstr>( utf8.data() ), utf8.size() };
    }

    MKT_NODISCARD static auto DetermineFileType( const std::filesystem::path& path ) noexcept -> PathFileType {
        MKT_BEGIN_PROFILER();
        std::error_code ec{};

        const auto status{ std::filesystem::status( path, ec ) };

        if ( std::filesystem::is_regular_file( status ) ) {
            return PathFileType::eFile;
        }

        if ( std::filesystem::is_directory( status ) ) {
            return PathFileType::eDirectory;
        }

        return PathFileType::eInvalid;
    }

    Path::Path() = default;

    Path::Path( const eastl::string& path )
        : Path{ eastl::string_view{ path } } {
    }

    Path::Path( const std::filesystem::path& path )
        : Path{ ToEastlString( path ) } {
    }

    Path::Path( const std::filesystem::directory_entry& path )
        : Path{ std::filesystem::path( path ) } {
    }

    Path::Path( core::cstr path )
        : Path{ eastl::string_view{ path != nullptr ? path : "" } } {
    }

    Path::Path( eastl::string_view path )
        : mPathUtf8{ path }, mNativePath{ ToStdPath( mPathUtf8 ) } {
        MKT_BEGIN_PROFILER();

        const auto& stdPath{ mNativePath };

        mStem = ToEastlString( stdPath.stem() );

        mFilename = ToEastlString( stdPath.filename() );
        mDirectoryName = ToEastlString( stdPath.parent_path().filename() );
        mIsAbsolutePath = stdPath.is_absolute();

        mExtension = ToEastlString( stdPath.extension() );

        mNormalizedExtension = mExtension;

        if ( !mNormalizedExtension.empty() && mNormalizedExtension[0] == '.' ) {
            mNormalizedExtension.erase( mNormalizedExtension.begin() );
        }

        eastl::transform( mNormalizedExtension.begin(), mNormalizedExtension.end(), mNormalizedExtension.begin(),
            []( char c ) {
            return c >= 'A' && c <= 'Z' ? core::as<char>( c + ( 'a' - 'A' ) ) : c;
        } );
    }

    auto Path::operator==( const Path& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mPathUtf8 == other.mPathUtf8;
    }

    auto Path::operator<( const Path& other ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mPathUtf8 < other.mPathUtf8;
    }

    auto Path::Exists() const noexcept -> bool {
        MKT_BEGIN_PROFILER();
        std::error_code error{};
        return std::filesystem::exists( mNativePath, error );
    }

    auto Path::GetPath() const noexcept -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mPathUtf8;
    }

    auto Path::GetC_Str() const noexcept -> cstr {
        MKT_BEGIN_PROFILER();
        return mPathUtf8.c_str();
    }

    auto Path::GetStem() const noexcept -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mStem;
    }

    auto Path::GetExtension() const noexcept -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mExtension;
    }

    auto Path::GetNormalizedExtension() const noexcept -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mNormalizedExtension;
    }

    auto Path::GetAbsolute() const -> eastl::string {
        MKT_BEGIN_PROFILER();

        if ( IsEmpty() ) {
            return {};
        }

        return ToEastlString( std::filesystem::absolute( mNativePath ).lexically_normal() );
    }

    auto Path::IsDirectoryEmpty() const -> bool {
        MKT_BEGIN_PROFILER();
        std::error_code error{};
        return std::filesystem::is_directory( mNativePath, error ) && std::filesystem::is_empty( mNativePath, error );
    }

    auto Path::IsExtension( eastl::string_view extension ) const -> bool {
        MKT_BEGIN_PROFILER();

        if ( extension.empty() ) {
            return false;
        }

        if ( extension.front() == '.' ) {
            extension.remove_prefix( 1 );
        }

        eastl::string_view fileExtension{ mExtension };

        if ( fileExtension.front() == '.' ) {
            fileExtension.remove_prefix( 1 );
        }

        return string::Equal( fileExtension, extension );
    }

    auto Path::IsExtension( std::initializer_list<const eastl::string_view> extensions ) const -> bool {
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

    auto Path::GetFilename() const noexcept -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mFilename;
    }

    auto Path::GetDirectory() const -> Path {
        MKT_BEGIN_PROFILER();
        return Path{ mNativePath.parent_path() };
    }

    auto Path::GetDirectoryName() const noexcept -> eastl::string_view {
        MKT_BEGIN_PROFILER();

        if ( mPathUtf8.empty() ) {
            return {};
        }

        return IsDirectory() && !mFilename.empty() ? eastl::string_view{ mFilename } : eastl::string_view{ mDirectoryName };
    }

    auto Path::GetFileType() const noexcept -> PathFileType {
        MKT_BEGIN_PROFILER();
        return DetermineFileType( mNativePath );
    }

    auto Path::IsEmpty() const noexcept -> bool {
        MKT_BEGIN_PROFILER();
        return mPathUtf8.empty();
    }

    auto Path::IsFile() const noexcept -> bool {
        MKT_BEGIN_PROFILER();
        return GetFileType() == PathFileType::eFile;
    }

    auto Path::IsDirectory() const noexcept -> bool {
        MKT_BEGIN_PROFILER();
        return GetFileType() == PathFileType::eDirectory;
    }

    auto Path::IsType( PathFileType type ) const noexcept -> bool {
        MKT_BEGIN_PROFILER();
        return GetFileType() == type;
    }

    auto Path::StartsWith( eastl::string_view str ) const -> bool {
        MKT_BEGIN_PROFILER();
        return eastl::string_view{ mPathUtf8 }.starts_with( str );
    }

    auto Path::Contains( eastl::string_view str ) const -> bool {
        MKT_BEGIN_PROFILER();
        return eastl::string_view{ mPathUtf8 }.find( str ) != eastl::string_view::npos;
    }

    auto Path::EndsWith( eastl::string_view str ) const -> bool {
        MKT_BEGIN_PROFILER();
        return eastl::string_view{ mPathUtf8 }.ends_with( str );
    }

    auto Path::IsAbsolutePath() const noexcept -> bool {
        MKT_BEGIN_PROFILER();
        return mIsAbsolutePath;
    }

    auto Path::ToRelative() const -> Path {
        MKT_BEGIN_PROFILER();

        if ( mPathUtf8.empty() ) {
            return Path{};
        }

        std::error_code ec{};
        const auto& absolutePath{ mNativePath };
        const auto currentPath{ std::filesystem::current_path( ec ) };

        if ( ec ) {
            return *this;
        }

        const auto relativePath{ std::filesystem::relative( absolutePath, currentPath, ec ) };

        if ( ec ) {
            return *this;
        }

        return Path{ relativePath };
    }

    auto Path::ToAbsolute() const -> Path {
        MKT_BEGIN_PROFILER();

        if ( mPathUtf8.empty() ) {
            return Path{};
        }

        return Path{ std::filesystem::absolute( mNativePath ).lexically_normal() };
    }

    Path::operator eastl::string_view() const {
        MKT_BEGIN_PROFILER();
        return mPathUtf8;
    }

    Path::operator const eastl::string&() const {
        MKT_BEGIN_PROFILER();
        return mPathUtf8;
    }

    auto PathBuilder::SetPath( const Path& path ) -> PathBuilder& {
        MKT_BEGIN_PROFILER();
        mPath /= path.GetPathTyped<std::filesystem::path>();
        return *this;
    }

    auto PathBuilder::Build() const -> Path {
        MKT_BEGIN_PROFILER();
        return Path{ mPath };
    }
}// namespace mikoto::filesystem