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

#include <algorithm>

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/GraphEditor.hh>

namespace mikoto::imgui {
    namespace ed = ax::NodeEditor;


    static auto DrawGraphSocket( ed::PinId id, ed::PinKind kind ) -> void {
        ed::BeginPin( id, kind );
        DrawBlueprintPinIcon( BlueprintPinShape::eFlow, true, { 0.7f, 0.85f, 0.87f, 1.0f } );
        const auto minimum{ ImGui::GetItemRectMin() };
        const auto maximum{ ImGui::GetItemRectMax() };
        const ImVec2 center{ ( minimum.x + maximum.x ) * 0.5f, ( minimum.y + maximum.y ) * 0.5f };
        ed::PinPivotRect( center, center );
        ed::EndPin();
    }


    auto GraphEditorBuilder::PushNode( eastl::string_view name ) -> GraphNode& {
        MKT_BEGIN_PROFILER();

        for ( auto& node: mNodes ) {
            if ( node.mKey == name ) {
                return node;
            }
        }

        auto& node{ mNodes.emplace_back() };
        node.mKey = name;
        node.mDisplayName = name;
        return node;
    }

    GraphEditor::GraphEditor( eastl::string_view name )
        : mName{ name } {
    }

    auto GraphEditor::Build( GraphEditorBuilder& builder ) -> void {
        MKT_BEGIN_PROFILER();
        bool changed{ mNodes.size() != builder.mNodes.size() };
        eastl::sort( builder.mNodes.begin(), builder.mNodes.end(), []( const auto& a, const auto& b ) {
            return a.mKey < b.mKey;
        } );

        for ( core::usize i{}; i < builder.mNodes.size(); ++i ) {
            auto& node{ builder.mNodes[i] };
            auto [it, inserted]{ mNodeIDs.try_emplace( node.mKey, mNextID ) };

            if ( inserted ) {
                mNextID += 3;// node, input, output
            }

            node.mID = it->second;
            eastl::sort( node.mDependencies.begin(), node.mDependencies.end() );

            if ( i >= mNodes.size() || mNodes[i].mKey != node.mKey || mNodes[i].mDependencies != node.mDependencies ) {
                changed = true;
            }
        }

        mNodes = eastl::move( builder.mNodes );

        if ( !mNodes.empty() && eastl::none_of( mNodes.begin(), mNodes.end(), [this]( const auto& node ) {
            return node.mID == mSelectedID;
        } ) ) {
            mSelectedID = mNodes.front().mID;
        }

        mLayoutRequested |= changed;
        mFitRequested |= changed;

        if ( changed ) {
            mFitDelay = 2;
        }
    }

    auto GraphEditor::Matches( const GraphNode& node ) const -> bool {
        MKT_BEGIN_PROFILER();

        if ( !mShowInactive && !node.mEnabled && !node.mExecuted ) {
            return false;
        }

        if ( !mFilter.IsActive() ) {
            return true;
        }

        eastl::string searchable{ node.mDisplayName };

        for ( const auto& resource: node.mResources ) {
            searchable += '\n';
            searchable += resource.mName;
        }

        return mFilter.PassFilter( searchable.c_str() );
    }

