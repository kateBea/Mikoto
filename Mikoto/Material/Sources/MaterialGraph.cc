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

#include <EASTL/algorithm.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/unordered_set.h>
#include <EASTL/unordered_map.h>

#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Material/MaterialGraph.hh>

namespace mikoto::material {
    using namespace core;


    using N = MaterialNodeType;
    using V = MaterialValueType;

    static auto CommonNodes() -> eastl::vector<N> {
        return { N::eBool, N::eInt, N::eFloat, N::eFloat2, N::eFloat3, N::eFloat4,
            N::eColor3, N::eColor4, N::eAdd, N::eSubtract, N::eMultiply,
            N::eDivide, N::ePower, N::eClamp, N::eLerp, N::eTexture2D };
    }

    static auto Rules( std::initializer_list<N> nodes, N output ) -> MaterialGraphRules {
        MaterialGraphRules result{ CommonNodes(), output, output == N::eSurfaceOutput ? N::eStandardSurface : N::eColor3 };
        result.mAllowedNodes.insert( result.mAllowedNodes.end(), nodes.begin(), nodes.end() );
        result.mAllowedNodes.push_back( output );
        return result;
    }

    static auto Components( V type ) -> core::i32 {
        switch ( type ) {
            case V::eFloat:
                return 1;
            case V::eFloat2:
                return 2;
            case V::eFloat3:
            case V::eColor3:
                return 3;
            case V::eFloat4:
            case V::eColor4:
                return 4;
            default:
                return 0;
        }
    }


    static auto TypeName( MaterialValueType type ) -> core::cstr {
        switch ( type ) {
            case MaterialValueType::eBool:
                return "bool";
            case MaterialValueType::eInt:
                return "int";
            case MaterialValueType::eFloat:
                return "float";
            case MaterialValueType::eFloat2:
                return "float2";
            case MaterialValueType::eFloat3:
                return "float3";
            case MaterialValueType::eFloat4:
                return "float4";
            case MaterialValueType::eColor3:
                return "color3";
            case MaterialValueType::eColor4:
                return "color4";
            case MaterialValueType::eSurface:
                return "surface";
            default:
                MKT_THROW_RUNTIME_ERROR( "Unsupported material graph value type." );
        }
    }

    static auto NodeTypeName( MaterialNodeType type ) -> core::cstr {
        switch ( type ) {
#define MKT_NODE_TYPE( name )       \
    case MaterialNodeType::e##name: \
        return #name
            MKT_NODE_TYPE( Bool );
            MKT_NODE_TYPE( Int );
            MKT_NODE_TYPE( Float );
            MKT_NODE_TYPE( Float2 );
            MKT_NODE_TYPE( Float3 );
            MKT_NODE_TYPE( Float4 );
            MKT_NODE_TYPE( Color3 );
            MKT_NODE_TYPE( Color4 );
            MKT_NODE_TYPE( Add );
            MKT_NODE_TYPE( Subtract );
            MKT_NODE_TYPE( Multiply );
            MKT_NODE_TYPE( Divide );
            MKT_NODE_TYPE( Power );
            MKT_NODE_TYPE( Clamp );
            MKT_NODE_TYPE( Lerp );
            MKT_NODE_TYPE( Texture2D );
            MKT_NODE_TYPE( NormalMap );
            MKT_NODE_TYPE( SurfaceOutput );
            MKT_NODE_TYPE( SceneColor );
            MKT_NODE_TYPE( SceneDepth );
            MKT_NODE_TYPE( SceneNormal );
            MKT_NODE_TYPE( ScreenUV );
            MKT_NODE_TYPE( PostProcessOutput );
            MKT_NODE_TYPE( WorldPosition );
            MKT_NODE_TYPE( WorldNormal );
            MKT_NODE_TYPE( Tangent );
            MKT_NODE_TYPE( VertexColor );
            MKT_NODE_TYPE( ParticleColor );
            MKT_NODE_TYPE( ParticleAge );
            MKT_NODE_TYPE( ParticleOutput );
#undef MKT_NODE_TYPE
            case MaterialNodeType::eStandardSurface:
                return "MikotoSurface";
            default:
                MKT_THROW_RUNTIME_ERROR( "Unsupported material graph node type." );
        }
    }

