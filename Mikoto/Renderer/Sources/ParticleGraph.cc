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
#include <algorithm>

#include <EASTL/algorithm.h>

#include <glm/gtx/compatibility.hpp>

#include <Core/Exception.hh>

#include <Renderer/Particle/ParticleGraph.hh>
#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::renderer {
    auto ParticleGraph::Name( ParticleNodeType type ) -> const char* {
        constexpr const char* names[]{ "Emission", "Spawn", "Update", "Appearance over lifetime", "Billboard output" };
        const auto index{ static_cast<unsigned>( type ) };
        return index < 5 ? names[index] : "Invalid";
    }

    auto ParticleGraph::AddNode( ParticleNodeType type ) -> bool {
        const auto index{ static_cast<core::u32>( type ) };

        if ( index >= 5 ) {
            return false;
        }

        const auto id{ index + 1 };

        for ( const auto& n: mNodes ) {
            if ( n.mID == id ) {
                return false;
            }
        }

        mNodes.push_back( { id, type, { index * 380.0f, 0 } } );
        return true;
    }

    auto ParticleGraph::Initialize( const ParticleEmitterSettings& settings ) -> void {
        mSettings = settings;
        mSettings.Sanitize();
        mNodes.clear();
        mLinks.clear();

        for ( unsigned i = 0; i < 5; ++i ) {
            AddNode( static_cast<ParticleNodeType>( i ) );
        }

        for ( unsigned i = 1; i < 5; ++i ) {
            Connect( i, i + 1 );
        }
    }

    auto ParticleGraph::RemoveNode( core::u32 id ) -> void {
        mNodes.erase( eastl::remove_if( mNodes.begin(), mNodes.end(), [&]( const auto& n ) {
            return n.mID == id;
        } ),
            mNodes.end() );
        mLinks.erase( eastl::remove_if( mLinks.begin(), mLinks.end(), [&]( const auto& l ) {
            return l.mSource == id || l.mDestination == id;
        } ),
            mLinks.end() );
    }

    auto ParticleGraph::CanConnect( core::u32 source, core::u32 destination ) const -> bool {
        // Runtime stages have a fixed order. This also excludes branches and cycles.
        if ( source < 1 || source > 4 || destination != source + 1 ) {
            return false;
        }

        bool from{};
        bool to{};

        for ( const auto& n: mNodes ) {
            from |= n.mID == source;
            to |= n.mID == destination;
        }

        if ( !from || !to ) {
            return false;
        }

        for ( const auto& l: mLinks ) {
            if ( l.mSource == source || l.mDestination == destination ) {
                return false;
            }
        }

        return true;
    }

    auto ParticleGraph::Connect( core::u32 source, core::u32 destination ) -> bool {
        if ( !CanConnect( source, destination ) ) {
            return false;
        }

        mLinks.push_back( { source, destination } );
        return true;
    }

    auto ParticleGraph::Disconnect( core::u32 source ) -> void {
        mLinks.erase( eastl::remove_if( mLinks.begin(), mLinks.end(), [&]( const auto& l ) {
            return l.mSource == source;
        } ),
            mLinks.end() );
    }

    auto ParticleGraph::Serialize() const -> YAML::Node {
        YAML::Node document;
        document["ParticleGraphVersion"] = 1;
        YAML::Emitter emitter;
        mSettings.Serialize( emitter );
        document["Settings"] = YAML::Load( emitter.c_str() );
        document["Nodes"] = YAML::Node( YAML::NodeType::Sequence );
        document["Links"] = YAML::Node( YAML::NodeType::Sequence );

        for ( const auto& n: mNodes ) {
            YAML::Node node;
            node["Type"] = static_cast<unsigned>( n.mType );
            node["X"] = n.mPosition.x;
            node["Y"] = n.mPosition.y;
            document["Nodes"].push_back( node );
        }

        for ( const auto& l: mLinks ) {
            YAML::Node link;
            link["Source"] = l.mSource;
            link["Destination"] = l.mDestination;
            document["Links"].push_back( link );
        }

        return document;
    }

    auto ParticleGraph::Deserialize( const YAML::Node& document, eastl::string& error ) -> bool {
        try {
            if ( document["ParticleGraphVersion"].as<int>( 0 ) != 1 || !document["Nodes"].IsSequence() ||
                 !document["Links"].IsSequence() || document["Nodes"].size() > 5 || document["Links"].size() > 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid particle graph format." );
            }

            ParticleGraph candidate;
            candidate.mSettings = ParticleEmitterSettings::Deserialize( document["Settings"] );

            for ( const auto& item: document["Nodes"] ) {
                if ( !candidate.AddNode( static_cast<ParticleNodeType>( item["Type"].as<unsigned>() ) ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid or duplicate particle stage." );
                }

                auto& p = candidate.mNodes.back().mPosition;
                p = { item["X"].as<float>(), item["Y"].as<float>() };
            }

            for ( const auto& item: document["Links"] ) {
                if ( !candidate.Connect( item["Source"].as<unsigned>(), item["Destination"].as<unsigned>() ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid particle graph connection." );
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

    auto ParticleGraphCompiler::Compile( const ParticleGraph& graph ) -> ParticleGraphCompileResult {
        ParticleGraph validated;
        eastl::string error;

        if ( !validated.Deserialize( graph.Serialize(), error ) ) {
            return { false, {}, error };
        }

        if ( validated.mNodes.size() != 5 || validated.mLinks.size() != 4 ) {
            return { false, {}, "Connect Emission -> Spawn -> Update -> Appearance -> Billboard output. Preview keeps the last valid recipe." };
        }

        auto settings{ validated.mSettings };
        settings.Sanitize();
        return { true, settings, "Compiled to emitter recipe (CPU/GPU)." };
    }
}// namespace mikoto::renderer
