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

#include <vector>
#include <string>
#include <string_view>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>

namespace Mikoto {

    auto ShowExampleAppCustomNodeGraph( bool* opened ) -> void;

    struct GraphNode {
        std::string Key{};        
        std::string DisplayName{};

        std::vector<std::string> Inputs{}; 
        std::vector<std::string> Outputs{};
    };

    class GraphEditorBuilder {
    public:


    private:

    };

    class GraphEditor {
    public:
        explicit GraphEditor( std::string_view name );

        auto Render() -> void;

        auto Build( const GraphEditorBuilder& builder ) -> void;

        MKT_NODISCARD auto GetName() const -> const std::string&;

    private:
        std::string m_Name{};
    };
}

#endif// MIKOTO_GRAPH_EDITOR_HH
