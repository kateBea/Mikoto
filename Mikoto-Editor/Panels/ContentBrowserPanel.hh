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

#ifndef MIKOTO_CONTENT_BROWSER_HH
#define MIKOTO_CONTENT_BROWSER_HH

#include <imgui.h>

#include <EASTL/deque.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Panels/Panel.hh>
#include <Renderer/Core/GpuDevice.hh>

#include <Application/ThumbnailCache.hh>

namespace mikoto::editor {

    struct EditorState;

    enum class IconType {
        eInvalid = -1,
        eFolder,
        eAudioFile,
        eVideoFile,
        eImageFile,
        eRegularFile,
        eMaterialFile,
    };

    struct ContentBrowserPanelDescription {
        IGpuDevice* mDevice{ nullptr };
        EditorState* mState{ nullptr };

        Path mProjectBasePath{};
        Path mResourcesBasePath{};
    };

    class ContentBrowserPanel final : public Panel {
    public:
        explicit ContentBrowserPanel(const ContentBrowserPanelDescription& desc);

        auto OnUpdate(float timeStep) -> void override;

        ~ContentBrowserPanel() override = default;

    private:
        auto DrawHeader() -> void;
        auto DrawSideHierarchy( const Path& root ) -> void;
        auto DrawMainBody() -> void;

        auto DrawBlankSpaceRightClickMenu() -> void;

        auto DrawCurrentDirItems() -> void;

    private:
        IGpuDevice* mDevice{ nullptr };
        EditorState* mEditorState{ nullptr };

        ImGuiTextFilter mSearchFilter{};

        eastl::unique_ptr<ThumbnailCache> mThumbnailCache{};
        ankerl::unordered_dense::map<IconType, ImTextureID> mThumbnailHandles{};

        Path mProjectBasePath{};
        Path mResourcesBasePath{};

        Path mCurrentDirectory{};
        Path mPreviousDirectory{};
        Path mForwardDirectory{};

        bool mShowFileTypeHint{ false };
        float mThumbnailSize{ 100.0f };

        eastl::deque<Path> mDirectoryStack{};

        // NOTE: texture is static because drag and
        // drop needs this to persis
        TextureHandle mThumbnail{};

        Path mSelectedItem{};
    };
}


#endif//MIKOTO_CONTENT_BROWSER_HH