    auto GraphEditor::Layout() -> void {
        MKT_BEGIN_PROFILER();
        // Rank the complete DAG by longest dependency path, then order each
        // rank using neighboring ranks to reduce crossings (DOT-style layout).
        ankerl::unordered_dense::map<core::u32, int> depths{};
        ankerl::unordered_dense::map<core::u32, const GraphNode*> visible{};

        for ( const auto& node: mNodes ) {
            if ( Matches( node ) ) {
                visible[node.mID] = &node;
            }
        }

        for ( core::usize round{}; round < visible.size(); ++round ) {
            bool progress{};

            for ( const auto& node: mNodes ) {
                if ( !visible.contains( node.mID ) || depths.contains( node.mID ) ) {
                    continue;
                }

                bool ready{ true };
                int depth{};

                for ( const auto& dependency: node.mDependencies ) {
                    const auto id{ mNodeIDs.find( dependency ) };

                    if ( id == mNodeIDs.end() || !visible.contains( id->second ) ) {
                        continue;
                    }

                    const auto previous{ depths.find( id->second ) };

                    if ( previous == depths.end() ) {
                        ready = false;
                        break;
                    }

                    if ( previous->second + 1 > depth ) {
                        depth = previous->second + 1;
                    }
                }

                if ( ready ) {
                    depths[node.mID] = depth;
                    progress = true;
                }
            }

            if ( !progress ) {
                break;
            }
        }

        eastl::vector<eastl::vector<core::u32>> ranks{};
        ankerl::unordered_dense::map<core::u32, eastl::vector<core::u32>> incoming{}, outgoing{};
        ankerl::unordered_dense::map<core::u32, float> order{};

        for ( const auto& node: mNodes ) {
            if ( !visible.contains( node.mID ) ) {
                continue;
            }
            // Malformed cyclic snapshots remain visible rather than blocking layout.
            const int depth{ depths.contains( node.mID ) ? depths.at( node.mID ) : 0 };

            if ( ranks.size() <= static_cast<core::usize>( depth ) ) {
                ranks.resize( depth + 1 );
            }

            order[node.mID] = static_cast<float>( ranks[depth].size() );
            ranks[depth].push_back( node.mID );
            incoming.try_emplace( node.mID );
            outgoing.try_emplace( node.mID );
        }

        for ( const auto& node: mNodes ) {
            if ( !visible.contains( node.mID ) ) {
                continue;
            }

            for ( const auto& dependency: node.mDependencies ) {
                const auto id{ mNodeIDs.find( dependency ) };

                if ( id == mNodeIDs.end() || !visible.contains( id->second ) ) {
                    continue;
                }

                incoming[node.mID].push_back( id->second );
                outgoing[id->second].push_back( node.mID );
            }
        }

        auto reorder = [&]( auto& rank, const auto& neighbors ) {
            auto barycenter = [&]( core::u32 id ) {
                const auto& adjacent{ neighbors.at( id ) };
                float sum{};

                for ( auto neighbor: adjacent ) {
                    sum += order.at( neighbor );
                }

                return adjacent.empty() ? order.at( id ) : sum / static_cast<float>( adjacent.size() );
            };
            eastl::stable_sort( rank.begin(), rank.end(), [&]( auto a, auto b ) {
                return barycenter( a ) < barycenter( b );
            } );

            for ( core::usize i{}; i < rank.size(); ++i ) {
                order[rank[i]] = static_cast<float>( i );
            }
        };

        for ( int sweep{}; sweep < 8; ++sweep ) {
            for ( auto& rank: ranks ) {
                reorder( rank, incoming );
            }

            for ( auto it{ ranks.rbegin() }; it != ranks.rend(); ++it ) {
                reorder( *it, outgoing );
            }
        }

        float x{};

        for ( const auto& rank: ranks ) {
            float height{};
            float width{};

            for ( auto id: rank ) {
                const auto size{ ed::GetNodeSize( id ) };
                height += ( eastl::max )( size.y, 160.0f ) + 45.0f;
                width = ( eastl::max )( width, size.x );
            }

            float y{ -height * 0.5f };

            for ( auto id: rank ) {
                ed::SetNodePosition( id, { x, y } );
                y += ( eastl::max )( ed::GetNodeSize( id ).y, 160.0f ) + 45.0f;
            }

            x += ( eastl::max )( width, 310.0f ) + 110.0f;
        }

        mLayoutRequested = false;
    }

