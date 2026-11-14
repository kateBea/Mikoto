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

#include <cmath>

#include <cmath>

#include <EASTL/algorithm.h>
#include <EASTL/hash_map.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Exception.hh>

#include <Scripting/ScriptGraph.hh>

namespace mikoto::scripting {
    using namespace core;

    static constexpr core::u64 kPinStride{ 16 };

    static auto FormatNumber( core::f32 value, ScriptLanguage language ) -> eastl::string {
        auto literal{ string::Format( "{:.9g}", value ) };
        if ( literal.find_first_of( ".eE" ) == eastl::string::npos ) {
            literal += ".0";
        }

        if ( language != ScriptLanguage::eLua ) {
            literal += 'f';
        }

        return literal;
    }

    // Lua decimal escapes and C++ octal escapes have fixed widths here so adjacent digits stay literal.
    // https://www.lua.org/manual/5.4/manual.html#3.1
    static auto QuoteScriptText( eastl::string_view text, ScriptLanguage language ) -> eastl::string {
        eastl::string result{ "\"" };
        for ( const auto character: text ) {
            switch ( character ) {
                case '\\': result += "\\\\"; break;
                case '"': result += "\\\""; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: {
                    const auto byte{ core::as<core::u8>( character ) };
                    if ( byte >= 32 && byte != 127 ) {
                        result += character;
                    }
                    else if ( language == ScriptLanguage::eLua ) {
                        result += string::Format( "\\{:03}", core::as<core::u32>( byte ) );
                    }
                    else if ( language == ScriptLanguage::eCSharp ) {
                        result += string::Format( "\\u{:04X}", core::as<core::u32>( byte ) );
                    }
                    else {
                        result += string::Format( "\\{:03o}", core::as<core::u32>( byte ) );
                    }
                    break;
                }
            }
        }

        result += '"';
        return result;
    }

    static auto IsScriptIdentifier( eastl::string_view name ) -> bool {
        const auto isLetter = []( char ch ) {
            return ( ch >= 'a' && ch <= 'z' ) || ( ch >= 'A' && ch <= 'Z' ) || ch == '_';
        };

        return !name.empty() && isLetter( name.front() ) &&
            eastl::all_of( name.begin() + 1, name.end(), [&]( char ch ) {
                return isLetter( ch ) || ( ch >= '0' && ch <= '9' );
            } );
    }

    // The graph's events define callback entry points and typed pins define dependencies.
    // https://dev.epicgames.com/documentation/unreal-engine/blueprint-compiler-overview
    class ScriptSourceWriter {
    public:
        ScriptSourceWriter( const ScriptGraph& graph, ScriptLanguage language, ScriptCompileResult& result )
            : mGraph{ graph }, mLanguage{ language }, mResult{ result } {
            for ( const auto& node: graph.GetNodes() ) {
                mNodes.emplace( node.mID, &node );
            }

            for ( const auto& link: graph.GetLinks() ) {
                mInputs.emplace( link.mDestination, link.mSource );
                if ( const auto* pin{ graph.FindPin( link.mSource ) }; pin && pin->mType == ScriptGraphValueType::eFlow ) {
                    mFlow.emplace( link.mSource, mNodes.at( link.mDestination / kPinStride ) );
                }
            }
        }

        auto Write( eastl::string_view name ) -> void {
            switch ( mLanguage ) {
                case ScriptLanguage::eLua:
                    mResult.mSource = "local script = {}\n\n";
                    break;
                case ScriptLanguage::eCSharp:
                    mResult.mSource = string::Format( "public sealed class @{} : global::Mikoto.EntityScript\n{{\n", name );
                    mIndent = 1;
                    break;
                case ScriptLanguage::eNative:
                    mResult.mSource = "#include <Scripting/Native/NativeScriptApi.hh>\n\n"
                        "using mikoto::scripting::native::EntityScript;\n"
                        "using mikoto::scripting::native::Vector3;\n\n"
                        "class GraphScript final : public EntityScript {\npublic:\n";
                    mIndent = 1;
                    break;
                default:
                    return;
            }

            for ( const auto& node: mGraph.GetNodes() ) {
                if ( node.mType == ScriptNodeType::eBeginPlay || node.mType == ScriptNodeType::eOnUpdate ) {
                    WriteEvent( node );
                }
            }

            if ( mLanguage == ScriptLanguage::eLua ) {
                mResult.mSource += "return script\n";
            }
            else if ( mLanguage == ScriptLanguage::eCSharp ) {
                mResult.mSource += "}\n";
            }
            else {
                mResult.mSource += "};\n\nMKT_NATIVE_SCRIPT( GraphScript )\n";
            }
        }

