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

#ifndef MIKOTO_FILESYSTEM_FILE_HH
#define MIKOTO_FILESYSTEM_FILE_HH

#include <mutex>

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/FileSystem.hh>

namespace mikoto::filesystem {

    /**
     * @brief Owns an absolute file path and a binary-safe, in-memory snapshot.
     * @details Getters read the cached snapshot without disk I/O. Watcher changes
     * are applied through FileService::LoadFile/GetFile or an explicit reload.
     * @note Content pointers and views may be invalidated by the next content
     * replacement or destruction. Callers must synchronize reads and borrowed
     * buffer access against updates; the update mutex does not protect getters.
     */
    class File final : public core::INonCopyable, public core::INonMoveable, public core::ReferenceCounted<File> {
    public:
        /**
         * @brief Resolves the path and loads the complete file into memory.
         * @param path File to open; stored as an absolute path.
         * @param create Creates a missing file without truncating an existing one.
         * Missing parent directories are also created.
         * @throws core::RuntimeException If the path is empty, is not a regular
         * file, or its size cannot be represented by the snapshot/read operation.
         * @throws std::ios_base::failure If opening, reading, or closing fails.
         * @note Path/filesystem and allocation errors also propagate.
         */
        explicit File( const Path& path, bool create = false );


        /**
         * @brief Opens a file from a null-terminated path string.
         * @param path Null-terminated file path.
         * @param create Whether to create a missing file.
         * @see File(const Path&, bool) for loading behavior and errors.
         */
        explicit File( core::cstr path, bool create = false );


        /**
         * @brief Opens a file from an owning path string.
         * @param path File path, copied during construction.
         * @param create Whether to create a missing file.
         * @see File(const Path&, bool) for loading behavior and errors.
         */
        explicit File( const eastl::string& path, bool create = false );


        /**
         * @brief Opens a file from a path view.
         * @param path File path, copied during construction.
         * @param create Whether to create a missing file.
         * @see File(const Path&, bool) for loading behavior and errors.
         */
        explicit File( eastl::string_view path, bool create = false );


        /**
         * @brief Gets the filename portion of the stored path.
         * @return Borrowed filename including its extension, valid for this object's lifetime.
         */
        MKT_NODISCARD auto GetName() const -> eastl::string_view;


        /**
         * @brief Gets the stored absolute file path.
         * @return Reference to the path, valid for this object's lifetime.
         */
        MKT_NODISCARD auto GetPath() const -> const Path&;


        /**
         * @brief Gets the file's parent directory.
         * @return Parent directory as an independent Path value.
         */
        MKT_NODISCARD auto GetDirectory() const -> Path;


        /**
         * @brief Checks whether the cached snapshot is nonempty.
         * @return True if at least one byte is cached; false otherwise.
         */
        MKT_NODISCARD auto HasContents() const -> bool;


        /**
         * @brief Borrows writable storage for the cached bytes.
         * @return Buffer containing GetSize() bytes; do not free or resize it.
         * @note Direct edits do not update the inferred type or save to disk.
         * Use SetContents() to replace contents and recompute their type.
         */
        MKT_NODISCARD auto GetContentsBytes() -> void*;


        /**
         * @brief Borrows read-only storage for the cached bytes.
         * @return Buffer containing GetSize() bytes; do not free it.
         */
        MKT_NODISCARD auto GetContentsBytes() const -> const void*;


        /**
         * @brief Returns the cached binary-safe string, which may contain null bytes.
         * @return Read-only reference to the cached contents, owned by this object.
         * @note The reference borrows this object's storage; it is not a snapshot
         * copy and its contents can change on subsequent updates.
         */
        MKT_NODISCARD auto GetContentsString() const -> const eastl::string&;


        /**
         * @brief Compares the path extension without regard to case.
         * @param extension Extension with or without a leading dot.
         * @return Whether the extension matches; an empty argument returns false.
         */
        MKT_NODISCARD auto IsExtension( eastl::string_view extension ) const -> bool;

        
        /**
         * @brief Compares the path extension against a list of extensions without regard to case.
         * @param extensions A span of extensions to check against, with or without leading dots.
         * @return Whether the path extension matches any in the list; an empty argument returns false.
         */
        MKT_NODISCARD auto IsExtension( std::initializer_list<const eastl::string_view> extensions ) const -> bool;


