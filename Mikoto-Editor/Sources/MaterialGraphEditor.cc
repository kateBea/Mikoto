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

#include <cfloat>
#include <cstring>
#include <filesystem>

#include <EASTL/array.h>
#include <EASTL/algorithm.h>

#include <Core/LocalizationService.hh>

#include <Filesystem/Path.hh>

#include <Assets/AssetRegistryService.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>

#include <Application/MaterialGraphEditor.hh>

namespace mikoto::editor {
    using namespace core;
    using namespace material;
    namespace ed = ax::NodeEditor;

    static auto HeaderColor( material::MaterialNodeType type ) -> ImVec4 {
        if ( type == material::MaterialNodeType::eTexture2D ) {
            return { .30f, .20f, .40f, 1 };
        }

        if ( type == material::MaterialNodeType::eSurfaceOutput || type == material::MaterialNodeType::eParticleOutput || type == material::MaterialNodeType::ePostProcessOutput ) {
            return { .16f, .33f, .23f, 1 };
        }

        if ( type >= material::MaterialNodeType::eBool && type <= material::MaterialNodeType::eColor4 ) {
            return { .18f, .29f, .40f, 1 };
        }

        return { .22f, .28f, .30f, 1 };
    }

    static auto EditString( core::cstr label, eastl::string& value ) -> bool {
        // Fixed editor buffer; the stored string remains dynamically sized and untruncated unless edited.
        eastl::array<char, 2048> buffer{};
        std::memcpy( buffer.data(), value.data(), eastl::min( value.size(), buffer.size() - 1 ) );

        if ( ImGui::InputText( label, buffer.data(), buffer.size() ) ) {
            value = buffer.data();
            return true;
        }

        return false;
    }

    static auto PinColor( material::MaterialValueType type ) -> ImVec4 {
        using V = material::MaterialValueType;

        switch ( type ) {
            case V::eBool:
                return { .9f, .16f, .18f, 1 };
            case V::eInt:
                return { .22f, .8f, .68f, 1 };
            case V::eFloat:
                return { .55f, .86f, .2f, 1 };
            case V::eFloat2:
            case V::eFloat3:
            case V::eFloat4:
                return { .95f, .72f, .2f, 1 };
            case V::eColor3:
            case V::eColor4:
                return { .95f, .45f, .2f, 1 };
            case V::eTexture2D:
            case V::eTextureCube:
                return { .15f, .65f, .95f, 1 };
            case V::eSurface:
                return { .75f, .4f, .92f, 1 };
            default:
                return { .7f, .7f, .75f, 1 };
        }
    }

    // Display names are independent of the saved graph's socket names.
    static auto PinLabel( const material::MaterialPin& pin ) -> eastl::string {
        auto label{ pin.mName };
        bool capitalize{ true };

        for ( auto& character: label ) {
            if ( character == '_' ) {
                character = ' ';
            }

            if ( capitalize && character >= 'a' && character <= 'z' ) {
                character = static_cast<char>( character - 'a' + 'A' );
            }

            capitalize = character == ' ';
        }

        return label;
    }

    static auto ValueWidth( material::MaterialValueType type ) -> core::f32 {
        using V = material::MaterialValueType;
        const auto& style{ ImGui::GetStyle() };
        const core::f32 component{ ImGui::CalcTextSize( "-000.000" ).x + style.FramePadding.x * 2 };
        core::i32 count{ 1 };

        if ( type == V::eFloat2 ) {
            count = 2;
        }

        if ( type == V::eFloat3 || type == V::eColor3 ) {
            count = 3;
        }

        if ( type == V::eFloat4 || type == V::eColor4 ) {
            count = 4;
        }

        const bool color{ type == V::eColor3 || type == V::eColor4 };
        return component * count + style.ItemInnerSpacing.x * ( count - 1 ) +
               ( color ? ImGui::GetFrameHeight() + style.ItemInnerSpacing.x : 0.0f );
    }

    static auto Pin( const material::MaterialPin& pin, bool connected ) -> void {
        const bool input{ pin.mDirection == material::PinDirection::eInput };
        ed::BeginPin( pin.mID * 8 + 2, input ? ed::PinKind::Input : ed::PinKind::Output );

        if ( !input ) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( PinLabel( pin ).c_str() );
            ImGui::SameLine();
        }