    private:
        using NodeType = ScriptNodeType;

        auto Line( eastl::string_view source ) -> void {
            constexpr core::usize spacesPerIndent{ 4 };
            mResult.mSource.append( mIndent * spacesPerIndent, ' ' );
            mResult.mSource.append( source.data(), source.size() );
            mResult.mSource += '\n';
        }

        auto Statement( eastl::string_view source ) -> void {
            if ( mLanguage == ScriptLanguage::eLua ) {
                Line( source );
            }
            else {
                Line( string::Format( "{};", source ) );
            }
        }

        auto WriteEvent( const ScriptNode& event ) -> void {
            mUpdating = event.mType == NodeType::eOnUpdate;
            switch ( mLanguage ) {
                case ScriptLanguage::eLua:
                    Line( mUpdating ? "function script:OnUpdate(timeStep)" : "function script:OnCreate()" );
                    break;
                case ScriptLanguage::eCSharp:
                    Line( mUpdating ? "public override void OnUpdate(float timeStep)" : "public override void OnCreate()" );
                    Line( "{" );
                    break;
                case ScriptLanguage::eNative:
                    Line( mUpdating ? "auto OnUpdate( [[maybe_unused]] float timeStep ) -> void override {" : "auto OnCreate() -> void override {" );
                    break;
                default:
                    return;
            }

            ++mIndent;
            WriteFlow( event.mPins.front().mID );
            --mIndent;
            Line( mLanguage == ScriptLanguage::eLua ? "end" : "}" );
            mResult.mSource += '\n';
        }

        auto WriteFlow( core::u64 output ) -> void {
            for ( auto next{ mFlow.find( output ) }; next != mFlow.end(); next = mFlow.find( output ) ) {
                const auto& node{ *next->second };
                const auto& pins{ node.mPins };

                // Getters are evaluated again after side effects rather than reused across statements.
                mValues.clear();
                if ( node.mType == NodeType::eBranch ) {
                    const auto condition{ Input( pins[1] ) };
                    const bool lua{ mLanguage == ScriptLanguage::eLua };
                    const bool hasElse{ mFlow.find( pins[3].mID ) != mFlow.end() };
                    Line( lua ? string::Format( "if {} then", condition ) : string::Format( "if ( {} ) {{", condition ) );
                    ++mIndent;
                    WriteFlow( pins[2].mID );
                    --mIndent;

                    if ( hasElse ) {
                        Line( lua ? "else" : "} else {" );
                        ++mIndent;
                        WriteFlow( pins[3].mID );
                        --mIndent;
                    }

                    Line( lua ? "end" : "}" );
                    return;
                }

                const auto value{ Input( pins[1] ) };
                if ( node.mType == NodeType::ePrint ) {
                    const auto* log{ mLanguage == ScriptLanguage::eLua ? "Console.Info" :
                        mLanguage == ScriptLanguage::eCSharp ? "global::Mikoto.Log.Info" : "LogInfo" };
                    Statement( string::Format( "{}({})", log, value ) );
                }
                else {
                    Statement( Transform( node.mType, value ) );
                }

                output = pins[2].mID;
            }
        }

        auto Transform( NodeType type, eastl::string_view value = {} ) const -> eastl::string {
            const bool set{ type == NodeType::eSetPosition || type == NodeType::eSetRotation || type == NodeType::eSetScale };
            const bool position{ type == NodeType::eSetPosition || type == NodeType::eGetPosition };
            const bool rotation{ type == NodeType::eSetRotation || type == NodeType::eGetRotation };
            const auto* field{ position ? "Position" : rotation ? "Rotation" : "Scale" };

            if ( mLanguage == ScriptLanguage::eCSharp ) {
                return set ? string::Format( "this.{} = {}", field, value ) : string::Format( "this.{}", field );
            }

            const auto* prefix{ "" };
            if ( mLanguage == ScriptLanguage::eLua ) {
                prefix = "self.Entity:GetTransform():";
                if ( position ) {
                    field = "Translation";
                }
            }

            return string::Format( "{}{}{}({})", prefix, set ? "Set" : "Get", field, value );
        }