        /**
         * @brief Gets the size of the cached snapshot.
         * @return Number of cached bytes, excluding the string terminator.
         */
        MKT_NODISCARD auto GetSize() const -> core::usize;


        /**
         * @brief Returns the type inferred during the last content replacement.
         * @details Recognized signatures take precedence over the extension fallback.
         * @return File type inferred from the last content replacement.
         */
        MKT_NODISCARD auto GetType() const -> FileType;


        /**
         * @brief Gets the extension from the stored path.
         * @return Borrowed extension as provided by Path::GetExtension(), valid
         * for this object's lifetime.
         */
        MKT_NODISCARD auto GetExtension() const -> eastl::string_view;


        /**
         * @brief Writes the cached contents directly to disk, replacing the file.
         * @details Keeps the in-memory snapshot unchanged.
         * @throws std::ios_base::failure If opening, writing, or closing fails.
         */
        auto FlushContents() -> void;


        /**
         * @brief Replaces the cached contents without writing to disk.
         * @param contents Binary-safe string moved into this object.
         * @details Clears the pending disk-dirty flag and recomputes the file type.
         * Call FlushContents() to persist this replacement.
         */
        auto SetContents( eastl::string&& contents ) -> void;


        /**
         * @brief Writes new bytes to disk, then replaces the cached contents.
         * @param ptr Source buffer; may be null only when sizeBytes is zero.
         * @param sizeBytes Number of bytes to copy and save; zero writes an empty file.
         * @details Writes directly to the destination with std::ofstream. The
         * cached contents and inferred type are updated after a successful close.
         * @throws core::RuntimeException If the buffer or byte count is invalid.
         * @throws std::ios_base::failure If opening, writing, or closing fails.
         */
        auto Write( const void* ptr, core::usize sizeBytes ) -> void;


        /**
         * @brief Writes a string's exact bytes to this file, then updates its cached contents.
         * @param data Bytes to copy and save, including embedded nulls; no string terminator is added.
         * An empty view truncates the file to zero bytes.
         * @details Delegates to the binary Write() overload. The cached contents and
         * inferred type are updated only after the file is successfully written and closed.
         * @see Write(const void*, core::usize) for writing behavior and errors.
         */
        auto Write( eastl::string_view data ) -> void;


        /**
         * @brief Reloads disk contents and recomputes the type, discarding cached edits.
         * @details Replaces the snapshot only after a complete successful read.
         * On failure, retains the previous snapshot and marks it dirty for a retry.
         * Watcher notifications arriving during the read remain pending.
         * @see LoadContents() for read errors.
         */
        auto UpdateContentsFromDisk() -> void;


    private:
        friend class FileService;

        /**
         * @brief Reads a candidate snapshot and installs it after a successful close.
         * @pre Called during construction or while holding mFileUpdateMutex.
         * @throws core::RuntimeException If the path is not a regular file or its
         * size cannot be represented by the snapshot/read operation.
         * @throws std::ios_base::failure If opening, reading, or closing fails.
         * @note Filesystem and allocation errors also propagate.
         */
        auto LoadContents() -> void;


        /**
         * @brief Reloads only when a watcher has marked the disk contents dirty.
         * @details Used by FileService; propagates reload errors and preserves
         * the dirty flag on failure so a later access can retry.
         * @see UpdateContentsFromDisk()
         */
        auto RefreshIfDirty() -> void;


        /**
         * @brief Infers mType from known content signatures, falling back to the extension.
         * @pre Called during construction or while holding mFileUpdateMutex.
         */
        auto UpdateType() -> void;

    private:
        Path mPath{};
        eastl::string mContents{};
        FileType mType{ FileType::eInvalid };

        std::mutex mFileUpdateMutex{};

        // Make shared cause this flag can be shared with external entities.
        // Watchers retain only this flag, never the File or its potentially large contents.
        eastl::shared_ptr<eastl::atomic<bool>> mDiskContentsDirty{ eastl::make_shared<eastl::atomic<bool>>( false ) };
    };

    using FileHandle = core::Ref<File>;
}// namespace mikoto::filesystem

#endif// MIKOTO_FILESYSTEM_FILE_HH
