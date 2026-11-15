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

#ifndef MIKOTO_PANELS_CONTENT_BROWSER_PANEL_HH
#define MIKOTO_PANELS_CONTENT_BROWSER_PANEL_HH

#include <imgui.h>

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/FileSystem.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <Application/ThumbnailCache.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {

    struct EditorState;

    enum class IconType {
        eInvalid = -1,
        eFolder,
        eAudioFile,
        eFontFile,
        eTrueTypeFont,
        eJsonFile,
        eConfigFile,
        eParticles,
        eVideoFile,
        eSubtitleFile,
        eCppFile,
        eJavaScriptFile,
        ePdfFile,
        ePngFile,
        eTextFile,
        eRegularFile,
    };

    struct ContentBrowserPanelDescription {
        renderer::rhi::IGpuDevice* mDevice{ nullptr };
        EditorState* mState{ nullptr };

        filesystem::Path mProjectBasePath{};
        filesystem::Path mResourcesBasePath{};

        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class ContentBrowserPanel final : public Panel {
    public:
        explicit ContentBrowserPanel( const ContentBrowserPanelDescription& desc );

        auto OnRender( float timeStep ) -> void override;
        auto SetProjectDirectory( const filesystem::Path& root, const filesystem::Path& cache ) -> void;

        ~ContentBrowserPanel() override = default;

    private:
        struct DirectoryEntry {
            filesystem::Path mPath{};
            filesystem::FileType mType{ filesystem::FileType::eInvalid };
            bool mIsDirectory{};
        };

        struct DirectoryListing {
            eastl::vector<DirectoryEntry> mEntries{};
            eastl::string mError{};
        };

        enum class FileOperation {
            eNone,
            eNewFolder,
            eNewFile,
            eRename,
            eRemove
        };

        enum class NewFileType {
            eText,
            eLua,
            eCSharp,
            eNative,
            eSurfaceMaterial,
            eParticleMaterial,
            eTextMaterial,
            eSkyboxMaterial,
            ePostProcessMaterial,
            eVolumeMaterial
        };

    private:
        auto DrawHeader() -> void;
        auto DrawBreadcrumbs() -> void;
        auto DrawSidebarShadow() -> void;
        auto RefreshDirectories() -> void;
        auto ApplyFileOperation() -> void;
        auto DrawCreateMenuItems() -> void;
        auto DrawFileOperationDialog() -> void;
        auto GetNewFileExtension() const -> core::cstr;
        auto DrawSideHierarchy( const filesystem::Path& root ) -> void;
        auto NavigateTo( const filesystem::Path& path, bool recordHistory = true ) -> void;

        auto BeginFileOperation( FileOperation operation ) -> void;
        auto DrawEntry( const DirectoryEntry& entry, core::f32 size ) -> void;
        auto DrawFolderEntry( const filesystem::Path& path, core::f32 size ) -> void;
        auto GetDirectoryListing( const filesystem::Path& path ) -> const DirectoryListing&;

        auto DrawCurrentDirItems() -> void;
        auto DrawBlankSpaceRightClickMenu() -> void;

    private:
        EditorState* mEditorState{ nullptr };
        renderer::rhi::IGpuDevice* mDevice{ nullptr };

        ImGuiTextFilter mSearchFilter{};

        eastl::unique_ptr<ThumbnailCache> mThumbnailCache{};

        // Retain textures, not backend descriptor IDs: unused ImGui descriptors
        // can be retired between frames while this panel is hidden.
        ankerl::unordered_dense::map<IconType, renderer::rhi::TextureHandle> mIconTextures{};

        filesystem::Path mProjectBasePath{};
        filesystem::Path mResourcesBasePath{};
        filesystem::Path mCurrentDirectory{};

        bool mShowFileTypeHint{ true };
        bool mShowItemShadows{ true };
        bool mShowDirectoryOnlyInSideView{ false };

        core::f32 mItemShadowSize{ 1.0f };
        core::f32 mThumbnailSize{ 67.0f };

        static constexpr core::f32 kThumbnailPadding{ 8.0f };

        core::usize mHistoryIndex{};
        eastl::vector<filesystem::Path> mHistory{};

        // Stable allocations let recursive tree drawing insert listings without
        // invalidating the listing currently being traversed.
        ankerl::unordered_dense::map<filesystem::Path, eastl::unique_ptr<DirectoryListing>> mDirectoryListings{};

        bool mResetScroll{};
        core::f32 mDirectoryRefreshElapsed{};
        eastl::vector<core::usize> mFilteredEntries{};

        FileOperation mFileOperation{ FileOperation::eNone };
        NewFileType mNewFileType{ NewFileType::eText };
        
        bool mOpenOperationDialog{};
        eastl::string mOperationError{};

        filesystem::Path mSelectedItem{};
        filesystem::Path mOperationTarget{};
        eastl::array<char, 256> mNameBuffer{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_PANELS_CONTENT_BROWSER_PANEL_HH