    auto GraphEditor::DrawNode( const GraphNode& node ) -> void {
        MKT_BEGIN_PROFILER();
        ImVec4 color{ 0.47f, 0.66f, 0.76f, 1.0f };

        if ( node.mType == "Compute" ) {
            color = { 0.68f, 0.56f, 0.73f, 1.0f };
        }

        if ( node.mType == "Transfer" ) {
            color = { 0.49f, 0.72f, 0.56f, 1.0f };
        }

        if ( !node.mEnabled && !node.mExecuted ) {
            color = { 0.47f, 0.47f, 0.50f, 1.0f };
        }

        ImGui::PushID( static_cast<int>( node.mID ) );
        ed::BeginNode( node.mID );
        const auto headerStart{ ImGui::GetCursorScreenPos() };
        ImGui::TextColored( { 0.84f, 0.85f, 0.86f, 1 }, "%s", node.mDisplayName.c_str() );
        ImGui::Dummy( { 310, 0 } );
        ImGui::TextColored( color, "%s", node.mType.c_str() );
        ImGui::TextDisabled( "%s | %s", node.mPolicy.c_str(), node.mExecuted ? "Executed" : ( node.mEnabled ? "Ready" : "Inactive" ) );

        if ( node.mSamples ) {
            ImGui::Text( "CPU last: %.3f ms%s", node.mLastMs, node.mExecuted ? "" : " (previous run)" );
        }
        else {
            ImGui::TextDisabled( "CPU: not sampled" );
        }

        ImGui::Separator();
        DrawGraphSocket( node.mID + 1, ed::PinKind::Input );
        ImGui::SameLine();
        ImGui::TextUnformatted( "Dependencies" );
        ImGui::SameLine( 230.0f );
        ImGui::TextUnformatted( "Then" );
        ImGui::SameLine();
        DrawGraphSocket( node.mID + 2, ed::PinKind::Output );

        if ( mShowResources ) {
            const bool expanded{ ImGui::TreeNode( "Resources", "Resources (%zu)", node.mResources.size() ) };

            if ( ImGui::IsItemToggledOpen() ) {
                mLayoutRequested = true;
            }

            if ( expanded ) {
                core::usize displayed{};

                for ( const auto& resource: node.mResources ) {
                    if ( displayed++ == 6 ) {
                        break;
                    }

                    const auto shortName{ resource.mName.substr( 0, 38 ) };
                    ImGui::TextColored( resource.mWrite ? ImVec4{ 0.76f, 0.65f, 0.46f, 1.0f } : ImVec4{ 0.4f, 0.68f, 0.69f, 1.0f },
                        "%s  %s%s", resource.mRead && resource.mWrite ? "RW" : ( resource.mWrite ? "W " : "R " ), shortName.c_str(), resource.mName.size() > 38 ? "..." : "" );
                }

                if ( node.mResources.size() > 6 ) {
                    ImGui::TextDisabled( "+%zu resources (select to inspect)", node.mResources.size() - 6 );
                }

                ImGui::TreePop();
            }
        }

        ed::EndNode();
        DrawBlueprintHeader( node.mID, headerStart, color );
        ImGui::PopID();
    }