        auto Literal( const ScriptPin& pin ) const -> eastl::string {
            switch ( pin.mType ) {
                case ScriptGraphValueType::eFloat:
                    return FormatNumber( pin.mNumber, mLanguage );
                case ScriptGraphValueType::eBool:
                    return pin.mBoolean ? "true" : "false";
                case ScriptGraphValueType::eKeyCode:
                    return string::Format( "{}", core::as<core::u32>( pin.mNumber ) );
                case ScriptGraphValueType::eString:
                    return QuoteScriptText( pin.mText, mLanguage );
                case ScriptGraphValueType::eVector3: {
                    const auto x{ FormatNumber( pin.mVector.x, mLanguage ) };
                    const auto y{ FormatNumber( pin.mVector.y, mLanguage ) };
                    const auto z{ FormatNumber( pin.mVector.z, mLanguage ) };
                    if ( mLanguage == ScriptLanguage::eNative ) {
                        return string::Format( "Vector3{{ {}, {}, {} }}", x, y, z );
                    }
                    return string::Format( "{}({}, {}, {})", mLanguage == ScriptLanguage::eLua ? "Vec3F.new" : "new global::Mikoto.Vector3", x, y, z );
                }
                default:
                    return {};
            }
        }

        auto Input( const ScriptPin& pin ) -> eastl::string {
            if ( const auto found{ mInputs.find( pin.mID ) }; found != mInputs.end() ) {
                return Value( found->second );
            }

            return Literal( pin );
        }

        auto Value( core::u64 pinID ) -> eastl::string {
            if ( const auto found{ mValues.find( pinID ) }; found != mValues.end() ) {
                return found->second;
            }

            const auto& node{ *mNodes.at( pinID / kPinStride ) };
            eastl::string expression{};
            switch ( node.mType ) {
                case NodeType::eFloat:
                case NodeType::eVector3:
                    return Literal( node.mPins.front() );
                case NodeType::eOnUpdate:
                    if ( !mUpdating ) {
                        mResult.mDiagnostics.push_back( { node.mID, "Time Step is only available in OnUpdate." } );
                    }
                    return "timeStep";
                case NodeType::eAdd:
                case NodeType::eGreater:
                case NodeType::eVectorAdd: {
                    const auto a{ Input( node.mPins[0] ) };
                    const auto b{ Input( node.mPins[1] ) };
                    expression = string::Format( "{} {} {}", a, node.mType == NodeType::eGreater ? ">" : "+", b );
                    break;
                }
                case NodeType::eKeyPressed: {
                    const auto key{ Input( node.mPins[0] ) };
                    if ( mLanguage == ScriptLanguage::eCSharp ) {
                        expression = string::Format( "global::Mikoto.Input.IsKeyPressed((global::Mikoto.KeyCode){})", key );
                    }
                    else {
                        expression = string::Format( "{}({})", mLanguage == ScriptLanguage::eLua ? "Input.IsKeyPressed" : "IsKeyPressed", key );
                    }
                    break;
                }
                case NodeType::eGetPosition:
                case NodeType::eGetRotation:
                case NodeType::eGetScale:
                    expression = Transform( node.mType );
                    break;
                default:
                    mResult.mDiagnostics.push_back( { node.mID, "Unsupported script output." } );
                    return {};
            }

            auto name{ string::Format( "value{}", ++mValueIndex ) };
            const auto* declaration{ mLanguage == ScriptLanguage::eLua ? "local" : mLanguage == ScriptLanguage::eCSharp ? "var" : "const auto" };
            Statement( string::Format( "{} {} = {}", declaration, name, expression ) );
            mValues.emplace( pinID, name );
            return name;
        }

        const ScriptGraph& mGraph;
        ScriptLanguage mLanguage{};
        ScriptCompileResult& mResult;
        eastl::hash_map<core::u64, const ScriptNode*> mNodes{};
        eastl::hash_map<core::u64, core::u64> mInputs{};
        eastl::hash_map<core::u64, const ScriptNode*> mFlow{};
        eastl::hash_map<core::u64, eastl::string> mValues{};
        core::usize mIndent{};
        core::u64 mValueIndex{};
        bool mUpdating{};
    };

