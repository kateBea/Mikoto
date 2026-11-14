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

#ifndef MIKOTO_RENDERER_PARTICLE_GRAPH_HH
#define MIKOTO_RENDERER_PARTICLE_GRAPH_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <Renderer/Particle/ParticleEmitter.hh>

namespace mikoto::renderer {
    enum class ParticleNodeType {
        eEmission,
        eSpawn,
        eUpdate,
        eAppearance,
        eOutput
    };

    struct ParticleGraphNode {
        core::u32 mID{};
        ParticleNodeType mType{};
        core::float2 mPosition{};
    };

    struct ParticleGraphLink {
        core::u32 mSource{};
        core::u32 mDestination{};
    };

    /** Authoring graph for the fixed stages supported by the current emitter runtime. */
    class ParticleGraph {
    public:
        ParticleEmitterSettings mSettings{};
        eastl::vector<ParticleGraphNode> mNodes{};
        eastl::vector<ParticleGraphLink> mLinks{};
        auto Initialize( const ParticleEmitterSettings& settings = {} ) -> void;
        auto AddNode( ParticleNodeType type ) -> bool;
        auto RemoveNode( core::u32 id ) -> void;
        auto CanConnect( core::u32 source, core::u32 destination ) const -> bool;
        auto Connect( core::u32 source, core::u32 destination ) -> bool;
        auto Disconnect( core::u32 source ) -> void;
        auto Serialize() const -> YAML::Node;
        auto Deserialize( const YAML::Node& document, eastl::string& error ) -> bool;
        static auto Name( ParticleNodeType type ) -> const char*;
    };

    struct ParticleGraphCompileResult {
        bool mSuccess{};
        ParticleEmitterSettings mSettings{};
        eastl::string mMessage{};
    };

    /** Lowers a valid stage chain into the existing CPU/GPU emitter recipe, not shader code. */
    class ParticleGraphCompiler {
    public:
        static auto Compile( const ParticleGraph& graph ) -> ParticleGraphCompileResult;
    };
}// namespace mikoto::renderer

#endif // MIKOTO_RENDERER_PARTICLE_GRAPH_HH
