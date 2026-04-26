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

#include <Core/Core.hh>
#include <Core/String.hh>

#include <Filesystem/Path.hh>
#include <Logging/Assert.hh>

namespace mikoto::filesystem {
    namespace {

        auto ToStdPath( eastl::string_view path ) -> std::filesystem::path {
            return std::filesystem::path{ path.data() };
        }

        auto ToEastlString( const std::filesystem::path& path ) -> eastl::string {
            return eastl::string{ path.string().c_str() };
        }

        auto DetermineFileType( const std::filesystem::path& path ) noexcept -> PathFileType {
            std::error_code ec{};

            if ( std::filesystem::is_regular_file( path, ec ) ) {
                return PathFileType::eFile;
            }

            if ( std::filesystem::is_directory( path, ec ) ) {
                return PathFileType::eDirectory;
            }

            return PathFileType::eInvalid;
        }

    }// namespace

    Path::Path()
        : mPathObjectType{ PathFileType::eInvalid } {
    }

    Path::Path( const eastl::string& path )
        : Path{ eastl::string_view{ path } } {
    }

    Path::Path( const std::filesystem::path& path )
        : Path{ ToEastlString( path ) } {
    }

    Path::Path( const std::filesystem::directory_entry& path )
        : Path{ std::filesystem::path( path ) }
    {}

    Path::Path( const char* path )
        : Path{ eastl::string_view{ path != nullptr ? path : "" } } {
    }

    Path::Path( eastl::string_view path )
        : mPathUtf8{ path }, mPathObjectType{ PathFileType::eInvalid } {

        const auto stdPath{ ToStdPath( mPathUtf8 ) };

        mStem = std::filesystem::path{ mPathUtf8.c_str() }.stem().string().c_str();

        mExtension = ToEastlString( stdPath.extension() );
        mFilename = ToEastlString( stdPath.filename() );
        mPathObjectType = DetermineFileType( stdPath );
        mIsAbsolutePath = stdPath.is_absolute();
    }

    auto Path::operator==( const Path& other ) const -> bool {
        return mPathUtf8 == other.mPathUtf8;
    }

    auto Path::operator<( const Path& other ) const -> bool {
        return mPathUtf8 < other.mPathUtf8;
    }

    auto Path::Exists() const noexcept -> bool {
        return std::filesystem::exists( GetAbsolute().data() );
    }

    auto Path::GetPath() const noexcept -> eastl::string_view {
        return mPathUtf8;
    }

    auto Path::GetC_Str() const noexcept -> const char* {
        return mPathUtf8.c_str();
    }

    auto Path::GetStem() const noexcept -> eastl::string_view {
        return mStem;
    }

    auto Path::GetExtension() const noexcept -> eastl::string_view {
        return mExtension;
    }

    auto Path::GetAbsolute() const noexcept -> eastl::string {
        return string::ToEA_Stl( std::filesystem::absolute( mPathUtf8.c_str() ).string() );
    }

    auto Path::IsDirectoryEmpty() const -> bool {
        namespace fs = std::filesystem;

        return fs::is_directory( mPathUtf8.c_str() ) &&
               fs::directory_iterator( mPathUtf8.c_str() ) == fs::directory_iterator{};
    }

    auto Path::GetFilename() const noexcept -> eastl::string_view {
        return mFilename;
    }

    auto Path::GetDirectory() const -> Path {
        return std::filesystem::path{ mPathUtf8.c_str() }.remove_filename();
    }

    auto Path::GetDirectoryName() const noexcept -> eastl::string_view {
        if ( mPathUtf8.empty() ) {
            return {};
        }

        return mStem;
    }

    auto Path::GetFileType() const noexcept -> PathFileType {
        return mPathObjectType;
    }

    auto Path::IsEmpty() const noexcept -> bool {
        return mPathUtf8.empty();
    }

    auto Path::IsFile() const noexcept -> bool {
        return mPathObjectType == PathFileType::eFile;
    }

    auto Path::IsDirectory() const noexcept -> bool {
        return mPathObjectType == PathFileType::eDirectory;
    }

    auto Path::IsType( PathFileType type ) const noexcept -> bool {
        return mPathObjectType == type;
    }

    auto Path::StartsWith( eastl::string_view str ) const -> bool {
        return eastl::string_view{ mPathUtf8 }.starts_with( str );
    }

    auto Path::Contains( eastl::string_view str ) const -> bool {
        return eastl::string_view{ mPathUtf8 }.find( str ) != eastl::string_view::npos;
    }

    auto Path::EndsWith ( eastl::string_view str ) const -> bool {
        return eastl::string_view{ mPathUtf8 }.ends_with( str );
    }

    auto Path::IsAbsolutePath() const noexcept -> bool {
        return mIsAbsolutePath;
    }

    auto Path::ToRelative() const noexcept -> Path {
        if ( mPathUtf8.empty() ) {
            return Path{};
        }

        std::error_code ec{};
        const auto absolutePath{ ToStdPath( mPathUtf8 ) };
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

    auto Path::ToAbsolute() const noexcept -> Path {
        if ( mPathUtf8.empty() ) {
            return Path{};
        }

        std::error_code ec{};
        const auto absolutePath{ std::filesystem::absolute( ToStdPath( mPathUtf8 ), ec ) };

        if ( ec ) {
            return *this;
        }

        return Path{ absolutePath };
    }

    Path::operator eastl::string_view() const {
        return mPathUtf8;
    }

    Path::operator const eastl::string&() const {
        return mPathUtf8;
    }
}// namespace mikoto::filesystem
