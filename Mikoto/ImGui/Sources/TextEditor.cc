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

#include <string_view>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <glm/common.hpp>

#include <ImGui/TextEditor.hh>

namespace mikoto::imgui {
    using namespace mikoto::core;


    static auto RetroBluePalette() -> const ::TextEditor::Palette& {
        static const auto palette = [] {
            auto result{ ::TextEditor::GetDarkPalette() };
            const auto set = [&]( ::TextEditor::Color color, ImU32 value ) {
                result[static_cast<usize>( color )] = value;
            };
            using Color = ::TextEditor::Color;

            // Keep syntax colours distinct from the selection and editing chrome below.
            set( Color::text, 0xff00ffff );
            set( Color::keyword, 0xffffff00 );
            set( Color::declaration, 0xffffff00 );
            set( Color::number, 0xff00ff00 );
            set( Color::string, 0xff808000 );
            set( Color::punctuation, 0xffffffff );
            set( Color::preprocessor, 0xff008000 );
            set( Color::identifier, 0xff00ffff );
            set( Color::knownIdentifier, 0xffffffff );
            set( Color::comment, 0xff808080 );

            set( Color::background, 0xff800000 );
            set( Color::cursor, 0xff0080ff );
            set( Color::selection, 0x80ffff00 );
            set( Color::lineNumber, 0xff808000 );
            set( Color::currentLineNumber, 0xff00ffff );

            return result;
        }();
        return palette;
    }

    // Keep these names aligned with Scripting/Sources/*Binding*.cc and
    // Scripting/Dotnet/{EntityScript,MathTypes}.cs. These are lexical hints,
    // not symbol resolution or type inference for user-defined variables.
    static auto MikotoCppLanguage() -> const ::TextEditor::Language& {
        static const auto definition = [] {
            auto language{ *::TextEditor::Language::Cpp() };
            constexpr const char* names[]{
                "mikoto", "core", "scene", "scripting", "Entity", "ScriptComponent",
                "TransformComponent", "TagComponent", "RigidBodyComponent",
                "IScript", "LuaScript", "DotnetScript", "NativeScript", "ScriptHandle", "ScriptLanguage", "NativeScriptingService",
                "ScriptingService", "f32", "f64", "i32", "u32", "u64"
            };

            for ( const auto* name: names ) {
                language.identifiers.insert( name );
            }

            return language;
        }();
        return definition;
    }

    static auto MikotoLuaLanguage() -> const ::TextEditor::Language& {
        static const auto definition = [] {
            auto language{ *::TextEditor::Language::Lua() };
            constexpr const char* names[]{
                "Entity", "Canvas", "TagComponent", "TransformComponent", "RigidBodyComponent",
                "PointLight", "SpotLight", "Vec2F", "Vec3F", "Vec4F", "Mat2F", "Mat3F", "Mat4F",
                "Input", "KeyCode", "System", "Console", "LogLevel", "Math"
            };

            for ( const auto* name: names ) {
                language.identifiers.insert( name );
            }

            return language;
        }();
        return definition;
    }

    static auto MikotoCSharpLanguage() -> const ::TextEditor::Language& {
        static const auto definition = [] {
            auto language{ *::TextEditor::Language::Cs() };
            constexpr const char* names[]{
                "Mikoto", "EntityScript", "Input", "KeyCode", "Log",
                "Vector2", "Vector3", "Vector4", "Matrix2", "Matrix3", "Matrix4"
            };

            for ( const auto* name: names ) {
                language.identifiers.insert( name );
            }

            return language;
        }();
        return definition;
    }


    TextEditor::TextEditor() {
        mEditor.SetPalette(
            ::TextEditor::GetDarkPalette() );

        SetLanguage( TextLanguage::eCpp );
    }

    auto TextEditor::SetText( const eastl::string& text ) -> void {
        mEditor.SetText( std::string_view{ text.data(), text.size() } );
    }

