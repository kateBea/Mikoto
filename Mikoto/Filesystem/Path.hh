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

#ifndef MIKOTO_FILESYSTEM_PATH_HH
#define MIKOTO_FILESYSTEM_PATH_HH

#include <concepts>
#include <filesystem>
#include <string_view>

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

namespace mikoto::filesystem {

    enum class PathFileType {
        eInvalid = -1,
        eFile,
        eDirectory,
    };

    class Path final {
    public:
        Path();
        Path( const eastl::string& path );
        Path( const std::filesystem::path& path );
        Path( const std::filesystem::directory_entry& path );

        Path( core::cstr path );
        Path( eastl::string_view path );

        Path( const Path& ) = default;
        Path( Path&& ) = default;

        auto operator=( const Path& ) -> Path& = default;
        auto operator=( Path&& ) noexcept -> Path& = default;

        // Implemented so that this guy can be directly used with unordered_dense
        auto operator==( const Path& other ) const -> bool;
        auto operator<( const Path& other ) const -> bool;

        MKT_NODISCARD auto Exists() const noexcept -> bool;
        MKT_NODISCARD auto GetPath() const noexcept -> eastl::string_view;
        MKT_NODISCARD auto GetC_Str() const noexcept -> core::cstr;
        MKT_NODISCARD auto GetStem() const noexcept -> eastl::string_view;
        MKT_NODISCARD auto GetExtension() const noexcept -> const eastl::string&;
        MKT_NODISCARD auto GetNormalizedExtension() const noexcept -> const eastl::string&;
        MKT_NODISCARD auto GetAbsolute() const -> eastl::string;

        MKT_NODISCARD auto IsDirectoryEmpty() const -> bool;
        MKT_NODISCARD auto IsExtension( eastl::string_view extension ) const -> bool;
        MKT_NODISCARD auto IsExtension( std::initializer_list<const eastl::string_view> extensions ) const -> bool;

        template<typename StringType>
        MKT_NODISCARD auto GetPathTyped() const -> StringType;// view, eastl::string, std::filesystem and const c_str, std::wstring

        MKT_NODISCARD auto GetFilename() const noexcept -> eastl::string_view;
        MKT_NODISCARD auto GetDirectory() const -> Path;

        // Directory's full name, or the containing directory's name for a file.
        MKT_NODISCARD auto GetDirectoryName() const noexcept -> eastl::string_view;

        MKT_NODISCARD auto GetFileType() const noexcept -> PathFileType;

        MKT_NODISCARD auto IsEmpty() const noexcept -> bool;
        MKT_NODISCARD auto IsFile() const noexcept -> bool;
        MKT_NODISCARD auto IsDirectory() const noexcept -> bool;
        MKT_NODISCARD auto IsType( PathFileType type ) const noexcept -> bool;

        MKT_NODISCARD auto StartsWith( eastl::string_view str ) const -> bool;
        MKT_NODISCARD auto Contains( eastl::string_view str ) const -> bool;
        MKT_NODISCARD auto EndsWith( eastl::string_view str ) const -> bool;

        MKT_NODISCARD auto IsAbsolutePath() const noexcept -> bool;

        MKT_NODISCARD auto ToRelative() const -> Path;
        MKT_NODISCARD auto ToAbsolute() const -> Path;

        MKT_NODISCARD operator eastl::string_view() const;
        MKT_NODISCARD operator const eastl::string&() const;

    private:
        eastl::string mStem{};
        eastl::string mPathUtf8{};
        eastl::string mExtension{};
        eastl::string mNormalizedExtension{};

        eastl::string mFilename{};
        eastl::string mDirectoryName{};

        std::filesystem::path mNativePath{};

        bool mIsAbsolutePath{ false };
    };

    class PathBuilder final {
    public:
        template<std::convertible_to<std::string_view> StringLike>
        auto SetPath( StringLike&& path ) -> PathBuilder& {
            MKT_BEGIN_PROFILER();
            const std::string_view view{ path };
            mPath /= Path{ eastl::string_view{ view.data(), view.size() } }.GetPathTyped<std::filesystem::path>();
            return *this;
        }

        auto SetPath( const Path& path ) -> PathBuilder&;

        MKT_NODISCARD auto Build() const -> Path;

    private:
        std::filesystem::path mPath{};
    };
}// namespace mikoto::filesystem

template<>
struct ankerl::unordered_dense::hash<mikoto::filesystem::Path> {
    using is_avalanching = void;

    auto operator()( mikoto::filesystem::Path const& path ) const noexcept -> mikoto::core::u64 {
        MKT_BEGIN_PROFILER();
        const eastl::string_view view{ path.GetPath() };
        return hash<std::string_view>{}( std::string_view{ view.data(), view.size() } );
    }
};// namespace ankerl::unordered_dense

#include <Filesystem/Path.inl>

#endif// MIKOTO_FILESYSTEM_PATH_HH