    static auto Value( MaterialValueType type, const core::float4& value ) -> YAML::Node {
        core::u32 components{};

        switch ( type ) {
            case MaterialValueType::eFloat:
                components = 1;
                break;
            case MaterialValueType::eFloat2:
                components = 2;
                break;
            case MaterialValueType::eFloat3:
            case MaterialValueType::eColor3:
                components = 3;
                break;
            case MaterialValueType::eFloat4:
            case MaterialValueType::eColor4:
                components = 4;
                break;
            default:
                MKT_THROW_RUNTIME_ERROR( "Unsupported material graph literal." );
        }

        if ( components == 1 ) {
            return YAML::Node{ value.x };
        }

        YAML::Node result{ YAML::NodeType::Sequence };
        result.SetStyle( YAML::EmitterStyle::Flow );

        for ( core::u32 i{}; i < components; ++i ) {
            result.push_back( value[i] );
        }

        return result;
    }

    static auto NodeKey( NodeID id ) -> eastl::string {
        return string::Format( "node_{}", id );
    }


    MaterialGraph::MaterialGraph( MaterialGraphDomain domain )
        : mDomain{ domain } {
    }

    SurfaceMaterialGraph::SurfaceMaterialGraph()
        : MaterialGraph{ MaterialGraphDomain::eSurface } {
    }

    PostprocessMaterialGraph::PostprocessMaterialGraph()
        : MaterialGraph{ MaterialGraphDomain::ePostProcess } {
    }

    ParticleMaterialGraph::ParticleMaterialGraph()
        : MaterialGraph{ MaterialGraphDomain::eParticle } {
    }

    auto SurfaceMaterialGraph::GetRules() const -> const MaterialGraphRules& {
        static const auto rules{ Rules(
            { N::eNormalMap, N::eWorldPosition, N::eWorldNormal,
                N::eTangent, N::eVertexColor, N::eStandardSurface },
            N::eSurfaceOutput ) };
        return rules;
    }

    auto PostprocessMaterialGraph::GetRules() const -> const MaterialGraphRules& {
        static const auto rules{ Rules( { N::eSceneColor, N::eSceneDepth, N::eSceneNormal, N::eScreenUV }, N::ePostProcessOutput ) };
        return rules;
    }

    auto ParticleMaterialGraph::GetRules() const -> const MaterialGraphRules& {
        static const auto rules{ Rules( { N::eParticleColor, N::eParticleAge }, N::eParticleOutput ) };
        return rules;
    }

    auto SurfaceMaterialGraph::CreateEmpty() const -> eastl::unique_ptr<MaterialGraph> {
        return eastl::make_unique<SurfaceMaterialGraph>();
    }

    auto PostprocessMaterialGraph::CreateEmpty() const -> eastl::unique_ptr<MaterialGraph> {
        return eastl::make_unique<PostprocessMaterialGraph>();
    }

    auto ParticleMaterialGraph::CreateEmpty() const -> eastl::unique_ptr<MaterialGraph> {
        return eastl::make_unique<ParticleMaterialGraph>();
    }

    auto MaterialGraph::GetDomain() const -> MaterialGraphDomain {
        return mDomain;
    }

    auto MaterialGraph::GetNodes() const -> const eastl::vector<MaterialNode>& {
        return mNodes;
    }

    auto MaterialGraph::GetNodes() -> eastl::vector<MaterialNode>& {
        return mNodes;
    }

    auto MaterialGraph::GetLinks() const -> const eastl::vector<MaterialLink>& {
        return mLinks;
    }

    auto MaterialGraph::GetLinks() -> eastl::vector<MaterialLink>& {
        return mLinks;
    }

    auto MaterialGraph::GetComments() -> eastl::vector<MaterialGraphComment>& {
        return mComments;
    }

    auto MaterialGraph::GetComments() const -> const eastl::vector<MaterialGraphComment>& {
        return mComments;
    }

    auto MaterialGraph::FindNode( NodeID id ) const -> const MaterialNode* {
        const auto found{ eastl::find_if( mNodes.begin(), mNodes.end(), [id]( const auto& node ) {
            return node.mID == id;
        } ) };
        return found == mNodes.end() ? nullptr : &*found;
    }