        const auto shape{ pin.mType == material::MaterialValueType::eTexture2D || pin.mType == material::MaterialValueType::eTextureCube ? imgui::BlueprintPinShape::eObject : imgui::BlueprintPinShape::eData };
        imgui::DrawBlueprintPinIcon( shape, connected, PinColor( pin.mType ) );
        const auto minimum{ ImGui::GetItemRectMin() };
        const auto maximum{ ImGui::GetItemRectMax() };
        const ImVec2 center{ ( minimum.x + maximum.x ) * .5f, ( minimum.y + maximum.y ) * .5f };
        ed::PinPivotRect( center, center );

        if ( input ) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted( PinLabel( pin ).c_str() );
        }

        ed::EndPin();
    }

    static auto EditValue( core::cstr label, material::MaterialValueType type, core::float4& value ) -> bool {
        using V = material::MaterialValueType;

        switch ( type ) {
            case V::eBool: {
                bool b{ value.x != 0 };

                if ( imgui::CheckBox( label, b ) ) {
                    value.x = b ? 1.0f : 0.0f;
                    return true;
                }

                return false;
            }

            case V::eInt:
            case V::eFloat:
                return ImGui::DragFloat( label, &value.x, type == V::eInt ? 1.0f : .01f );
            case V::eFloat2:
                return ImGui::DragFloat2( label, &value.x, .01f );
            case V::eFloat3:
                return ImGui::DragFloat3( label, &value.x, .01f );
            case V::eFloat4:
                return ImGui::DragFloat4( label, &value.x, .01f );
            case V::eColor3:
            case V::eColor4: {
                // Inline controls belong to the node; the picker popup must
                // run outside the node-editor canvas's input/coordinate scope.
                ImGui::PushID( label );
                const bool alpha{ type == V::eColor4 };
                const float swatch{ ImGui::GetFrameHeight() };
                const float width{ ImGui::CalcItemWidth() };
                ImGui::SetNextItemWidth( eastl::max( 1.0f, width - swatch - ImGui::GetStyle().ItemSpacing.x ) );
                const auto flags{ ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoPicker |
                                  ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoOptions };
                bool changed{ alpha ? ImGui::ColorEdit4( "##Components", &value.x, flags ) : ImGui::ColorEdit3( "##Components", &value.x, flags ) };
                ImGui::SameLine();
                const bool open{ ImGui::ColorButton( "##Swatch",
                    ImVec4{ value.x, value.y, value.z, alpha ? value.w : 1.0f },
                    alpha ? ImGuiColorEditFlags_None : ImGuiColorEditFlags_NoAlpha,
                    ImVec2{ swatch, swatch } ) };
                ed::Suspend();

                if ( open ) {
                    ImGui::OpenPopup( "Color picker" );
                }

                if ( ImGui::BeginPopup( "Color picker" ) ) {
                    ImGui::SetNextItemWidth( 280.0f );
                    changed |= alpha ? ImGui::ColorPicker4( "##Picker", &value.x, ImGuiColorEditFlags_Float ) : ImGui::ColorPicker3( "##Picker", &value.x, ImGuiColorEditFlags_Float );
                    ImGui::EndPopup();
                }

                ed::Resume();
                ImGui::PopID();
                return changed;
            }

            default:
                return false;
        }
    }


    auto MaterialGraphEditor::Reset() -> void {
        mContext.Reset();
        mRestoreLayout = true;
        mRenamingNode = {};
        mTextures.clear();
        mStatus.clear();
        mGeneratedYaml.clear();
        mShowGeneratedYaml = false;
    }

    auto MaterialGraphEditor::Render( material::MaterialGraph& graph, core::cstr surfaceLabel ) -> bool {
        bool changed{};

        if ( graph.GetNodes().empty() ) {
            graph.InitializeDefault();
            changed = true;
        }

        imgui::NodeEditorScope scope{ mContext };
        const auto previousStyle{ ed::GetStyle() };
        imgui::ApplyBlueprintStyle();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_graph_editor_add_node", "Add node" ).c_str() ) ) {
            ImGui::OpenPopup( "Material nodes" );
        }

        if ( ImGui::BeginPopup( "Material nodes" ) ) {
            const auto add{ [&]( material::MaterialNodeType type, material::MaterialValueType valueType ) {
                const auto id{ graph.AddNode( type, valueType ) };

                if ( id != material::kInvalidNodeID ) {
                    const auto point{ ed::ScreenToCanvas( ImGui::GetMousePos() ) };
                    graph.FindNode( id )->mPosition = { point.x, point.y };
                    ed::SetNodePosition( id * 8 + 1, point );
                    changed = true;
                }
                else {
                    mStatus = MKT_LOC( "material_graph_editor_only_one_output_node_is_allowed" ).c_str();
                }
            } };

            for ( const auto type: graph.GetRules().mAllowedNodes ) {
                if ( ( type >= material::MaterialNodeType::eAdd && type <= material::MaterialNodeType::eLerp ) || type == material::MaterialNodeType::eTexture2D ) {
                    if ( ImGui::BeginMenu( material::MaterialGraph::GetNodeName( type ) ) ) {
                        constexpr material::MaterialValueType types[]{ material::MaterialValueType::eFloat, material::MaterialValueType::eFloat2, material::MaterialValueType::eFloat3, material::MaterialValueType::eFloat4, material::MaterialValueType::eColor3, material::MaterialValueType::eColor4 };
                        constexpr core::cstr labels[]{ "Float", "Float2", "Float3", "Float4", "Color3", "Color4" };

                        for ( core::usize i{}; i < 6; ++i ) {
                            if ( ImGui::MenuItem( labels[i] ) ) {
                                add( type, types[i] );
                            }
                        }

                        ImGui::EndMenu();
                    }
                }
                else if ( ImGui::MenuItem( material::MaterialGraph::GetNodeName( type ) ) ) {
                    add( type, material::MaterialValueType::eUnknown );
                }
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_graph_editor_comment_group", "Comment / group" ).c_str() ) ) {
            auto point{ ed::ScreenToCanvas( ImGui::GetMousePos() ) };
            ImVec2 size{ 400, 240 };
            eastl::vector<ed::NodeId> selected( ed::GetSelectedObjectCount() );
            const auto count{ ed::GetSelectedNodes( selected.data(), static_cast<int>( selected.size() ) ) };

            if ( count > 0 ) {
                ImVec2 minimum{ FLT_MAX, FLT_MAX }, maximum{ -FLT_MAX, -FLT_MAX };

                for ( int i{}; i < count; ++i ) {
                    const auto position{ ed::GetNodePosition( selected[i] ) };
                    const auto extent{ ed::GetNodeSize( selected[i] ) };
                    minimum.x = eastl::min( minimum.x, position.x );
                    minimum.y = eastl::min( minimum.y, position.y );
                    maximum.x = eastl::max( maximum.x, position.x + extent.x );
                    maximum.y = eastl::max( maximum.y, position.y + extent.y );
                }

                point = { minimum.x - 30, minimum.y - 70 };
                size = { maximum.x - minimum.x + 60, maximum.y - minimum.y + 80 };
            }

            const auto id{ graph.AddComment( "Group", { point.x, point.y } ) };
            graph.GetComments().back().mSize = { size.x, size.y };
            ed::SetNodePosition( id * 8 + 1, point );
            ed::SetGroupSize( id * 8 + 1, size );
            changed = true;
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_graph_editor_generate_yaml", "Generate YAML" ).c_str() ) ) {
            const auto result{ asset::AssetRegistryService::Get()->CompileMaterialGraph( graph ) };
            mGeneratedYaml = result.mSource;
            mStatus = result.mSuccess ? MKT_LOC( "material_graph_editor_yaml_generated" ).c_str() : MKT_LOC( "material_graph_editor_graph_validation_failed_see_console" ).c_str();
            mShowGeneratedYaml = result.mSuccess;
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "material_graph_editor_frame_all", "Frame all" ).c_str() ) ) {
            ed::NavigateToContent();
        }

        ImGui::TextWrapped( "%s", mStatus.empty() ? MKT_LOC( "material_graph_editor_graph_edits_are_saved_with_the_material_rendering_still_uses" ).c_str() : mStatus.c_str() );

        if ( mShowGeneratedYaml ) {
            ImGui::SetNextWindowSize( ImVec2{ 720, 560 }, ImGuiCond_FirstUseEver );
            ImGui::SetNextWindowSizeConstraints( ImVec2{ 360, 200 }, ImVec2{ FLT_MAX, FLT_MAX } );

            if ( imgui::BeginWindow( imgui::widget::LocalizedLabel( "material_graph_editor_generate_yaml", "MaterialGraphYaml" ).c_str(),
                     &mShowGeneratedYaml, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse ) ) {
                if ( ImGui::Button( imgui::widget::LocalizedLabel( "inspector_copy", "Copy" ).c_str() ) ) {
                    ImGui::SetClipboardText( mGeneratedYaml.c_str() );
                }

                ImGui::InputTextMultiline( "##GeneratedYaml", mGeneratedYaml.data(), mGeneratedYaml.size() + 1,
                    ImVec2{ -1, -1 }, ImGuiInputTextFlags_ReadOnly );
            }

            ImGui::End();
        }

        ed::Begin( "Material authoring" );

        for ( auto& comment: graph.GetComments() ) {
            const ed::NodeId id{ comment.mID * 8 + 1 };

            if ( mRestoreLayout ) {
                ed::SetNodePosition( id, { comment.mPosition.x, comment.mPosition.y } );
                ed::SetGroupSize( id, { comment.mSize.x, comment.mSize.y } );
            }

            ed::BeginNode( id );
            ImGui::PushID( static_cast<core::i32>( comment.mID ) );
            ImGui::SetNextItemWidth( 220 );
            changed |= EditString( "##Group name", comment.mName );
            ed::Group( { comment.mSize.x, comment.mSize.y } );
            const auto groupSize{ ImGui::GetItemRectSize() };
            const core::float2 storedSize{ groupSize.x, groupSize.y };
            changed |= !mRestoreLayout && comment.mSize != storedSize;
            comment.mSize = storedSize;
            ImGui::PopID();
            ed::EndNode();
            const auto position{ ed::GetNodePosition( id ) };

            if ( !mRestoreLayout && ( comment.mPosition.x != position.x || comment.mPosition.y != position.y ) ) {
                changed = true;
            }

            comment.mPosition = { position.x, position.y };
        }

        for ( auto& node: graph.GetNodes() ) {
            const ed::NodeId id{ node.mID * 8 + 1 };

            if ( mRestoreLayout ) {
                ed::SetNodePosition( id, { node.mPosition.x, node.mPosition.y } );
            }

            const auto previewState{ mTextures.find( node.mID ) };
            const bool hasError{ previewState != mTextures.end() && !previewState->second.mError.empty() };
            ed::PushStyleColor( ed::StyleColor_NodeBg, { .20f, .20f, .21f, 1 } );
            ed::PushStyleColor( ed::StyleColor_NodeBorder, hasError ? ImVec4{ .85f, .25f, .23f, 1 } : ImVec4{ .30f, .32f, .35f, 1 } );
            ed::PushStyleVar( ed::StyleVar_NodeRounding, 10.0f );
            ed::PushStyleVar( ed::StyleVar_NodePadding, ImVec4{ 12, 8, 12, 10 } );
            ed::PushStyleVar( ed::StyleVar_NodeBorderWidth, hasError ? 2.0f : 1.0f );
            ed::BeginNode( id );
            ImGui::PushID( static_cast<core::i32>( node.mID ) );
            ImGui::PushItemWidth( 210 );
            const auto headerStart{ ImGui::GetCursorScreenPos() };
            const auto typeName{ material::MaterialGraph::GetNodeName( node.mType ) };
            const auto title{ node.mType == material::MaterialNodeType::eStandardSurface && surfaceLabel ? surfaceLabel : typeName };

            if ( mRenamingNode == node.mID ) {
                changed |= EditString( "##Node name", node.mName );

                if ( ImGui::IsItemDeactivated() || ImGui::IsKeyPressed( ImGuiKey_Enter ) ) {
                    mRenamingNode = {};
                }
            }
            else {
                ImGui::TextUnformatted( node.mName == typeName ? title : node.mName.c_str() );

                if ( ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                    mRenamingNode = node.mID;
                }
            }

            ImGui::Dummy( { 210, 6 } );

            if ( node.mType == material::MaterialNodeType::eInt ) {
                changed |= ImGui::InputInt( imgui::widget::LocalizedLabel( "material_graph_editor_value", "Value" ).c_str(), &node.mIntegerValue );
            }
            else if ( node.mType == material::MaterialNodeType::eBool ) {
                changed |= imgui::CheckBox( imgui::widget::LocalizedLabel( "material_graph_editor_value", "Value" ).c_str(), node.mBooleanValue );
            }
            else if ( node.mType >= material::MaterialNodeType::eFloat && node.mType <= material::MaterialNodeType::eColor4 ) {
                changed |= EditValue( MKT_LOC( "material_graph_editor_value" ).c_str(), node.mOutputs.front().mType, node.mValue );
            }

            if ( node.mType == material::MaterialNodeType::eTexture2D ) {
                if ( EditString( MKT_LOC( "material_graph_editor_image_path" ).c_str(), node.mTexturePath ) ) {
                    mTextures.erase( node.mID );
                    changed = true;
                }

                if ( !node.mTexturePath.empty() && !ImGui::IsItemActive() ) {
                    auto [found, inserted]{ mTextures.try_emplace( node.mID ) };
                    auto& preview{ found->second };

                    if ( inserted ) {
                        // Losing focus commits a typed path. Missing files and metadata races
                        // are ordinary editor errors; never unwind through an open ImGui node.
                        try {
                            const filesystem::Path path{ node.mTexturePath };
                            std::error_code error{};

                            if ( !std::filesystem::is_regular_file( path.GetPathTyped<std::filesystem::path>(), error ) ) {
                                preview.mError = MKT_LOC( "material_graph_editor_select_an_existing_image_file" ).c_str();
                            }
                            else {
                                preview.mRequest = asset::AssetRegistryService::Get()->Request<asset::Texture>( asset::TextureLoadDescription{ .mPath = path, .mPriority = asset::LoadPriority::eBackground } );
                            }
                        } catch ( const std::exception& error ) {
                            preview.mError = error.what();
                        }
                    }

                    auto texture{ preview.mRequest.GetIfReady() };

                    if ( texture ) {
                        const core::f32 ratio{ static_cast<core::f32>( texture->GetWidth() ) / eastl::max( 1u, texture->GetHeight() ) };
                        ImGui::Image( imgui::ImGuiService::Get()->GetTextureID( texture->GetResource() ), { ratio >= 1 ? 150.0f : 150.0f * ratio, ratio >= 1 ? 150.0f / ratio : 150.0f } );
                    }
                    else {
                        if ( preview.mError.empty() ) {
                            preview.mError = preview.mRequest.GetError();
                        }

                        ImGui::PushTextWrapPos( ImGui::GetCursorPosX() + 210.0f );

                        if ( preview.mError.empty() ) {
                            ImGui::TextDisabled( MKT_LOC( "material_graph_editor_loading_image" ).c_str() );
                        }
                        else {
                            ImGui::TextColored( { 1.0f, .4f, .36f, 1 }, MKT_LOC( "material_graph_editor_image_unavailable" ).c_str() );
                            ImGui::TextWrapped( "%s", preview.mError.c_str() );
                        }

                        ImGui::PopTextWrapPos();
                        // Cache failures rather than hitting the filesystem every frame.
                        // Editing the path or explicitly retrying releases this failed entry.
                        if ( !preview.mError.empty() && ImGui::SmallButton( imgui::widget::LocalizedLabel( "material_graph_editor_retry_image", "Retry image" ).c_str() ) ) {
                            mTextures.erase( node.mID );
                        }
                    }
                }
            }
            // Measure once per node so every row uses the same label/value
            // columns. Reserve them even when a connection hides its editor.
            const auto& style{ ImGui::GetStyle() };
            const core::f32 iconWidth{ ImGui::GetTextLineHeight() + 4.0f };
            core::f32 labelWidth{};
            core::f32 valueWidth{};
            core::f32 outputWidth{};

            for ( const auto& pin: node.mInputs ) {
                labelWidth = eastl::max( labelWidth, ImGui::CalcTextSize( PinLabel( pin ).c_str() ).x );

                if ( !pin.mRequired ) {
                    valueWidth = eastl::max( valueWidth, ValueWidth( pin.mType ) );
                }
            }

            for ( const auto& pin: node.mOutputs ) {
                outputWidth = eastl::max( outputWidth, ImGui::CalcTextSize( PinLabel( pin ).c_str() ).x + iconWidth + style.ItemSpacing.x );
            }

            const core::f32 inputX{ ImGui::GetCursorPosX() };
            const core::f32 valueX{ inputX + iconWidth + style.ItemSpacing.x + labelWidth + style.ItemSpacing.x };
            const core::f32 inputWidth{ node.mInputs.empty() ? 0.0f : valueX - inputX + valueWidth };
            ImGui::BeginGroup();

            for ( auto& pin: node.mInputs ) {
                const bool connected{ eastl::any_of( graph.GetLinks().begin(), graph.GetLinks().end(), [&]( const auto& l ) {
                    return l.mDestination == pin.mID;
                } ) };
                Pin( pin, connected );

                if ( !connected && !pin.mRequired ) {
                    // SameLine offsets include the current group offset; valueX is
                    // already window-local. Set it explicitly to avoid movement-dependent widths.
                    ImGui::SameLine();
                    ImGui::SetCursorPosX( valueX );
                    ImGui::PushID( static_cast<core::i32>( pin.mID ) );
                    ImGui::PushItemWidth( ValueWidth( pin.mType ) );
                    changed |= EditValue( "##Default", pin.mType, pin.mValue );
                    ImGui::PopItemWidth();
                    ImGui::PopID();
                }
            }

            ImGui::EndGroup();

            if ( !node.mInputs.empty() && !node.mOutputs.empty() ) {
                ImGui::SameLine();
                ImGui::SetCursorPosX( inputX + inputWidth + ImGui::GetFontSize() * 1.5f );
            }

            ImGui::BeginGroup();
            const core::f32 outputX{ ImGui::GetCursorPosX() };

            for ( const auto& pin: node.mOutputs ) {
                const core::f32 width{ ImGui::CalcTextSize( PinLabel( pin ).c_str() ).x + iconWidth + style.ItemSpacing.x };
                ImGui::SetCursorPosX( outputX + outputWidth - width );
                const bool connected{ eastl::any_of( graph.GetLinks().begin(), graph.GetLinks().end(), [&]( const auto& l ) {
                    return l.mSource == pin.mID;
                } ) };
                Pin( pin, connected );
            }

            ImGui::EndGroup();
            ImGui::PopItemWidth();
            ImGui::PopID();
            ed::EndNode();
            imgui::DrawBlueprintHeader( id, headerStart, hasError ? ImVec4{ .55f, .18f, .18f, 1 } : HeaderColor( node.mType ) );
            ed::PopStyleVar( 3 );
            ed::PopStyleColor( 2 );
            const auto position{ ed::GetNodePosition( id ) };

            if ( !mRestoreLayout && ( node.mPosition.x != position.x || node.mPosition.y != position.y ) ) {
                changed = true;
            }

            node.mPosition = { position.x, position.y };
        }

        for ( const auto& link: graph.GetLinks() ) {
            const auto* source{ graph.FindPin( link.mSource ) };
            ed::Link( link.mID * 8 + 3, link.mSource * 8 + 2, link.mDestination * 8 + 2,
                source ? PinColor( source->mType ) : ImVec4{ 1, 1, 1, 1 }, 2.0f );
        }

        if ( ed::BeginCreate() ) {
            ed::PinId a{}, b{};

            if ( ed::QueryNewLink( &a, &b ) && a && b ) {
                material::PinID source{ a.Get() / 8 }, destination{ b.Get() / 8 };

                if ( const auto* pin{ graph.FindPin( source ) }; pin && pin->mDirection == material::PinDirection::eInput ) {
                    eastl::swap( source, destination );
                }

                if ( graph.CanConnect( source, destination ) ) {
                    if ( ed::AcceptNewItem() ) {
                        graph.Connect( source, destination );
                        changed = true;
                    }
                }
                else {
                    ed::RejectNewItem( { 1, .2f, .2f, 1 } );
                }
            }
        }

        ed::EndCreate();

        if ( ed::BeginDelete() ) {
            ed::LinkId link{};

            while ( ed::QueryDeletedLink( &link ) ) {
                if ( ed::AcceptDeletedItem() ) {
                    graph.Disconnect( link.Get() / 8 );
                    changed = true;
                }
            }

            ed::NodeId node{};

            while ( ed::QueryDeletedNode( &node ) ) {
                if ( ed::AcceptDeletedItem() ) {
                    const auto id{ node.Get() / 8 };
                    graph.RemoveNode( id );
                    graph.RemoveComment( id );
                    mTextures.erase( id );
                    changed = true;
                }
            }
        }

        ed::EndDelete();
        ed::End();
        ed::GetStyle() = previousStyle;

        if ( mRestoreLayout ) {
            ed::ClearSelection();
            ed::NavigateToContent();
            mRestoreLayout = false;
        }

        return changed;
    }
}// namespace mikoto::editor
