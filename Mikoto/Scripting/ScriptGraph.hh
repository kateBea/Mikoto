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

#ifndef MIKOTO_SCRIPTING_SCRIPT_GRAPH_HH
#define MIKOTO_SCRIPTING_SCRIPT_GRAPH_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Scripting/Script.hh>

namespace mikoto::scripting {
    enum class ScriptGraphValueType {
        eFlow,
        eFloat,
        eBool,
        eString,
        eVector3,
        eKeyCode
    };

    enum class ScriptNodeType {
        eBeginPlay,
        eFloat,
        eAdd,
        eGreater,
        eBranch,
        ePrint,
        eComment,
        eOnUpdate,
        eGetPosition,
        eSetPosition,
        eKeyPressed,
        eGetRotation,
        eSetRotation,
        eGetScale,
        eSetScale,
        eVector3,
        eVectorAdd
    };

    struct ScriptPin {
        core::u64 mID{};
        eastl::string mName{};
        ScriptGraphValueType mType{};
        bool mInput{};
        core::f32 mNumber{};
        core::float3 mVector{};
        bool mBoolean{};
        eastl::string mText{};
    };

    struct ScriptNode {
        core::u64 mID{};
        ScriptNodeType mType{};
        eastl::string mName{};
        eastl::string mBinding{};
        core::float2 mPosition{};
        eastl::vector<ScriptPin> mPins{};
    };

    struct ScriptLink {
        core::u64 mID{};
        core::u64 mSource{};
        core::u64 mDestination{};
    };

    struct ScriptDiagnostic {
        core::u64 mNode{};
        eastl::string mMessage{};
    };

    /** Editor-independent authoring document. No runtime execution or scene ownership. */
    class ScriptGraph {
    public:

        auto RemoveNode( core::u64 id ) -> void;
        auto Disconnect( core::u64 id ) -> void;

        MKT_NODISCARD auto AddNode( ScriptNodeType type, core::float2 position = {} ) -> core::u64;

        MKT_NODISCARD auto CanConnect( core::u64 source, core::u64 destination ) const -> bool;
        MKT_NODISCARD auto Connect( core::u64 source, core::u64 destination ) -> bool;
        MKT_NODISCARD auto FindPin( core::u64 id ) const -> const ScriptPin*;
        MKT_NODISCARD auto GetNodes() -> eastl::vector<ScriptNode>&;
        MKT_NODISCARD auto GetNodes() const -> const eastl::vector<ScriptNode>&;
        MKT_NODISCARD auto GetLinks() const -> const eastl::vector<ScriptLink>&;
        MKT_NODISCARD auto Validate() const -> eastl::vector<ScriptDiagnostic>;
        MKT_NODISCARD auto Serialize() const -> YAML::Node;

        // Transactional load: leaves the current document untouched on failure.
        MKT_NODISCARD auto Deserialize( const YAML::Node& document, eastl::string& error ) -> bool;
        auto InitializeDemo() -> void;

    private:
        eastl::vector<ScriptNode> mNodes{};
        eastl::vector<ScriptLink> mLinks{};
        core::u64 mNextID{ 1 };
    };

    struct ScriptCompileResult {
        bool mSuccess{};
        eastl::string mSource{};
        eastl::vector<ScriptDiagnostic> mDiagnostics{};
    };

    /** Lowers typed data connections and event flow into Mikoto script source. */
    class ScriptGraphCompiler {
    public:
        MKT_NODISCARD auto Compile( const ScriptGraph& graph, ScriptLanguage language, eastl::string_view name ) const -> ScriptCompileResult;
    };
}// namespace mikoto::scripting

#endif // MIKOTO_SCRIPTING_SCRIPT_GRAPH_HH