    auto MaterialGraph::FindNode( NodeID id ) -> MaterialNode* {
        return const_cast<MaterialNode*>( static_cast<const MaterialGraph*>( this )->FindNode( id ) );
    }

    auto MaterialGraph::FindPin( PinID id ) const -> const MaterialPin* {
        for ( const auto& node: mNodes ) {
            for ( const auto& pin: node.mInputs ) {
                if ( pin.mID == id ) {
                    return &pin;
                }
            }

            for ( const auto& pin: node.mOutputs ) {
                if ( pin.mID == id ) {
                    return &pin;
                }
            }
        }

        return nullptr;
    }

    auto MaterialGraph::FindPin( PinID id ) -> MaterialPin* {
        return const_cast<MaterialPin*>( static_cast<const MaterialGraph*>( this )->FindPin( id ) );
    }

    auto MaterialGraph::GetNodeName( N type ) -> cstr {
        switch ( type ) {
#define MKT_NODE_NAME( type, name ) \
    case N::type:                   \
        return name
            MKT_NODE_NAME( eBool, "Boolean" );
            MKT_NODE_NAME( eInt, "Integer" );
            MKT_NODE_NAME( eFloat, "Float" );
            MKT_NODE_NAME( eFloat2, "Float2" );
            MKT_NODE_NAME( eFloat3, "Float3" );
            MKT_NODE_NAME( eFloat4, "Float4" );
            MKT_NODE_NAME( eColor3, "Color3" );
            MKT_NODE_NAME( eColor4, "Color4" );
            MKT_NODE_NAME( eAdd, "Add" );
            MKT_NODE_NAME( eSubtract, "Subtract" );
            MKT_NODE_NAME( eMultiply, "Multiply" );
            MKT_NODE_NAME( eDivide, "Divide" );
            MKT_NODE_NAME( ePower, "Power" );
            MKT_NODE_NAME( eClamp, "Clamp" );
            MKT_NODE_NAME( eLerp, "Lerp" );
            MKT_NODE_NAME( eTexture2D, "Texture 2D" );
            MKT_NODE_NAME( eNormalMap, "Normal map" );
            MKT_NODE_NAME( eStandardSurface, "Standard surface" );
            MKT_NODE_NAME( eSurfaceOutput, "Surface output" );
            MKT_NODE_NAME( eSceneColor, "Scene color" );
            MKT_NODE_NAME( eSceneDepth, "Scene depth" );
            MKT_NODE_NAME( eSceneNormal, "Scene normal" );
            MKT_NODE_NAME( eScreenUV, "Screen UV" );
            MKT_NODE_NAME( ePostProcessOutput, "Post-process output" );
            MKT_NODE_NAME( eWorldPosition, "World position" );
            MKT_NODE_NAME( eWorldNormal, "World normal" );
            MKT_NODE_NAME( eTangent, "Tangent" );
            MKT_NODE_NAME( eVertexColor, "Vertex color" );
            MKT_NODE_NAME( eParticleColor, "Particle color" );
            MKT_NODE_NAME( eParticleAge, "Particle age" );
            MKT_NODE_NAME( eParticleOutput, "Particle output" );
#undef MKT_NODE_NAME
            default:
                return "Invalid";
        }
    }