    auto ScriptGraph::AddNode( ScriptNodeType type, core::float2 position ) -> core::u64 {
        if ( type < ScriptNodeType::eBeginPlay || type > ScriptNodeType::eVectorAdd ) {
            return 0;
        }

        ScriptNode node{ .mID = mNextID++, .mType = type, .mPosition = position };
        auto pin{ [&]( const char* name, ScriptGraphValueType value, bool input ) {
            node.mPins.push_back( { node.mID * 16 + node.mPins.size() + 1, name, value, input } );
        } };
        using V = ScriptGraphValueType;
        using N = ScriptNodeType;

        switch ( type ) {
            case N::eBeginPlay:
                node.mName = "EntityScript.OnCreate";
                node.mBinding = "Mikoto.EntityScript.OnCreate";
                pin( "Then", V::eFlow, false );
                break;
            case N::eFloat:
                node.mName = "Float";
                pin( "Value", V::eFloat, false );
                break;
            case N::eAdd:
                node.mName = "Add";
                pin( "A", V::eFloat, true );
                pin( "B", V::eFloat, true );
                pin( "Result", V::eFloat, false );
                break;
            case N::eGreater:
                node.mName = "Greater Than";
                pin( "A", V::eFloat, true );
                pin( "B", V::eFloat, true );
                pin( "Result", V::eBool, false );
                break;
            case N::eBranch:
                node.mName = "Branch";
                pin( "In", V::eFlow, true );
                pin( "Condition", V::eBool, true );
                pin( "True", V::eFlow, false );
                pin( "False", V::eFlow, false );
                break;
            case N::ePrint:
                node.mName = "Log.Info";
                node.mBinding = "Mikoto.Log.Info";
                pin( "In", V::eFlow, true );
                pin( "Text", V::eString, true );
                pin( "Then", V::eFlow, false );
                break;
            case N::eComment:
                node.mName = "Comment";
                break;
            case N::eOnUpdate:
                node.mName = "EntityScript.OnUpdate";
                node.mBinding = "Mikoto.EntityScript.OnUpdate";
                pin( "Then", V::eFlow, false );
                pin( "Time Step", V::eFloat, false );
                break;
            case N::eKeyPressed:
                node.mName = "Input.IsKeyPressed";
                node.mBinding = "Mikoto.Input.IsKeyPressed";
                pin( "Key Code", V::eKeyCode, true );
                pin( "Pressed", V::eBool, false );
                node.mPins[0].mNumber = 73;
                break;
            case N::eGetPosition:
            case N::eGetRotation:
            case N::eGetScale:
                node.mName = type == N::eGetPosition ? "Get Position" : type == N::eGetRotation ? "Get Rotation"
                                                                                                : "Get Scale";
                node.mBinding = type == N::eGetPosition ? "Mikoto.EntityScript.Position.get" : type == N::eGetRotation ? "Mikoto.EntityScript.Rotation.get"
                                                                                                                       : "Mikoto.EntityScript.Scale.get";
                pin( "Value", V::eVector3, false );
                break;
            case N::eSetPosition:
            case N::eSetRotation:
            case N::eSetScale:
                node.mName = type == N::eSetPosition ? "Set Position" : type == N::eSetRotation ? "Set Rotation"
                                                                                                : "Set Scale";
                node.mBinding = type == N::eSetPosition ? "Mikoto.EntityScript.Position.set" : type == N::eSetRotation ? "Mikoto.EntityScript.Rotation.set"
                                                                                                                       : "Mikoto.EntityScript.Scale.set";
                pin( "In", V::eFlow, true );
                pin( "Value", V::eVector3, true );
                pin( "Then", V::eFlow, false );

                if ( type == N::eSetScale ) {
                    node.mPins[1].mVector = core::float3{ 1 };
                }

                break;
            case N::eVector3:
                node.mName = "Vector3";
                pin( "Value", V::eVector3, false );
                break;
            case N::eVectorAdd:
                node.mName = "Add Vector3";
                pin( "A", V::eVector3, true );
                pin( "B", V::eVector3, true );
                pin( "Result", V::eVector3, false );
                break;
        }

        mNodes.push_back( eastl::move( node ) );
        return mNodes.back().mID;
    }