    auto GraphEditor::DrawInspector() -> void {
        MKT_BEGIN_PROFILER();
        const auto found{ eastl::find_if( mNodes.begin(), mNodes.end(), [this]( const auto& node ) {
            return node.mID == mSelectedID;
        } ) };

        if ( found == mNodes.end() ) {
            ImGui::TextWrapped( "Select a pass to inspect all resource usages and timing samples." );
            return;
        }

        const auto& node{ *found };
        ImGui::TextWrapped( "%s", node.mDisplayName.c_str() );
        ImGui::Text( "%s | %s", node.mType.c_str(), node.mPolicy.c_str() );
        ImGui::PushID( static_cast<int>( node.mID ) );

        if ( ImGui::TreeNodeEx( "CPU recording", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth ) ) {
            if ( node.mSamples ) {
                ImGui::Text( "Last  %.4f ms", node.mLastMs );
                ImGui::Text( "Min   %.4f ms", node.mMinMs );
                ImGui::Text( "Max   %.4f ms", node.mMaxMs );
                ImGui::Text( "Samples  %llu", static_cast<unsigned long long>( node.mSamples ) );

                if ( !node.mExecuted ) {
                    ImGui::TextWrapped( "This pass did not run in the captured frame. Times are from its previous executions." );
                }
            }
            else {
                ImGui::TextDisabled( "No executions recorded." );
            }

            ImGui::TreePop();
        }

        if ( ImGui::TreeNodeEx( "Resources", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth ) ) {
            for ( const auto& resource: node.mResources ) {
                ImGui::PushID( static_cast<int>( resource.mHandle ) );

                if ( ImGui::TreeNodeEx( "Resource", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", resource.mName.c_str() ) ) {
                    ImGui::Text( "#%u | %s%s", resource.mHandle, resource.mRead ? "Read " : "", resource.mWrite ? "Write" : "" );
                    ImGui::TextWrapped( "Stage: %s", resource.mStage.c_str() );
                    ImGui::TextWrapped( "%s%s", resource.mDetails.c_str(), resource.mImported ? " | Imported" : "" );
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
        }

        if ( ImGui::TreeNodeEx( "Depends on", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth ) ) {
            for ( const auto& dependency: node.mDependencies ) {
                ImGui::TextWrapped( "%s", dependency.c_str() );
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    auto GraphEditor::Render( bool& open ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !open ) {
            return;
        }

        ImGui::SetNextWindowSize( { 1200, 700 }, ImGuiCond_FirstUseEver );

        if ( ImGui::Begin( mName.c_str(), &open ) ) {
            RenderContents();
        }

        ImGui::End();
    }

    auto GraphEditor::RenderContents() -> void {
        MKT_BEGIN_PROFILER();
        imgui::CheckBox( "Freeze capture", mPaused );
        ImGui::SameLine();

        if ( imgui::CheckBox( "Inactive passes", mShowInactive ) ) {
            mLayoutRequested = mFitRequested = true;
        }

        if ( mFilter.Draw( "Find pass or resource", 240.0f ) ) {
            mLayoutRequested = mFitRequested = true;
        }

        double totalMs{};

        for ( const auto& node: mNodes ) {
            if ( node.mExecuted ) {
                totalMs += node.mLastMs;
            }
        }

        ImGui::SameLine();
        ImGui::Text( "%zu passes | CPU recording %.3f ms", mNodes.size(), totalMs );
        ImGui::TextDisabled( "Timings measure CPU recording, not GPU time." );

        if ( mNodes.empty() ) {
            ImGui::TextDisabled( "No render passes available." );
            return;
        }

        // Both views inspect the same snapshot and share capture and filtering controls.
        // https://github.com/ocornut/imgui/blob/v1.92.9b-docking/imgui_demo.cpp (Tabs, Tables)
        if ( ImGui::BeginTabBar( "PassViews" ) ) {
            if ( ImGui::BeginTabItem( "Graph" ) ) {
                DrawGraph();
                ImGui::EndTabItem();
            }

            if ( ImGui::BeginTabItem( "Pass list" ) ) {
                DrawPassList();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    auto GraphEditor::DrawGraph() -> void {
        MKT_BEGIN_PROFILER();
        NodeEditorScope scope{ mContext };

        if ( ImGui::Button( "Fit graph" ) ) {
            mFitRequested = true;
        }

        ImGui::SameLine();

        if ( ImGui::Button( "Auto layout" ) ) {
            mLayoutRequested = true;
            mFitRequested = true;
        }

        ImGui::SameLine();

        if ( imgui::CheckBox( "Resources", mShowResources ) ) {
            mLayoutRequested = true;
        }

        const float sidebar{ ( eastl::min )( 300.0f, ImGui::GetContentRegionAvail().x * 0.32f ) };
        ImGui::BeginChild( "PassInspector", { sidebar, 0 }, ImGuiChildFlags_Borders );
        DrawInspector();
        ImGui::EndChild();
        ImGui::SameLine();
        const auto previousStyle{ ed::GetStyle() };
        ApplyBlueprintStyle();
        ImGui::PushStyleColor( ImGuiCol_Text, { 0.78f, 0.79f, 0.8f, 1 } );
        ImGui::PushStyleColor( ImGuiCol_TextDisabled, { 0.58f, 0.6f, 0.62f, 1 } );
        ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 8, 6 } );
        ed::Begin( "PassGraphCanvas" );

        for ( const auto& node: mNodes ) {
            if ( Matches( node ) ) {
                DrawNode( node );
            }
        }

        if ( mLayoutRequested ) {
            Layout();
        }

        for ( const auto& node: mNodes ) {
            if ( !Matches( node ) ) {
                continue;
            }

            for ( const auto& dep: node.mDependencies ) {
                const auto source{ eastl::find_if( mNodes.begin(), mNodes.end(), [&dep]( const auto& n ) {
                    return n.mKey == dep;
                } ) };

                if ( source == mNodes.end() || !Matches( *source ) ) {
                    continue;
                }

                const auto link{ ( static_cast<uintptr_t>( source->mID ) << 32 ) | node.mID };
                ed::Link( link, source->mID + 2, node.mID + 1, { 0.4f, 0.68f, 0.69f, 1.0f }, 1.5f );
            }
        }

        ed::NodeId selection{};

        if ( ed::GetSelectedNodes( &selection, 1 ) ) {
            mSelectedID = static_cast<core::u32>( selection.Get() );
        }

        ed::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor( 2 );
        ed::GetStyle() = previousStyle;
        // Give newly created canvases two frames to establish their size and view.
        if ( mFitRequested ) {
            if ( mFitDelay > 0 ) {
                --mFitDelay;
            }
            else {
                ed::NavigateToContent( 0.0f );
                mFitRequested = false;
            }
        }
    }

    auto GraphEditor::DrawPassList() -> void {
        MKT_BEGIN_PROFILER();
        const float scale{ ImGui::GetFontSize() / 17.0f };
        const ImGuiTableFlags flags{ ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingFixedFit };

        if ( !ImGui::BeginTable( "PassResourceList", 5, flags, ImGui::GetContentRegionAvail() ) ) {
            return;
        }

        // Keep names and headers visible while browsing the complete resource list.
        ImGui::TableSetupColumn( "Pass", ImGuiTableColumnFlags_WidthFixed, 240.0f * scale );
        ImGui::TableSetupColumn( "Resource", ImGuiTableColumnFlags_WidthFixed, 240.0f * scale );
        ImGui::TableSetupColumn( "Access", ImGuiTableColumnFlags_WidthFixed, 100.0f * scale );
        ImGui::TableSetupColumn( "Stages", ImGuiTableColumnFlags_WidthFixed, 190.0f * scale );
        ImGui::TableSetupColumn( "Details", ImGuiTableColumnFlags_WidthFixed, 300.0f * scale );
        ImGui::TableSetupScrollFreeze( 1, 1 );
        ImGui::TableHeadersRow();

        for ( const auto& node: mNodes ) {
            if ( !Matches( node ) ) {
                continue;
            }

            // Emit every usage directly; passes without resources still get a row.
            const core::usize rowCount{ node.mResources.empty() ? 1 : node.mResources.size() };

            for ( core::usize index{}; index < rowCount; ++index ) {
                ImGui::TableNextRow();

                if ( ImGui::TableSetColumnIndex( 0 ) && index == 0 ) {
                    ImGui::TextWrapped( "%s", node.mDisplayName.c_str() );
                    ImGui::TextDisabled( "%s | %s", node.mType.c_str(), node.mPolicy.c_str() );
                    ImGui::TextDisabled( "%s", node.mExecuted ? "Executed" : ( node.mEnabled ? "Ready" : "Inactive" ) );
                }

                if ( node.mResources.empty() ) {
                    if ( ImGui::TableSetColumnIndex( 1 ) ) {
                        ImGui::TextDisabled( "No resources" );
                    }

                    continue;
                }

                const auto& resource{ node.mResources[index] };

                if ( ImGui::TableSetColumnIndex( 1 ) ) {
                    ImGui::TextWrapped( "%s", resource.mName.c_str() );
                    ImGui::TextDisabled( "#%u%s", resource.mHandle, resource.mImported ? " | Imported" : "" );
                }

                if ( ImGui::TableSetColumnIndex( 2 ) ) {
                    ImGui::TextUnformatted( resource.mRead && resource.mWrite ? "Read/write" :
                        ( resource.mWrite ? "Write" : ( resource.mRead ? "Read" : "State only" ) ) );
                }

                if ( ImGui::TableSetColumnIndex( 3 ) ) {
                    ImGui::TextWrapped( "%s", resource.mStage.empty() ? "-" : resource.mStage.c_str() );
                }

                if ( ImGui::TableSetColumnIndex( 4 ) ) {
                    ImGui::TextWrapped( "%s", resource.mDetails.c_str() );
                }
            }
        }

        ImGui::EndTable();
    }

    auto GraphEditor::IsPaused() const -> bool {
        MKT_BEGIN_PROFILER();
        return mPaused;
    }

    auto GraphEditor::GetName() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    NodeEditorContext::~NodeEditorContext() {
        MKT_BEGIN_PROFILER();

        if ( mContext ) {
            ax::NodeEditor::DestroyEditor( mContext );
        }
    }

    auto NodeEditorContext::Reset() -> void {
        if ( mContext ) {
            ax::NodeEditor::DestroyEditor( mContext );
            mContext = nullptr;
        }
    }

    auto NodeEditorContext::Get() -> ax::NodeEditor::EditorContext* {
        MKT_BEGIN_PROFILER();

        if ( !mContext ) {
            ax::NodeEditor::Config config{};
            config.SettingsFile = nullptr;
            config.EnableSmoothZoom = true;
            mContext = ax::NodeEditor::CreateEditor( &config );
        }

        return mContext;
    }

    NodeEditorScope::NodeEditorScope( NodeEditorContext& context )
        : mPrevious{ ax::NodeEditor::GetCurrentEditor() } {
        MKT_BEGIN_PROFILER();
        ax::NodeEditor::SetCurrentEditor( context.Get() );
    }

    NodeEditorScope::~NodeEditorScope() {
        MKT_BEGIN_PROFILER();
        ax::NodeEditor::SetCurrentEditor( mPrevious );
    }
}