    auto MaterialGraph::AddNode( N type, V valueType ) -> NodeID {
        const auto& rules{ GetRules() };
        const bool typed{ ( type >= N::eAdd && type <= N::eLerp ) || type == N::eTexture2D };

        if ( typed ) {
            if ( valueType == V::eUnknown ) {
                valueType = type == N::eTexture2D ? V::eColor3 : V::eFloat3;
            }

            if ( Components( valueType ) == 0 ) {
                return kInvalidNodeID;
            }
        }
        else if ( valueType != V::eUnknown ) {
            return kInvalidNodeID;
        }

        if ( eastl::find( rules.mAllowedNodes.begin(), rules.mAllowedNodes.end(), type ) == rules.mAllowedNodes.end() ) {
            return kInvalidNodeID;
        }

        if ( type == rules.mRequiredOutputNode && eastl::any_of( mNodes.begin(), mNodes.end(), [type]( const auto& n ) {
            return n.mType == type;
        } ) ) {
            return kInvalidNodeID;
        }

        const NodeID id{ mNextNodeID++ };
        mNodes.push_back( MaterialNode{ .mID = id, .mType = type, .mName = GetNodeName( type ) } );
        mNodes.back().mValueType = valueType;
        const auto input{ [&]( core::cstr name, V value, core::float4 initial = core::float4{}, bool required = false ) {
            const auto pin{ AddInputPin( id, name, value ) };
            auto* p{ FindPin( pin ) };
            p->mValue = initial;
            p->mRequired = required;
        } };
        const auto output{ [&]( V value ) {
            AddOutputPin( id, "out", value );
        } };

        switch ( type ) {
            case N::eBool:
                output( V::eBool );
                break;
            case N::eInt:
                output( V::eInt );
                break;
            case N::eFloat:
                output( V::eFloat );
                break;
            case N::eFloat2:
                output( V::eFloat2 );
                break;
            case N::eFloat3:
                output( V::eFloat3 );
                break;
            case N::eFloat4:
                output( V::eFloat4 );
                break;
            case N::eColor3:
                output( V::eColor3 );
                break;
            case N::eColor4:
                output( V::eColor4 );
                break;
            case N::eTexture2D:
                input( "texcoord", V::eFloat2 );
                output( valueType );
                break;
            case N::eWorldPosition:
            case N::eWorldNormal:
            case N::eTangent:
            case N::eSceneNormal:
                output( V::eFloat3 );
                break;
            case N::eVertexColor:
            case N::eParticleColor:
            case N::eSceneColor:
                output( V::eColor3 );
                break;
            case N::eParticleAge:
            case N::eSceneDepth:
                output( V::eFloat );
                break;
            case N::eScreenUV:
                output( V::eFloat2 );
                break;
            case N::eNormalMap:
                input( "in", V::eFloat3, core::float4{ .5f, .5f, 1, 0 } );
                input( "scale", V::eFloat, core::float4{ 1 } );
                output( V::eFloat3 );
                break;
            case N::eStandardSurface:
                input( "base_color", V::eColor3, core::float4{ .8f } );
                input( "metalness", V::eFloat );
                input( "specular_roughness", V::eFloat, core::float4{ .5f } );
                input( "emission", V::eFloat );
                input( "emission_color", V::eColor3, core::float4{ 1 } );
                input( "opacity", V::eColor3, core::float4{ 1 } );
                input( "normal", V::eFloat3 );
                output( V::eSurface );
                break;
            case N::eSurfaceOutput:
                input( "surfaceshader", V::eSurface, {}, true );
                break;
            case N::ePostProcessOutput:
            case N::eParticleOutput:
                input( "color", V::eColor3, {}, true );
                input( "opacity", V::eFloat, core::float4{ 1 } );
                break;
            case N::eClamp:
                input( "in", valueType );
                input( "low", valueType );
                input( "high", valueType, core::float4{ 1 } );
                output( valueType );
                break;
            case N::eLerp:
                input( "bg", valueType );
                input( "fg", valueType, core::float4{ 1 } );
                input( "mix", V::eFloat, core::float4{ .5f } );
                output( valueType );
                break;
            default:
                input( "in1", valueType );
                input( "in2", valueType, core::float4{ 1 } );
                output( valueType );
                break;
        }

        return id;
    }

    auto MaterialGraph::AddInputPin( NodeID node, eastl::string_view name, V type ) -> PinID {
        auto* target{ FindNode( node ) };

        if ( !target ) {
            return kInvalidPinID;
        }

        const auto id{ mNextPinID++ };
        target->mInputs.push_back( { id, node, eastl::string{ name }, type, PinDirection::eInput } );
        return id;
    }

    auto MaterialGraph::AddOutputPin( NodeID node, eastl::string_view name, V type ) -> PinID {
        auto* target{ FindNode( node ) };

        if ( !target ) {
            return kInvalidPinID;
        }

        const auto id{ mNextPinID++ };
        target->mOutputs.push_back( { id, node, eastl::string{ name }, type, PinDirection::eOutput } );
        return id;
    }

