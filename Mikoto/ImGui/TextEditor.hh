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

#ifndef MIKOTO_IMGUI_TEXT_EDITOR_HH
#define MIKOTO_IMGUI_TEXT_EDITOR_HH

#include <imgui.h>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <TextEditor.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::imgui {

    enum class TextLanguage : core::i32 {
        eNone,
        eCpp,
        eC,
        eHlsl,
        eGlsl,
        eLua,
        eSql,
        eAngelScript,
        eCSharp
    };

    enum class TextEditorPalette : core::i32 {
        eDark,
        eLight,
        eRetroBlue
    };

    enum class TextSelectionMode : core::i32 {
        eNormal,
        eWord,
        eLine
    };

    struct TextCoordinates {
        core::i32 mLine{};
        core::i32 mColumn{};
    };

    class TextEditor {
    public:
        TextEditor();

        auto SetText( const eastl::string& text ) -> void;
        auto SetReadOnly( bool isReadOnly ) -> void;
        auto SetLanguage( TextLanguage language ) -> void;
        auto SetShowWhitespaces( bool enable ) -> void;
        auto SetPalette( TextEditorPalette palette ) -> void;

        auto SetSelection( const TextCoordinates& start,
            const TextCoordinates& end, TextSelectionMode mode = TextSelectionMode::eNormal ) -> void;

        auto SelectAll() -> void;

        auto Undo() -> void;
        auto Redo() -> void;
        auto Copy() -> void;
        auto Cut() -> void;
        auto Delete() -> void;
        auto Paste() -> void;

        MKT_NODISCARD auto GetCursorPosition() const -> TextCoordinates;

        MKT_NODISCARD auto GetText() const -> eastl::string;
        MKT_NODISCARD auto IsReadOnly() const -> bool;
        MKT_NODISCARD auto GetLanguage() const -> TextLanguage;
        MKT_NODISCARD auto GetPalette() const -> TextEditorPalette;

        MKT_NODISCARD auto CanUndo() const -> bool;
        MKT_NODISCARD auto CanRedo() const -> bool;
        MKT_NODISCARD auto HasSelection() const -> bool;
        MKT_NODISCARD auto IsOverwrite() const -> bool;
        MKT_NODISCARD auto GetTotalLines() const -> core::i32;
        MKT_NODISCARD auto GetCursorLine() const -> core::i32;
        MKT_NODISCARD auto GetCursorColumn() const -> core::i32;

        auto Draw( core::cstr title, const ImVec2& size = {}, bool showBorder = false ) -> void;

        MKT_NODISCARD static auto GetLanguageName( TextLanguage language ) -> core::cstr;

    private:
        MKT_NODISCARD auto ToDocumentPosition( const TextCoordinates& position ) const -> ::TextEditor::DocPos;

    private:
        ::TextEditor mEditor{};

        TextLanguage mLanguage{ TextLanguage::eNone };
        TextEditorPalette mPalette{ TextEditorPalette::eDark };
    };
}// namespace mikoto::imgui

#endif//MIKOTO_IMGUI_TEXT_EDITOR_HH
