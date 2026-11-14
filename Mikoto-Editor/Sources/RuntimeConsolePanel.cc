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

#include <imgui.h>

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/RuntimeConsole.hh>
#include <Core/LocalizationService.hh>

#include <Memory/Allocator.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiService.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Panels/RuntimeConsolePanel.hh>

#include <Layers/EditorLayer.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;


    struct LogAppearance {
        core::cstr mIcon{};
        ImVec4 mColor{};
    };

    static auto GetLogAppearance( core::ConsoleLogLevel level ) -> LogAppearance {
        switch ( level ) {
            case core::ConsoleLogLevel::eError:
                return { ICON_MD_ERROR, { 1.0f, 0.3f, 0.3f, 1.0f } };
            case core::ConsoleLogLevel::eWarning:
                return { ICON_MD_WARNING, { 1.0f, 0.8f, 0.3f, 1.0f } };
            case core::ConsoleLogLevel::eDebug:
                return { ICON_MD_BUG_REPORT, { 0.5f, 0.8f, 1.0f, 1.0f } };
            case core::ConsoleLogLevel::eInfo:
                return { ICON_MD_INFO, { 0.45f, 0.85f, 0.65f, 1.0f } };
        }

        return { ICON_MD_INFO, ImGui::GetStyleColorVec4( ImGuiCol_Text ) };
    }

    static auto DrawSeverityFilter( core::ConsoleLogLevel level, const char* key, bool& enabled ) -> bool {
        const auto appearance{ GetLogAppearance( level ) };
        const auto label{ string::Format( "{} {}###{}", appearance.mIcon,
            MKT_LOC( key ), key ) };
        ImGui::PushStyleColor( ImGuiCol_Text, appearance.mColor );
        const bool changed{ imgui::CheckBox( label.c_str(), enabled ) };
        ImGui::PopStyleColor();
        imgui::SetCursorHandOnLastItemHovered();
        return changed;
    }

    static auto LogIconWidth() -> float {
        return ImGui::GetFontSize() + 8.0f;
    }


    RuntimeConsolePanel::RuntimeConsolePanel( const RuntimeConsolePanelCreateInfo& info )
        : Panel{ "Console", "Panel", info.mGroup, info.mIsToggleable }, mState{ info.mState } {
        MKT_BEGIN_PROFILER();
        mPanelHeaderName = imgui::widget::MakeIconTitle( ICON_MD_TERMINAL, mPanelName );
    }

    auto RuntimeConsolePanel::MeasureLog( CachedLog& log ) -> void {
        log.mLines.clear();
        log.mWidth = 0.0f;
        const auto& text{ log.mEntry.mText };
        core::usize offset{};

        while ( offset <= text.size() ) {
            const auto newline{ text.find( '\n', offset ) };
            const auto end{ newline == eastl::string::npos ? text.size() : newline };
            auto length{ end - offset };

            if ( length && text[offset + length - 1] == '\r' ) {
                --length;
            }

            log.mLines.push_back( { offset, length } );
            const auto* begin{ text.data() + offset };
            log.mWidth = eastl::max( log.mWidth, ImGui::CalcTextSize( begin, begin + length ).x );

            if ( newline == eastl::string::npos ) {
                break;
            }

            offset = newline + 1;
        }

        log.mWidth += LogIconWidth();
    }

    auto RuntimeConsolePanel::DrawLogLine( const CachedLog& log, const DisplayLine& line ) -> void {
        const auto& entry{ log.mEntry };
        const auto appearance{ GetLogAppearance( entry.mLevel ) };
        const ImU32 severityColor{ ImGui::GetColorU32( appearance.mColor ) };
        auto* draw{ ImGui::GetWindowDrawList() };
        ImVec2 position{ ImGui::GetCursorScreenPos() };
        const auto* begin{ entry.mText.data() + line.mOffset };
        const auto* end{ begin + line.mLength };

        if ( line.mOffset == 0 ) {
            // The console uses a code font; icons live in the default UI font.
            draw->AddText( ImGui::GetIO().Fonts->Fonts[0], ImGui::GetFontSize(), position, severityColor, appearance.mIcon );
        }

        position.x += LogIconWidth();

        if ( line.mOffset == 0 ) {
            const auto bracket{ entry.mText.find( ']' ) };

            if ( bracket != eastl::string::npos && bracket < line.mLength ) {
                const auto* prefixEnd{ begin + bracket + 1 };
                draw->AddText( position, severityColor, begin, prefixEnd );
                position.x += ImGui::CalcTextSize( begin, prefixEnd ).x;
                begin = prefixEnd;
            }
        }

        draw->AddText( position, ImGui::GetColorU32( ImGuiCol_Text ), begin, end );

        // One item per visual line keeps clipping and right-click actions working
        // across the icon, label and body, including empty continuation lines.
        ImGui::Dummy( { log.mWidth, ImGui::GetTextLineHeight() } );
        ImGui::PushID( static_cast<int>( entry.mID >> 32 ) );
        ImGui::PushID( static_cast<int>( entry.mID ) );
        ImGui::PushID( static_cast<int>( line.mOffset ) );

        if ( ImGui::BeginPopupContextItem( "LogEntry" ) ) {
            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "runtime_console_copy_full_message", "Copy full message" ).c_str() ) ) {
                ImGui::SetClipboardText( entry.mText.c_str() );
            }

            if ( ImGui::MenuItem( imgui::widget::LocalizedLabel( "runtime_console_copy_message_id", "Copy message ID" ).c_str() ) ) {
                ImGui::SetClipboardText( string::Format( "#{:06}", entry.mID ).c_str() );
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
        ImGui::PopID();
        ImGui::PopID();
    }

    auto RuntimeConsolePanel::AppendDisplayLines( const CachedLog& log ) -> void {
        bool visible{};

        switch ( log.mEntry.mLevel ) {
            case core::ConsoleLogLevel::eInfo:
                visible = mShowInfo;
                break;
            case core::ConsoleLogLevel::eWarning:
                visible = mShowWarning;
                break;
            case core::ConsoleLogLevel::eError:
                visible = mShowError;
                break;
            case core::ConsoleLogLevel::eDebug:
                visible = mShowDebug;
                break;
        }

        if ( !visible || !mTextFilter.PassFilter( log.mEntry.mText.c_str() ) ) {
            return;
        }

        for ( const auto& line: log.mLines ) {
            mDisplayLines.push_back( { log.mEntry.mID, line.mOffset, line.mLength } );
        }

        // Monotonic queue: a newer, wider entry outlives and dominates narrower
        // candidates. Each candidate enters and leaves once, so eviction keeps
        // the exact horizontal extent without rescanning retained messages.
        while ( !mWidthCandidates.empty() && mWidthCandidates.back().mWidth <= log.mWidth ) {
            mWidthCandidates.pop_back();
        }

        mWidthCandidates.push_back( { log.mEntry.mID, log.mWidth } );
    }

    auto RuntimeConsolePanel::UpdateLogCache( bool rebuild ) -> void {
        MKT_BEGIN_PROFILER();
        const auto firstID{ mPaused ? ( mLogs.empty() ? mNextLogID : mLogs.front().mEntry.mID ) : core::RuntimeConsole::Get()->ReadLogUpdates( mNextLogID, mIncomingLogs ) };

        while ( !mLogs.empty() && mLogs.front().mEntry.mID < firstID ) {
            --mLevelCounts[static_cast<core::usize>( mLogs.front().mEntry.mLevel )];
            mLogs.pop_front();
        }
        while ( !mDisplayLines.empty() && mDisplayLines.front().mEntryID < firstID ) {
            mDisplayLines.pop_front();
        }
        while ( !mWidthCandidates.empty() && mWidthCandidates.front().mEntryID < firstID ) {
            mWidthCandidates.pop_front();
        }

        if ( mLogFont != ImGui::GetFont() || mLogFontSize != ImGui::GetFontSize() ) {
            mLogFont = ImGui::GetFont();
            mLogFontSize = ImGui::GetFontSize();

            for ( auto& log: mLogs ) {
                MeasureLog( log );
            }

            rebuild = true;
        }

        if ( rebuild ) {
            mDisplayLines.clear();
            mWidthCandidates.clear();
            // Filtering rebuilds row references only. Unicode measurement and
            // multiline splitting are cached until the font or message changes.
            for ( const auto& log: mLogs ) {
                AppendDisplayLines( log );
            }
        }

        for ( auto& entry: mIncomingLogs ) {
            ++mLevelCounts[static_cast<core::usize>( entry.mLevel )];
            mLogs.push_back( CachedLog{ eastl::move( entry ) } );

            MeasureLog( mLogs.back() );

            AppendDisplayLines( mLogs.back() );
        }

        mIncomingLogs.clear();
        mContentWidth = mWidthCandidates.empty() ? 0.0f : mWidthCandidates.front().mWidth;
    }

    auto RuntimeConsolePanel::OnRender( float ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mPanelIsVisible ) {
            return;
        }

        if ( mFocusCommandInput ) {
            ImGui::SetNextWindowFocus();
        }

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible, ImGuiWindowFlags_NoCollapse ) ) {
            ImGui::End();
            return;
        }

        mPanelIsFocused = ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows );
        mPanelIsHovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_RootAndChildWindows );

        bool filterChanged{ DrawSeverityFilter( core::ConsoleLogLevel::eInfo, "runtime_console_info", mShowInfo ) };
        ImGui::SameLine();
        filterChanged |= DrawSeverityFilter( core::ConsoleLogLevel::eWarning, "runtime_console_warn", mShowWarning );
        ImGui::SameLine();
        filterChanged |= DrawSeverityFilter( core::ConsoleLogLevel::eError, "runtime_console_error", mShowError );
        ImGui::SameLine();
        filterChanged |= DrawSeverityFilter( core::ConsoleLogLevel::eDebug, "runtime_console_debug", mShowDebug );
        imgui::CheckBox( imgui::widget::LocalizedLabel( "runtime_console_auto_scroll", "Auto-scroll" ).c_str(), mAutoScroll );
        ImGui::SameLine();
        imgui::CheckBox( imgui::widget::LocalizedLabel( "runtime_console_pause_display", "Pause display" ).c_str(), mPaused );

        if ( ImGui::IsItemHovered() ) {
            mikoto::imgui::widget::ItemTooltip( MKT_LOC( "runtime_console_logging_continues_while_the_display_is_paused_resume_shows_the" ).c_str() );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "runtime_console_clear", "Clear" ).c_str() ) ) {
            core::RuntimeConsole::Get()->ExecuteCommand( "clear" );
            mLogs.clear();
            mDisplayLines.clear();
            mWidthCandidates.clear();
            mLevelCounts.fill( 0 );
            mWasAtBottom = true;
        }

        ImGui::SameLine();
        const bool copyVisible{ ImGui::Button( imgui::widget::LocalizedLabel( "runtime_console_copy_visible", "Copy visible" ).c_str() ) };
        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "runtime_console_latest", "Latest" ).c_str() ) ) {
            mScrollToBottom = true;
        }

        filterChanged |= imgui::widget::SearchFilter( "##ConsoleSearch", mTextFilter,
            MKT_LOC( "runtime_console_search_include_exclude" ) );
        ImGui::TextDisabled( MKT_LOC( "runtime_console_messages_errors_warnings" ).c_str(),
            static_cast<unsigned long long>( mLogs.size() ),
            static_cast<unsigned long long>( mLevelCounts[static_cast<core::usize>( core::ConsoleLogLevel::eError )] ),
            static_cast<unsigned long long>( mLevelCounts[static_cast<core::usize>( core::ConsoleLogLevel::eWarning )] ),
            mPaused ? MKT_LOC( "runtime_console_display_paused" ).c_str() : "" );
        ImGui::Separator();

        {
            imgui::ImGuiScopedTextFont font{ imgui::ImGuiService::Get()->PushFont( "./Resources/Fonts/Google_Sans_Code/static/GoogleSansCode-Light.ttf" ) };
            const auto previousNextID{ mNextLogID };
            UpdateLogCache( filterChanged );

            if ( copyVisible ) {
                eastl::string text{};

                for ( const auto& line: mDisplayLines ) {
                    const auto& entry{ mLogs[static_cast<core::usize>( line.mEntryID - mLogs.front().mEntry.mID )].mEntry };
                    text.append( entry.mText.data() + line.mOffset, line.mLength );
                    text += '\n';
                }

                ImGui::SetClipboardText( text.c_str() );
            }

            mScrollToBottom |= mAutoScroll && mWasAtBottom && previousNextID != mNextLogID;
            // Retain the measured width when long lines scroll out of view.
            ImGui::SetNextWindowContentSize( ImVec2{ mContentWidth, 0.0f } );
            ImGui::PushStyleColor( ImGuiCol_ChildBg, ImGui::GetStyleColorVec4( ImGuiCol_FrameBg ) );
            const bool visible{ ImGui::BeginChild( "ConsoleScrollRegion",
                ImVec2{ 0.0f, -ImGui::GetFrameHeightWithSpacing() }, false,
                ImGuiWindowFlags_HorizontalScrollbar ) };

            if ( visible ) {
                ImGuiListClipper clipper{};
                clipper.Begin( static_cast<core::i32>( mDisplayLines.size() ), ImGui::GetTextLineHeightWithSpacing() );

                while ( clipper.Step() ) {
                    for ( core::i32 index{ clipper.DisplayStart }; index < clipper.DisplayEnd; ++index ) {
                        const auto& line{ mDisplayLines[index] };
                        const auto& log{ mLogs[static_cast<core::usize>( line.mEntryID - mLogs.front().mEntry.mID )] };
                        DrawLogLine( log, line );
                    }
                }

                if ( mScrollToBottom ) {
                    ImGui::SetScrollHereY( 1.0f );
                    mScrollToBottom = false;
                    mWasAtBottom = true;
                }
                else {
                    mWasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - ImGui::GetTextLineHeight();
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::Separator();

        ImGui::SetNextItemWidth( -1 );

        if ( mFocusCommandInput ) {
            ImGui::SetKeyboardFocusHere();
            mFocusCommandInput = false;
        }

        if ( ImGui::InputTextWithHint( "##ConsoleInput",
                 MKT_LOC( "runtime_console_command_help_tab_to_complete_up_down_for_history" ).c_str(),
                 mCommandInput.data(), mCommandInput.size(),
                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion,
                 []( ImGuiInputTextCallbackData* data ) {
            return static_cast<RuntimeConsolePanel*>( data->UserData )->CommandInputCallback( *data );
        }, this ) ) {
            if ( mCommandInput[0] != '\0' ) {
                eastl::string command{ mCommandInput.data() };
                command.trim();

                if ( !command.empty() ) {
                    if ( mCommandHistory.empty() || mCommandHistory.back() != command ) {
                        if ( mCommandHistory.size() == 100 ) {
                            mCommandHistory.erase( mCommandHistory.begin() );
                        }

                        mCommandHistory.push_back( command );
                    }

                    core::RuntimeConsole::Get()->Info( string::Format( "> {}", command ) );
                    core::RuntimeConsole::Get()->ExecuteCommand( command );
                }

                mHistoryPosition = -1;
                mCommandDraft.clear();
                mCommandInput[0] = '\0';
                mScrollToBottom = true;
            }

            ImGui::SetKeyboardFocusHere( -1 );
        }

        ImGui::End();
    }

    auto RuntimeConsolePanel::CommandInputCallback( ImGuiInputTextCallbackData& data ) -> int {
        if ( data.EventFlag == ImGuiInputTextFlags_CallbackHistory && !mCommandHistory.empty() ) {
            if ( data.EventKey == ImGuiKey_UpArrow ) {
                if ( mHistoryPosition < 0 ) {
                    mCommandDraft.assign( data.Buf, data.BufTextLen );
                    mHistoryPosition = static_cast<core::i32>( mCommandHistory.size() ) - 1;
                }
                else if ( mHistoryPosition > 0 ) {
                    --mHistoryPosition;
                }
            }
            else if ( data.EventKey == ImGuiKey_DownArrow && mHistoryPosition >= 0 ) {
                if ( ++mHistoryPosition == static_cast<core::i32>( mCommandHistory.size() ) ) {
                    mHistoryPosition = -1;
                }
            }
            else {
                return 0;
            }

            const auto& command{ mHistoryPosition < 0 ? mCommandDraft : mCommandHistory[mHistoryPosition] };
            data.DeleteChars( 0, data.BufTextLen );
            data.InsertChars( 0, command.c_str() );
        }
        else if ( data.EventFlag == ImGuiInputTextFlags_CallbackCompletion ) {
            // Complete only the command name; leave argument text untouched.
            const eastl::string prefix{ data.Buf, static_cast<core::usize>( data.CursorPos ) };

            if ( prefix.find_first_of( " \t" ) != eastl::string::npos ) {
                return 0;
            }

            eastl::vector<eastl::string> matches{};

            for ( const auto& command: core::RuntimeConsole::Get()->GetCommands() ) {
                if ( command.mName.compare( 0, prefix.size(), prefix ) == 0 ) {
                    matches.push_back( command.mName );
                }
            }

            if ( matches.empty() ) {
                return 0;
            }

            eastl::string completion{ matches.front() };

            for ( const auto& match: matches ) {
                core::usize common{};

                while ( common < completion.size() && common < match.size() && completion[common] == match[common] ) {
                    ++common;
                }

                completion.resize( common );
            }

            data.DeleteChars( 0, data.CursorPos );
            data.InsertChars( 0, completion.c_str() );

            if ( matches.size() == 1 && data.BufTextLen == data.CursorPos ) {
                data.InsertChars( data.CursorPos, " " );
            }
            else if ( matches.size() > 1 ) {
                eastl::string message{ MKT_LOC( "runtime_console_commands" ).c_str() };

                for ( const auto& match: matches ) {
                    message += " " + match;
                }

                core::RuntimeConsole::Get()->Info( message );
                mScrollToBottom = true;
            }
        }

        return 0;
    }

    auto RuntimeConsolePanel::FocusCommandInput() -> void {
        mPanelIsVisible = true;
        mFocusCommandInput = true;
    }
}