    auto MaterialGraph::CanConnect( V source, V destination ) -> bool {
        if ( source == V::eUnknown || destination == V::eUnknown ) {
            return false;
        }

        return source == destination || ( Components( source ) && Components( destination ) &&
                                            ( Components( source ) == Components( destination ) || source == V::eFloat ) );
    }

    auto MaterialGraph::CanConnect( PinID source, PinID destination ) const -> bool {
        const auto* from{ FindPin( source ) };
        const auto* to{ FindPin( destination ) };

        if ( !from || !to || from->mDirection != PinDirection::eOutput || to->mDirection != PinDirection::eInput || !CanConnect( from->mType, to->mType ) ) {
            return false;
        }
        // Walk downstream from the destination: connecting back to the source would create a cycle.
        eastl::vector<NodeID> pending{ to->mNodeID };
        eastl::unordered_set<NodeID> visited{};

        while ( !pending.empty() ) {
            const auto node{ pending.back() };
            pending.pop_back();

            if ( node == from->mNodeID ) {
                return false;
            }

            if ( !visited.insert( node ).second ) {
                continue;
            }

            for ( const auto& link: mLinks ) {
                const auto* a{ FindPin( link.mSource ) };
                const auto* b{ FindPin( link.mDestination ) };

                if ( a && b && a->mNodeID == node ) {
                    pending.push_back( b->mNodeID );
                }
            }
        }

        return true;
    }

    auto MaterialGraph::Connect( PinID source, PinID destination ) -> LinkID {
        if ( !CanConnect( source, destination ) ) {
            return kInvalidLinkID;
        }

        for ( const auto& link: mLinks ) {
            if ( link.mSource == source && link.mDestination == destination ) {
                return link.mID;
            }
        }
        // One producer per input. Rewiring is atomic after validation.
        eastl::erase_if( mLinks, [destination]( const auto& link ) {
            return link.mDestination == destination;
        } );
        const auto id{ mNextLinkID++ };
        mLinks.push_back( { id, source, destination } );
        return id;
    }

    auto MaterialGraph::Disconnect( LinkID id ) -> void {
        eastl::erase_if( mLinks, [id]( const auto& link ) {
            return link.mID == id;
        } );
    }

    auto MaterialGraph::RemoveNode( NodeID id ) -> void {
        eastl::erase_if( mLinks, [&]( const auto& link ) {
            const auto* a{ FindPin( link.mSource ) };
            const auto* b{ FindPin( link.mDestination ) };
            return !a || !b || a->mNodeID == id || b->mNodeID == id;
        } );
        eastl::erase_if( mNodes, [id]( const auto& node ) {
            return node.mID == id;
        } );
    }

    auto MaterialGraph::Clear() -> void {
        mNodes.clear();
        mLinks.clear();
        mComments.clear();
    }

    auto MaterialGraph::InitializeDefault() -> void {
        if ( !mNodes.empty() ) {
            return;
        }

        const auto output{ AddNode( GetRules().mRequiredOutputNode ) };
        const auto source{ AddNode( GetRules().mDefaultSourceNode ) };
        FindNode( output )->mPosition = { 400, 60 };
        FindNode( source )->mPosition = { 30, 60 };
        FindNode( source )->mValue = core::float4{ 1 };
        Connect( FindNode( source )->mOutputs.front().mID, FindNode( output )->mInputs.front().mID );
    }

    auto MaterialGraph::AddComment( eastl::string_view name, core::float2 position ) -> NodeID {
        const auto id{ mNextNodeID++ };
        mComments.push_back( { id, eastl::string{ name }, position } );
        return id;
    }

    auto MaterialGraph::RemoveComment( NodeID id ) -> void {
        eastl::erase_if( mComments, [id]( const auto& comment ) {
            return comment.mID == id;
        } );
    }