    auto TextEditor::SetReadOnly( bool isReadOnly ) -> void {
        mEditor.SetReadOnlyEnabled( isReadOnly );
    }

    auto TextEditor::GetText() const -> eastl::string {
        const auto text{ mEditor.GetText() };
        return eastl::string{ text.data(), text.size() };
    }

    auto TextEditor::IsReadOnly() const -> bool {
        return mEditor.IsReadOnlyEnabled();
    }

    auto TextEditor::SetLanguage( TextLanguage language ) -> void {
        // Resetting the definition restarts incremental syntax coloring. The
        // inspector calls this every frame; let coloring finish when unchanged.
        if ( mLanguage == language ) {
            return;
        }

        mLanguage = language;

        switch ( language ) {
            case TextLanguage::eCpp:
                mEditor.SetLanguage( &MikotoCppLanguage() );
                break;

            case TextLanguage::eC:
                mEditor.SetLanguage( ::TextEditor::Language::C() );
                break;

            case TextLanguage::eHlsl:
                mEditor.SetLanguage( ::TextEditor::Language::Hlsl() );
                break;

            case TextLanguage::eGlsl:
                mEditor.SetLanguage( ::TextEditor::Language::Glsl() );
                break;

            case TextLanguage::eLua:
                mEditor.SetLanguage( &MikotoLuaLanguage() );
                break;

            case TextLanguage::eSql:
                mEditor.SetLanguage( ::TextEditor::Language::Sql() );
                break;

            case TextLanguage::eAngelScript:
                mEditor.SetLanguage( ::TextEditor::Language::AngelScript() );
                break;

            case TextLanguage::eCSharp:
                mEditor.SetLanguage( &MikotoCSharpLanguage() );
                break;

            case TextLanguage::eNone:
                mEditor.SetLanguage( nullptr );
                break;
            default:;
        }
    }

    auto TextEditor::SetShowWhitespaces( bool enable ) -> void {
        mEditor.SetShowWhitespacesEnabled( enable );
    }

    auto TextEditor::GetLanguage() const -> TextLanguage {
        return mLanguage;
    }

    auto TextEditor::Draw( core::cstr title, const ImVec2& size, bool showBorder ) -> void {
        // The editor draws a navigation outline whenever it has keyboard focus.
        // Suppress that decoration locally without disabling keyboard navigation
        // or changing the caller's optional child border.
        ImGui::PushStyleColor( ImGuiCol_NavCursor, ImVec4{} );
        mEditor.Render( title, size, showBorder ? ImGuiChildFlags_Borders : ImGuiChildFlags_None );
        ImGui::PopStyleColor();
    }

    auto TextEditor::GetLanguageName( TextLanguage language ) -> core::cstr {
        switch ( language ) {
            case TextLanguage::eCpp:
                return "C++";

            case TextLanguage::eC:
                return "C";

            case TextLanguage::eHlsl:
                return "HLSL";

            case TextLanguage::eGlsl:
                return "GLSL";

            case TextLanguage::eLua:
                return "Lua";

            case TextLanguage::eSql:
                return "SQL";

            case TextLanguage::eAngelScript:
                return "AngelScript";

            case TextLanguage::eCSharp:
                return "C#";

            case TextLanguage::eNone:
            default:
                return "None";
        }
    }

    auto TextEditor::Undo() -> void {
        mEditor.Undo();
    }

    auto TextEditor::Redo() -> void {
        mEditor.Redo();
    }

    auto TextEditor::Copy() -> void {
        mEditor.Copy();
    }

    auto TextEditor::Cut() -> void {
        mEditor.Cut();
    }

    auto TextEditor::Delete() -> void {
        // Replacing selected text preserves the clipboard and participates in undo.
        if ( mEditor.AnyCursorHasSelection() ) {
            mEditor.ReplaceTextInAllCursors( "" );
        }
    }

    auto TextEditor::Paste() -> void {
        mEditor.Paste();
    }

