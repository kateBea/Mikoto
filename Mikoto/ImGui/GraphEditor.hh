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

#ifndef MIKOTO_IMGUI_GRAPH_EDITOR_HH
#define MIKOTO_IMGUI_GRAPH_EDITOR_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <imgui_node_editor.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <ImGui/ImGuiWidget.hh>

namespace mikoto::imgui {

    // Separate camera, selection and layout for every dockable graph.
    class NodeEditorContext final : public core::INonCopyable {
    public:
        NodeEditorContext() = default;

        /** @brief Resets editor-only state when replacing
         * a graph document. Call outside its scope.
         */
        auto Reset() -> void;

        auto Get() -> ax::NodeEditor::EditorContext*;

        ~NodeEditorContext();

    private:
        ax::NodeEditor::EditorContext* mContext{};
    };

    class NodeEditorScope final : public core::INonCopyable {
    public:
        explicit NodeEditorScope( NodeEditorContext& context );

        ~NodeEditorScope();
    private:

        ax::NodeEditor::EditorContext* mPrevious{};
    };

    struct GraphResourceUsage {
        eastl::string mName{};
        eastl::string mDetails{};
        eastl::string mStage{};
        core::u32 mHandle{};
        bool mRead{};
        bool mWrite{};
        bool mImported{};
    };

    struct GraphNode {
        core::u32 mID{};
        eastl::string mKey{};
        eastl::string mDisplayName{};
        eastl::string mType{};
        eastl::string mPolicy{};
        eastl::vector<eastl::string> mDependencies{};
        eastl::vector<GraphResourceUsage> mResources{};
        bool mEnabled{ true };
        bool mExecuted{};
        core::u64 mSamples{};
        double mLastMs{};
        double mMinMs{};
        double mMaxMs{};
    };

    class GraphEditorBuilder {
    public:
        auto PushNode( eastl::string_view name ) -> GraphNode&;

    private:
        friend class GraphEditor;
        eastl::vector<GraphNode> mNodes{};
    };

    // Read-only snapshots; no pointers into renderer-owned data survive Build().
    class GraphEditor final {
    public:
        explicit GraphEditor( eastl::string_view name );
        auto Render( bool& open ) -> void;
        auto RenderContents() -> void;
        auto Build( GraphEditorBuilder& builder ) -> void;
        auto IsPaused() const -> bool;
        auto GetName() const -> const eastl::string&;

    private:
        auto Layout() -> void;
        auto DrawNode( const GraphNode& node ) -> void;
        auto DrawInspector() -> void;
        auto DrawGraph() -> void;
        auto DrawPassList() -> void;
        auto Matches( const GraphNode& node ) const -> bool;

        eastl::string mName{};
        NodeEditorContext mContext{};
        eastl::vector<GraphNode> mNodes{};
        ankerl::unordered_dense::map<eastl::string, core::u32> mNodeIDs{};
        core::u32 mNextID{ 1 };
        core::u32 mSelectedID{};
        ImGuiTextFilter mFilter{};
        bool mPaused{};
        bool mShowResources{ true };
        bool mShowInactive{ true };
        bool mLayoutRequested{ true };
        bool mFitRequested{ true };
        int mFitDelay{ 2 };
    };
}// namespace mikoto::imgui

#endif// MIKOTO_IMGUI_GRAPH_EDITOR_HH