    auto MaterialGraph::Validate() const -> eastl::vector<eastl::string> {
        eastl::vector<eastl::string> errors{};
        const auto& rules{ GetRules() };
        core::usize outputs{};
        auto schema{ CreateEmpty() };
        eastl::unordered_set<NodeID> nodes{};
        eastl::unordered_set<PinID> pins{};
        eastl::unordered_set<PinID> inputs{};

        for ( const auto& node: mNodes ) {
            if ( !nodes.insert( node.mID ).second || node.mID == kInvalidNodeID ) {
                errors.push_back( "Duplicate or invalid node ID." );
            }

            if ( node.mType == rules.mRequiredOutputNode ) {
                ++outputs;
            }

            if ( eastl::find( rules.mAllowedNodes.begin(), rules.mAllowedNodes.end(), node.mType ) == rules.mAllowedNodes.end() ) {
                errors.push_back( "Node is not allowed in this domain." );
            }

            schema->Clear();
            const auto schemaID{ schema->AddNode( node.mType, node.mValueType ) };

            if ( const auto* expected{ schema->FindNode( schemaID ) } ) {
                const auto matches{ []( const auto& actual, const auto& definition ) {
                    if ( actual.size() != definition.size() ) {
                        return false;
                    }

                    for ( core::usize i{}; i < actual.size(); ++i ) {
                        if ( actual[i].mName != definition[i].mName || actual[i].mType != definition[i].mType || actual[i].mDirection != definition[i].mDirection || actual[i].mRequired != definition[i].mRequired ) {
                            return false;
                        }
                    }

                    return true;
                } };

                if ( !matches( node.mInputs, expected->mInputs ) || !matches( node.mOutputs, expected->mOutputs ) ) {
                    errors.push_back( "Node pin schema does not match its type." );
                }
            }
            else {
                errors.push_back( "Node value type is not supported." );
            }

            for ( const auto* list: { &node.mInputs, &node.mOutputs } ) {
                for ( const auto& pin: *list ) {
                    if ( !pins.insert( pin.mID ).second || pin.mNodeID != node.mID || pin.mID == kInvalidPinID ) {
                        errors.push_back( "Invalid pin identity." );
                    }

                    if ( pin.mRequired && eastl::none_of( mLinks.begin(), mLinks.end(), [&]( const auto& link ) {
                        return link.mDestination == pin.mID;
                    } ) ) {
                        errors.push_back( eastl::string{ "Required input is disconnected: " } + node.mName + "." + pin.mName );
                    }
                }
            }
        }

        if ( outputs != 1 ) {
            errors.push_back( "Graph requires exactly one domain output." );
        }

        for ( const auto& link: mLinks ) {
            if ( !CanConnect( link.mSource, link.mDestination ) ) {
                errors.push_back( "Invalid connection or cycle." );
            }

            if ( !inputs.insert( link.mDestination ).second ) {
                errors.push_back( "An input has multiple producers." );
            }
        }

        return errors;
    }

    auto MaterialGraph::Serialize() const -> YAML::Node {
        YAML::Node data{};
        data["Version"] = 1;
        data["Domain"] = static_cast<core::u32>( mDomain );

        for ( const auto& node: mNodes ) {
            YAML::Node n{};
            n["Inputs"] = YAML::Node{ YAML::NodeType::Sequence };
            n["ID"] = node.mID;
            n["Type"] = static_cast<core::u32>( node.mType );
            n["ValueType"] = static_cast<core::u32>( node.mValueType );
            n["Name"] = node.mName.c_str();
            n["Integer"] = node.mIntegerValue;
            n["Boolean"] = node.mBooleanValue;
            n["X"] = node.mPosition.x;
            n["Y"] = node.mPosition.y;

            if ( !node.mTexturePath.empty() ) {
                n["TexturePath"] = node.mTexturePath.c_str();
            }

            for ( core::i32 i{}; i < 4; ++i ) {
                n["Value"].push_back( node.mValue[i] );
            }

            for ( const auto& pin: node.mInputs ) {
                YAML::Node p{};

                for ( core::i32 i{}; i < 4; ++i ) {
                    p.push_back( pin.mValue[i] );
                }

                n["Inputs"].push_back( p );
            }

            data["Nodes"].push_back( n );
        }

        for ( const auto& link: mLinks ) {
            const auto* a{ FindPin( link.mSource ) };
            const auto* b{ FindPin( link.mDestination ) };

            if ( !a || !b ) {
                continue;
            }

            YAML::Node l{};
            l["From"] = a->mNodeID;
            l["Output"] = a->mName.c_str();
            l["To"] = b->mNodeID;
            l["Input"] = b->mName.c_str();
            data["Links"].push_back( l );
        }

        for ( const auto& comment: mComments ) {
            YAML::Node c{};
            c["Name"] = comment.mName.c_str();
            c["X"] = comment.mPosition.x;
            c["Y"] = comment.mPosition.y;
            c["Width"] = comment.mSize.x;
            c["Height"] = comment.mSize.y;
            data["Comments"].push_back( c );
        }

        return data;
    }