    auto TextEditor::GetCursorPosition() const -> TextCoordinates {
        const auto position{ mEditor.GetCurrentCursorPosition() };
        const auto line{ mEditor.GetLineText( position.line ) };
        usize glyph{};
        i32 column{};

        // Public columns retain the old editor's tab-expanded positions.
        for ( const unsigned char byte: line ) {
            if ( ( byte & 0xc0 ) == 0x80 ) {
                continue;
            }

            if ( glyph++ == position.index ) {
                break;
            }

            column += byte == '\t' ? static_cast<i32>( mEditor.GetTabSize() ) - column % static_cast<i32>( mEditor.GetTabSize() ) : 1;
        }

        return TextCoordinates{ static_cast<i32>( position.line ), column };
    }

    auto TextEditor::SelectAll() -> void {
        mEditor.SelectAll();
    }

    auto TextEditor::SetPalette( TextEditorPalette palette ) -> void {
        mPalette = palette;

        switch ( palette ) {
            case TextEditorPalette::eDark:
                mEditor.SetPalette(
                    ::TextEditor::GetDarkPalette() );
                break;

            case TextEditorPalette::eLight:
                mEditor.SetPalette(
                    ::TextEditor::GetLightPalette() );
                break;

            case TextEditorPalette::eRetroBlue:
                mEditor.SetPalette(
                    RetroBluePalette() );
                break;
        }
    }

    auto TextEditor::ToDocumentPosition( const TextCoordinates& position ) const -> ::TextEditor::DocPos {
        const auto lineIndex{ static_cast<usize>( glm::max( position.mLine, 0 ) ) };

        if ( lineIndex >= mEditor.GetLineCount() ) {
            const auto lastLine{ mEditor.GetLineCount() - 1 };
            // The backend clamps an oversized glyph index to the line's end.
            return { lastLine, mEditor.GetLineText( lastLine ).size() };
        }

        const auto line{ mEditor.GetLineText( lineIndex ) };
        usize glyph{};
        i32 column{};

        for ( const unsigned char byte: line ) {
            if ( ( byte & 0xc0 ) == 0x80 ) {
                continue;
            }

            if ( column >= position.mColumn ) {
                break;
            }

            column += byte == '\t' ? static_cast<i32>( mEditor.GetTabSize() ) - column % static_cast<i32>( mEditor.GetTabSize() ) : 1;
            ++glyph;
        }

        return { lineIndex, glyph };
    }

    auto TextEditor::SetSelection( const TextCoordinates& start, const TextCoordinates& end,
        TextSelectionMode mode ) -> void {
        auto first{ ToDocumentPosition( start ) };
        auto last{ ToDocumentPosition( end ) };

        if ( last < first ) {
            const auto temporary{ first };
            first = last;
            last = temporary;
        }

        if ( mode == TextSelectionMode::eLine ) {
            mEditor.SelectLines( first.line, last.line );
        }
        else {
            if ( mode == TextSelectionMode::eWord ) {
                first = mEditor.FindWordStart( first, true );
                last = mEditor.FindWordEnd( last, true );
            }

            mEditor.SelectRegion( first, last );
        }
    }

    auto TextEditor::GetPalette() const -> TextEditorPalette {
        return mPalette;
    }

    auto TextEditor::CanUndo() const -> bool {
        return mEditor.CanUndo();
    }

    auto TextEditor::CanRedo() const -> bool {
        return mEditor.CanRedo();
    }

    auto TextEditor::HasSelection() const -> bool {
        return mEditor.AnyCursorHasSelection();
    }

    auto TextEditor::IsOverwrite() const -> bool {
        return mEditor.IsOverwriteEnabled();
    }

    auto TextEditor::GetTotalLines() const -> core::i32 {
        return static_cast<i32>( mEditor.GetLineCount() );
    }

    auto TextEditor::GetCursorLine() const -> core::i32 {
        return GetCursorPosition().mLine;
    }

    auto TextEditor::GetCursorColumn() const -> core::i32 {
        return GetCursorPosition().mColumn;
    }
}// namespace mikoto::imgui