    auto ScriptGraph::FindPin( core::u64 id ) const -> const ScriptPin* {
        for ( const auto& node: mNodes ) {
            for ( const auto& pin: node.mPins ) {
                if ( pin.mID == id ) {
                    return &pin;
                }
            }
        }

        return nullptr;
    }

    auto ScriptGraph::Disconnect( core::u64 id ) -> void {
        mLinks.erase( eastl::remove_if( mLinks.begin(), mLinks.end(), [&]( const auto& l ) {
            return l.mID == id;
        } ),
            mLinks.end() );
    }

    auto ScriptGraph::RemoveNode( core::u64 id ) -> void {
        mLinks.erase( eastl::remove_if( mLinks.begin(), mLinks.end(), [&]( const auto& l ) {
            return l.mSource / 16 == id || l.mDestination / 16 == id;
        } ),
            mLinks.end() );
        mNodes.erase( eastl::remove_if( mNodes.begin(), mNodes.end(), [&]( const auto& n ) {
            return n.mID == id;
        } ),
            mNodes.end() );
    }

    auto ScriptGraph::CanConnect( core::u64 source, core::u64 destination ) const -> bool {
        const auto* a{ FindPin( source ) };
        const auto* b{ FindPin( destination ) };

        if ( !a || !b || a->mInput || !b->mInput || a->mType != b->mType || source / 16 == destination / 16 ) {
            return false;
        }

        for ( const auto& link: mLinks ) {
            if ( link.mDestination == destination || ( a->mType == ScriptGraphValueType::eFlow && link.mSource == source ) ) {
                return false;
            }
        }
        // Explicit loop nodes can be introduced with runtime semantics later.
        // At this stage both data and execution connections must be acyclic.
        eastl::vector<core::u64> pending{ destination / 16 }, visited{};

        while ( !pending.empty() ) {
            const auto id{ pending.back() };
            pending.pop_back();

            if ( id == source / 16 ) {
                return false;
            }

            if ( eastl::find( visited.begin(), visited.end(), id ) != visited.end() ) {
                continue;
            }

            visited.push_back( id );

            for ( const auto& link: mLinks ) {
                if ( link.mSource / 16 == id ) {
                    pending.push_back( link.mDestination / 16 );
                }
            }
        }

        return true;
    }

    auto ScriptGraph::Connect( core::u64 source, core::u64 destination ) -> bool {
        if ( !CanConnect( source, destination ) ) {
            return false;
        }

        mLinks.push_back( { mNextID++, source, destination } );
        return true;
    }

    auto ScriptGraph::Validate() const -> eastl::vector<ScriptDiagnostic> {
        eastl::vector<ScriptDiagnostic> errors{};
        core::usize entries{};
        core::usize updates{};

        for ( const auto& node: mNodes ) {
            entries += node.mType == ScriptNodeType::eBeginPlay;
            updates += node.mType == ScriptNodeType::eOnUpdate;

            for ( const auto& pin: node.mPins ) {
                if ( ( pin.mType == ScriptGraphValueType::eFloat && !std::isfinite( pin.mNumber ) ) ||
                     ( pin.mType == ScriptGraphValueType::eVector3 &&
                         ( !std::isfinite( pin.mVector.x ) || !std::isfinite( pin.mVector.y ) || !std::isfinite( pin.mVector.z ) ) ) ) {
                    errors.push_back( { node.mID, "Use finite numeric values." } );
                }
                if ( pin.mType == ScriptGraphValueType::eKeyCode && ( pin.mNumber < 0 || pin.mNumber > 511 || glm::floor( pin.mNumber ) != pin.mNumber ) ) {
                    errors.push_back( { node.mID, "Use a valid integral Mikoto.KeyCode value." } );
                }

                if ( pin.mInput && pin.mType == ScriptGraphValueType::eFlow && eastl::none_of( mLinks.begin(), mLinks.end(), [&]( const auto& l ) {
                    return l.mDestination == pin.mID;
                } ) ) {
                    errors.push_back( { node.mID, "Connect the execution input." } );
                }
            }
        }

        if ( entries > 1 || updates > 1 || entries + updates == 0 ) {
            errors.push_back( { 0, "Use OnCreate and/or OnUpdate, at most one of each." } );
        }
        // Revalidate connections rather than trusting editor callers.
        ScriptGraph checked{ *this };
        checked.mLinks.clear();

        for ( const auto& link: mLinks ) {
            if ( !checked.Connect( link.mSource, link.mDestination ) ) {
                errors.push_back( { link.mDestination / 16, "Invalid connection or cycle." } );
            }
        }

        return errors;
    }

