// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.
#ifndef MIKOTO_PANELS_SCRIPT_GRAPH_PANEL_HH
#define MIKOTO_PANELS_SCRIPT_GRAPH_PANEL_HH

#include <Assets/AssetLoadRegistry.hh>
#include <Assets/AssetScriptSource.hh>

#include <Scripting/ScriptGraph.hh>

#include <ImGui/TextEditor.hh>
#include <ImGui/GraphEditor.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct ScriptGraphPanelCreateInfo {
        eastl::string mGroup{ "Other" };
        bool mIsToggleable{ true };
    };

    class ScriptGraphPanel final : public Panel {
    public:
        explicit ScriptGraphPanel( const ScriptGraphPanelCreateInfo& info );
        auto OnRender( core::f32 timeStep ) -> void override;

    private:
        auto DrawGraph() -> void;
        auto Open() -> void;
        auto Save() -> void;
        auto GenerateScript( eastl::string_view name ) -> bool;
        auto ExportScript() -> void;
        auto DrawScriptPreview() -> void;
        imgui::TextEditor mScriptPreview{};
        asset::AssetHandle<asset::ScriptSource> mPreviewRequest{};
        filesystem::Path mPreviewPath{};
        eastl::string mPreviewError{};
        bool mShowScriptPreview{};
        bool mPreviewLoaded{};
        scripting::ScriptGraph mGraph{};
        scripting::ScriptGraphCompiler mCompiler{};
        scripting::ScriptCompileResult mResult{};
        core::i32 mLanguageIndex{};
        imgui::NodeEditorContext mContext{};
        eastl::string mMessage{};
        bool mRestore{ true };
        bool mFit{ true };
        bool mDirty{};
    };
}// namespace mikoto::editor

#endif // MIKOTO_PANELS_SCRIPT_GRAPH_PANEL_HH