    auto MaterialGraph::Deserialize( const YAML::Node& data ) -> void {
        if ( data["Version"].as<core::u32>( 0 ) != 1 || data["Domain"].as<core::u32>() != static_cast<core::u32>( mDomain ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid material graph version or domain." );
        }

        auto next{ CreateEmpty() };
        eastl::vector<eastl::pair<NodeID, NodeID>> remap{};
        const auto scalar{ []( const YAML::Node& value ) {
            const auto f{ value.as<core::f32>() };
            return f;
        } };

        for ( const auto& n: data["Nodes"] ) {
            const auto old{ n["ID"].as<NodeID>() };

            if ( eastl::any_of( remap.begin(), remap.end(), [old]( const auto& pair ) {
                return pair.first == old;
            } ) ) {
                MKT_THROW_RUNTIME_ERROR( "Duplicate graph node ID." );
            }

            const auto id{ next->AddNode( static_cast<N>( n["Type"].as<core::u16>() ), static_cast<V>( n["ValueType"].as<core::u8>( 0 ) ) ) };

            if ( id == kInvalidNodeID ) {
                MKT_THROW_RUNTIME_ERROR( "Illegal graph node." );
            }

            remap.push_back( { old, id } );

            auto* node{ next->FindNode( id ) };
            node->mName = n["Name"].as<std::string>().c_str();
            node->mIntegerValue = n["Integer"].as<core::i32>( 0 );
            node->mBooleanValue = n["Boolean"].as<bool>( false );
            node->mPosition = { scalar( n["X"] ), scalar( n["Y"] ) };
            node->mTexturePath = n["TexturePath"].as<std::string>( "" ).c_str();

            if ( n["Value"].size() != 4 || n["Inputs"].size() != node->mInputs.size() ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid graph node values." );
            }

            for ( core::i32 i{}; i < 4; ++i ) {
                node->mValue[i] = scalar( n["Value"][i] );
            }

            for ( core::usize p{}; p < node->mInputs.size(); ++p ) {
                if ( n["Inputs"][p].size() != 4 ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid pin value." );
                }

                for ( core::i32 i{}; i < 4; ++i ) {
                    node->mInputs[p].mValue[i] = scalar( n["Inputs"][p][i] );
                }
            }
        }

        const auto pin{ [&]( const YAML::Node& id, const YAML::Node& name, bool output ) -> PinID {
            const auto old{ id.as<NodeID>() };

            for ( const auto& pair: remap ) {
                if ( pair.first == old ) {
                    const auto* node{ next->FindNode( pair.second ) };

                    for ( const auto& p: output ? node->mOutputs : node->mInputs ) {
                        if ( p.mName == name.as<std::string>().c_str() ) {
                            return p.mID;
                        }
                    }
                }
            }

            return kInvalidPinID;
        } };

        eastl::unordered_set<PinID> connected{};

        for ( const auto& l: data["Links"] ) {
            const auto a{ pin( l["From"], l["Output"], true ) };
            const auto b{ pin( l["To"], l["Input"], false ) };

            if ( !connected.insert( b ).second || next->Connect( a, b ) == kInvalidLinkID ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid graph connection." );
            }
        }

        for ( const auto& c: data["Comments"] ) {
            next->AddComment( c["Name"].as<std::string>().c_str(), { scalar( c["X"] ), scalar( c["Y"] ) } );
            next->mComments.back().mSize = { scalar( c["Width"] ), scalar( c["Height"] ) };
        }

        mNodes = eastl::move( next->mNodes );
        mLinks = eastl::move( next->mLinks );
        mComments = eastl::move( next->mComments );
        mNextNodeID = next->mNextNodeID;
        mNextPinID = next->mNextPinID;
        mNextLinkID = next->mNextLinkID;
    }


    auto MaterialGraphCompiler::Compile( const MaterialGraph& graph ) const -> CompileResult {
        CompileResult result{};

        for ( const auto& error: graph.Validate() ) {
            result.mDiagnostics.push_back( { error } );
        }

        for ( const auto& node: graph.GetNodes() ) {
            if ( node.mType == MaterialNodeType::eTexture2D && node.mTexturePath.empty() ) {
                result.mDiagnostics.push_back( { eastl::string{ "Texture node '" } + node.mName + "' needs an image path." } );
            }
        }

        if ( !result.mDiagnostics.empty() ) {
            return result;
        }

        try {
            YAML::Node document{};
            document["format"] = "Mikoto Material Graph";
            document["version"] = 1;

            switch ( graph.GetDomain() ) {
                case MaterialGraphDomain::eSurface:
                    document["domain"] = "surface";
                    break;
                case MaterialGraphDomain::ePostProcess:
                    document["domain"] = "postprocess";
                    break;
                case MaterialGraphDomain::eParticle:
                    document["domain"] = "particle";
                    break;
                default:
                    MKT_THROW_RUNTIME_ERROR( "Unsupported material graph domain." );
            }

            // Stable IDs identify nodes; display names may be duplicated or renamed.
            eastl::unordered_map<PinID, const MaterialPin*> sources{};

            for ( const auto& link: graph.GetLinks() ) {
                sources[link.mDestination] = graph.FindPin( link.mSource );
            }

            for ( const auto& node: graph.GetNodes() ) {
                YAML::Node generated{};
                generated["type"] = NodeTypeName( node.mType );
                generated["name"] = node.mName.c_str();

                if ( node.mType >= MaterialNodeType::eBool && node.mType <= MaterialNodeType::eColor4 ) {
                    if ( node.mType == MaterialNodeType::eBool ) {
                        generated["value"] = node.mBooleanValue;
                    }
                    else if ( node.mType == MaterialNodeType::eInt ) {
                        generated["value"] = node.mIntegerValue;
                    }
                    else {
                        generated["value"] = Value( node.mOutputs.front().mType, node.mValue );
                    }
                }

                if ( node.mType == MaterialNodeType::eTexture2D ) {
                    generated["texture"] = node.mTexturePath.c_str();
                }

                for ( const auto& pin: node.mInputs ) {
                    YAML::Node input{};
                    input["type"] = TypeName( pin.mType );

                    if ( const auto found{ sources.find( pin.mID ) }; found != sources.end() ) {
                        const auto& source{ *found->second };
                        input["connection"] = string::Format( "{}.{}", NodeKey( source.mNodeID ), source.mName ).c_str();
                    }
                    else if ( node.mType == MaterialNodeType::eTexture2D && pin.mName == "texcoord" ) {
                        input["semantic"] = "UV0";
                    }
                    else if ( node.mType == MaterialNodeType::eStandardSurface && pin.mName == "normal" ) {
                        input["semantic"] = "WorldNormal";
                    }
                    else {
                        input["value"] = Value( pin.mType, pin.mValue );
                    }

                    input.SetStyle( YAML::EmitterStyle::Flow );
                    generated["inputs"][pin.mName.c_str()] = input;
                }
                // Preserve source and destination types so later code generation can
                // implement the graph's validated scalar splats and color/vector conversions.
                for ( const auto& pin: node.mOutputs ) {
                    generated["outputs"][pin.mName.c_str()] = TypeName( pin.mType );
                }

                document["nodes"][NodeKey( node.mID ).c_str()] = generated;

                if ( node.mType == graph.GetRules().mRequiredOutputNode ) {
                    document["output"] = NodeKey( node.mID ).c_str();
                }
            }

            result.mSource = YAML::Dump( document ).c_str();
            result.mSuccess = true;
        } catch ( const std::exception& error ) {
            result.mDiagnostics.push_back( { error.what() } );
        }

        return result;
    }
}// namespace mikoto::material
