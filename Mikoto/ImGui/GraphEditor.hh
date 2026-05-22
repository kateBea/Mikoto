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

#ifndef MIKOTO_GRAPH_EDITOR_HH
#define MIKOTO_GRAPH_EDITOR_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <imgui_node_editor.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

namespace mikoto::gui {

    struct GraphNode {
        core::i32 mID{};
        eastl::string mKey{};
        eastl::string mDisplayName{};

        eastl::vector<eastl::string> mInputs{};
        eastl::vector<eastl::string> mOutputs{};
    };

    class GraphEditorBuilder {
    public:

        auto PushNode( eastl::string_view node ) -> void;

    private:
        friend class GraphEditor;

    private:
        ankerl::unordered_dense::map<eastl::string, GraphNode> mNodes{};
    };

    // TODO: investigate how we can properly use multiple
    class GraphEditor {
    public:
        explicit GraphEditor( eastl::string_view name );

        auto Render() -> void;

        auto Build( GraphEditorBuilder& builder ) -> void;

        MKT_NODISCARD auto GetName() const -> const eastl::string&;

    private:
        static auto RenderNode( const GraphNode& node ) -> void;

    private:
        eastl::string mName{};
        eastl::string mConfigLayoutFile{};

        ax::NodeEditor::Config mConfig{};
        ax::NodeEditor::EditorContext* mContext{};

        ankerl::unordered_dense::map<eastl::string, GraphNode> mNodes{};
    };
}

#endif// MIKOTO_GRAPH_EDITOR_HH
