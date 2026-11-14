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

#include <cstdio>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/DotGraph.hh>

namespace mikoto::core {

    static auto OwnText( eastl::string_view text ) -> eastl::string {
        MKT_BEGIN_PROFILER();

        if ( text.empty() ) {
            return {};
        }

        return { text.data(), text.size() };
    }

    static auto AppendQuoted( eastl::string& output, eastl::string_view text ) -> void {
        MKT_BEGIN_PROFILER();
        // Quote all user text so punctuation cannot become DOT syntax.
        // Double backslashes also protect literal Graphviz escapes such
        // as \N. Preserve UTF-8 bytes and normalize line endings.
        output.push_back( '"' );

        for ( usize index{}; index < text.size(); ++index ) {
            const auto character{ text[index] };

            switch ( character ) {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\r':
                    if ( index + 1 < text.size() && text[index + 1] == '\n' ) {
                        ++index;
                    }

                    output += "\\n";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                default: {
                    const auto byte{ static_cast<ubyte>( character ) };

                    if ( ( byte < 0x20 && character != '\t' ) || byte == 0x7f ) {
                        // Render unsupported control bytes as literal \xHH,
                        // including embedded NULs, rather than emitting invalid DOT.
                        static constexpr cstr digits{ "0123456789ABCDEF" };
                        output += "\\\\x";
                        output.push_back( digits[byte >> 4] );
                        output.push_back( digits[byte & 0x0f] );
                    }
                    else {
                        output.push_back( character );
                    }

                    break;
                }
            }
        }

        output.push_back( '"' );
    }


    DotGraph::DotGraph( eastl::string_view name, GraphType type )
        : mName{ OwnText( name ) }, mType{ type } {
        MKT_BEGIN_PROFILER();
    }

    auto DotGraph::AddNode( eastl::string_view label ) -> NodeID {
        MKT_BEGIN_PROFILER();
        const NodeID id{ mNodes.size() };
        mNodes.push_back( Node{ .mLabel = OwnText( label ) } );
        return id;
    }

    auto DotGraph::AddEdge( NodeID from, NodeID to, eastl::string_view label ) -> bool {
        MKT_BEGIN_PROFILER();

        if ( from >= mNodes.size() || to >= mNodes.size() ) {
            return false;
        }

        mEdges.push_back( Edge{ .mFrom = from, .mTo = to, .mLabel = OwnText( label ) } );
        return true;
    }

    auto DotGraph::Clear() -> void {
        MKT_BEGIN_PROFILER();
        mEdges.clear();
        mNodes.clear();
    }

    auto DotGraph::ToDot() const -> eastl::string {
        MKT_BEGIN_PROFILER();
        const bool directed{ mType == GraphType::eDirected };
        eastl::string output{ directed ? "digraph " : "graph " };
        AppendQuoted( output, { mName.data(), mName.size() } );
        output += " {\n    rankdir=LR;\n    node [shape=box];\n";

        // Emit nodes independently of edges, preserving isolated nodes and
        // avoiding recursive traversal (a diagnostic graph may contain cycles).
        for ( NodeID index{}; index < mNodes.size(); ++index ) {
            output += string::Format( "    n{} [label=", index );
            const auto& label{ mNodes[index].mLabel };
            AppendQuoted( output, { label.data(), label.size() } );
            output += "];\n";
        }

        for ( const auto& edge: mEdges ) {
            output += string::Format( "    n{} {} n{}", edge.mFrom, directed ? "->" : "--", edge.mTo );

            if ( !edge.mLabel.empty() ) {
                output += " [label=";
                AppendQuoted( output, { edge.mLabel.data(), edge.mLabel.size() } );
                output += "]";
            }

            output += ";\n";
        }

        output += "}\n";
        return output;
    }

    auto DotGraph::Dump() const -> void {
        MKT_BEGIN_PROFILER();
        const auto dot{ ToDot() };

        fmt::print( "{}", fmt::string_view{ dot.data(), dot.size() } );
        std::fflush( stdout );
    }

}// namespace mikoto::core