    auto ScriptGraph::InitializeDemo() -> void {
        *this = {};

        // The startup branch demonstrates data flowing into an execution decision.
        const auto begin{ AddNode( ScriptNodeType::eBeginPlay, { 0, 0 } ) };
        const auto value{ AddNode( ScriptNodeType::eFloat, { 0, 190 } ) };
        mNodes.back().mPins[0].mNumber = 5;
        const auto compare{ AddNode( ScriptNodeType::eGreater, { 420, 190 } ) };
        mNodes.back().mPins[1].mNumber = 3;
        const auto branch{ AddNode( ScriptNodeType::eBranch, { 850, 0 } ) };
        const auto yes{ AddNode( ScriptNodeType::ePrint, { 1280, 0 } ) };
        mNodes.back().mPins[1].mText = "Value is greater than three";
        const auto no{ AddNode( ScriptNodeType::ePrint, { 1280, 230 } ) };
        mNodes.back().mPins[1].mText = "Value is three or less";

        ( void )Connect( begin * 16 + 1, branch * 16 + 1 );
        ( void )Connect( value * 16 + 1, compare * 16 + 1 );
        ( void )Connect( compare * 16 + 3, branch * 16 + 2 );
        ( void )Connect( branch * 16 + 3, yes * 16 + 1 );
        ( void )Connect( branch * 16 + 4, no * 16 + 1 );

        // The update branch moves only while its keyboard condition is true.
        const auto update{ AddNode( ScriptNodeType::eOnUpdate, { 0, 550 } ) };
        const auto key{ AddNode( ScriptNodeType::eKeyPressed, { 0, 750 } ) };
        const auto moveBranch{ AddNode( ScriptNodeType::eBranch, { 450, 550 } ) };
        const auto position{ AddNode( ScriptNodeType::eGetPosition, { 450, 850 } ) };
        const auto add{ AddNode( ScriptNodeType::eVectorAdd, { 1000, 850 } ) };
        mNodes.back().mPins[1].mVector = { 0.01f, 0, 0 };
        const auto set{ AddNode( ScriptNodeType::eSetPosition, { 1500, 550 } ) };

        Connect( update * 16 + 1, moveBranch * 16 + 1 );
        Connect( key * 16 + 2, moveBranch * 16 + 2 );
        Connect( moveBranch * 16 + 3, set * 16 + 1 );
        Connect( position * 16 + 1, add * 16 + 1 );
        Connect( add * 16 + 3, set * 16 + 2 );
    }

    auto ScriptGraph::Serialize() const -> YAML::Node {
        YAML::Node doc;
        doc["Version"] = 1;
        doc["Nodes"] = YAML::Node( YAML::NodeType::Sequence );
        doc["Links"] = YAML::Node( YAML::NodeType::Sequence );

        for ( const auto& node: mNodes ) {
            YAML::Node n;
            n["ID"] = node.mID;
            n["Type"] = static_cast<int>( node.mType );
            n["Name"] = node.mName.c_str();
            n["Binding"] = node.mBinding.c_str();
            n["X"] = node.mPosition.x;
            n["Y"] = node.mPosition.y;
            n["Values"] = YAML::Node( YAML::NodeType::Sequence );

            for ( const auto& pin: node.mPins ) {
                YAML::Node v;
                v["Number"] = pin.mNumber;

                for ( int c = 0; c < 3; ++c ) {
                    v["Vector"].push_back( pin.mVector[c] );
                }

                v["Boolean"] = pin.mBoolean;
                v["Text"] = pin.mText.c_str();
                n["Values"].push_back( v );
            }

            doc["Nodes"].push_back( n );
        }

        for ( const auto& link: mLinks ) {
            YAML::Node l;
            l["Source"] = link.mSource;
            l["Destination"] = link.mDestination;
            doc["Links"].push_back( l );
        }

        return doc;
    }

    auto ScriptGraph::Deserialize( const YAML::Node& doc, eastl::string& error ) -> bool {
        try {
            if ( doc["Version"].as<int>() != 1 || !doc["Nodes"].IsSequence() || !doc["Links"].IsSequence() || doc["Nodes"].size() > 4096 || doc["Links"].size() > 16384 ) {
                MKT_THROW_RUNTIME_ERROR( "Unsupported or oversized script graph." );
            }

            ScriptGraph candidate;

            for ( const auto& n: doc["Nodes"] ) {
                const auto id{ n["ID"].as<core::u64>() };
                const auto type{ n["Type"].as<int>() };

                if ( id == 0 || id > 1000000 || type < 0 || type > static_cast<int>( ScriptNodeType::eVectorAdd ) || eastl::any_of( candidate.mNodes.begin(), candidate.mNodes.end(), [&]( const auto& v ) {
                    return v.mID == id;
                } ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid node ID or type." );
                }

                candidate.mNextID = id;
                candidate.AddNode( static_cast<ScriptNodeType>( type ) );
                auto& node{ candidate.mNodes.back() };
                node.mName = n["Name"].as<std::string>().c_str();
                node.mPosition = { n["X"].as<float>(), n["Y"].as<float>() };

                if ( n["Values"].size() != node.mPins.size() ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid node values or position." );
                }

                for ( core::usize i{}; i < node.mPins.size(); ++i ) {
                    auto& pin{ node.mPins[i] };
                    const auto v{ n["Values"][i] };
                    pin.mNumber = v["Number"].as<float>();

                    if ( v["Vector"] ) {
                        if ( !v["Vector"].IsSequence() || v["Vector"].size() != 3 ) {
                            MKT_THROW_RUNTIME_ERROR( "Invalid vector." );
                        }

                        for ( int c = 0; c < 3; ++c ) {
                            pin.mVector[c] = v["Vector"][c].as<float>();
                        }
                    }

                    pin.mBoolean = v["Boolean"].as<bool>();
                    pin.mText = v["Text"].as<std::string>().c_str();

                    if ( pin.mType == ScriptGraphValueType::eKeyCode && ( pin.mNumber < 0 || pin.mNumber > 511 || glm::floor( pin.mNumber ) != pin.mNumber ) ) {
                        MKT_THROW_RUNTIME_ERROR( "Use a valid integral Mikoto.KeyCode value." );
                    }
                }
            }

            for ( const auto& node: candidate.mNodes ) {
                candidate.mNextID = eastl::max( candidate.mNextID, node.mID + 1 );
            }

            for ( const auto& l: doc["Links"] ) {
                if ( !candidate.Connect( l["Source"].as<core::u64>(), l["Destination"].as<core::u64>() ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid script connection." );
                }
            }

            *this = eastl::move( candidate );
            error.clear();
            return true;
        } catch ( const std::exception& e ) {
            error = e.what();
            return false;
        }
    }

    auto ScriptGraph::GetNodes() -> eastl::vector<ScriptNode>& {
        return mNodes;
    }

    auto ScriptGraph::GetNodes() const -> const eastl::vector<ScriptNode>& {
        return mNodes;
    }

    auto ScriptGraph::GetLinks() const -> const eastl::vector<ScriptLink>& {
        return mLinks;
    }

    auto ScriptGraphCompiler::Compile( const ScriptGraph& graph, ScriptLanguage language, eastl::string_view name ) const -> ScriptCompileResult {
        ScriptCompileResult result{};
        result.mDiagnostics = graph.Validate();

        if ( language != ScriptLanguage::eCSharp && language != ScriptLanguage::eLua && language != ScriptLanguage::eNative ) {
            result.mDiagnostics.push_back( { 0, "Select a script language." } );
        }
        else if ( language == ScriptLanguage::eCSharp && !IsScriptIdentifier( name ) ) {
            result.mDiagnostics.push_back( { 0, "Use a C# filename starting with a letter or underscore and containing only letters, digits and underscores." } );
        }

        if ( !result.mDiagnostics.empty() ) {
            return result;
        }

        ScriptSourceWriter{ graph, language, result }.Write( name );
        result.mSuccess = result.mDiagnostics.empty();

        if ( !result.mSuccess ) {
            result.mSource.clear();
        }

        return result;
    }
}// namespace mikoto::scripting
