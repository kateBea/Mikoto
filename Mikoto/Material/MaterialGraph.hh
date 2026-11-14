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

#ifndef MIKOTO_MATERIAL_MATERIAL_GRAPH_HH
#define MIKOTO_MATERIAL_MATERIAL_GRAPH_HH

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::material {

    using NodeID = core::u64;
    using PinID = core::u64;
    using LinkID = core::u64;

    inline constexpr NodeID kInvalidNodeID{ std::numeric_limits<NodeID>::max() };
    inline constexpr PinID kInvalidPinID{ std::numeric_limits<PinID>::max() };
    inline constexpr LinkID kInvalidLinkID{ std::numeric_limits<LinkID>::max() };

    enum class PinDirection : core::u8 {
        eInput,
        eOutput,
    };

    enum class MaterialValueType : core::u8 {
        eUnknown,

        eBool,
        eInt,
        eFloat,

        eFloat2,
        eFloat3,
        eFloat4,

        eColor3,
        eColor4,

        eMatrix3,
        eMatrix4,

        eTexture2D,
        eTextureCube,

        eSurface,
    };

    enum class MaterialGraphDomain : core::u8 {
        eUnknown,
        eSurface,
        eText,
        eSkybox,
        eParticle,
        ePostProcess,
    };

    enum class MaterialNodeType : core::u16 {
        eInvalid,

        // Constants
        eBool,
        eInt,
        eFloat,
        eFloat2,
        eFloat3,
        eFloat4,
        eColor3,
        eColor4,

        // Math
        eAdd,
        eSubtract,
        eMultiply,
        eDivide,
        ePower,
        eClamp,
        eLerp,

        // Textures
        eTexture2D,
        eTextureCube,
        eNormalMap,

        // Surface
        eStandardSurface,
        eSurfaceOutput,

        // Post process
        eSceneColor,
        eSceneDepth,
        eSceneNormal,
        eScreenUV,
        ePostProcessOutput,
        eWorldPosition,
        eWorldNormal,
        eTangent,
        eVertexColor,
        eParticleColor,
        eParticleAge,
        eParticleOutput,
    };

    /** Domain contracts belong to graph specializations, not the editor/compiler. */
    struct MaterialGraphRules {
        eastl::vector<MaterialNodeType> mAllowedNodes{};
        MaterialNodeType mRequiredOutputNode{ MaterialNodeType::eInvalid };
        MaterialNodeType mDefaultSourceNode{ MaterialNodeType::eColor3 };
    };

    /** A named, movable comment box. Contents move with the box in the editor. */
    struct MaterialGraphComment {
        NodeID mID{ kInvalidNodeID };
        eastl::string mName{ "Group" };
        core::float2 mPosition{};
        core::float2 mSize{ 400.0f, 240.0f };
    };

    struct MaterialPin {
        PinID mID{ kInvalidPinID };
        NodeID mNodeID{ kInvalidNodeID };

        eastl::string mName{};

        MaterialValueType mType{ MaterialValueType::eUnknown };
        PinDirection mDirection{ PinDirection::eInput };
        core::float4 mValue{};
        bool mRequired{};
    };

    struct MaterialNode {
        NodeID mID{ kInvalidNodeID };

        MaterialNodeType mType{ MaterialNodeType::eInvalid };

        eastl::string mName{};

        core::float2 mPosition{};
        core::float4 mValue{};
        MaterialValueType mValueType{ MaterialValueType::eUnknown };
        // Preserve integer precision; discrete constants never pass through float storage.
        core::i32 mIntegerValue{};
        bool mBooleanValue{};
        eastl::string mTexturePath{};

        eastl::vector<MaterialPin> mInputs{};
        eastl::vector<MaterialPin> mOutputs{};
    };

    struct MaterialLink {
        LinkID mID{ kInvalidLinkID };

        PinID mSource{ kInvalidPinID };
        PinID mDestination{ kInvalidPinID };
    };

    /**
     * @brief Generic editable graph storage, links, validation and persistence.
     * Domain specializations supply immutable rules. Invalid links never mutate
     * the graph; incomplete documents can still be saved while being authored.
     */
    class MaterialGraph {
    public:
        explicit MaterialGraph( MaterialGraphDomain domain );

        MKT_PUSH_DEFAULT_MOVE_AND_COPY( MaterialGraph );

        auto InitializeDefault() -> void;

        MKT_NODISCARD auto GetNodes() const -> const eastl::vector<MaterialNode>&;
        MKT_NODISCARD auto GetNodes() -> eastl::vector<MaterialNode>&;

        MKT_NODISCARD auto GetLinks() const -> const eastl::vector<MaterialLink>&;
        MKT_NODISCARD auto GetLinks() -> eastl::vector<MaterialLink>&;

        MKT_NODISCARD auto FindNode( NodeID id ) -> MaterialNode*;
        MKT_NODISCARD auto FindNode( NodeID id ) const -> const MaterialNode*;

        MKT_NODISCARD auto FindPin( PinID id ) -> MaterialPin*;
        MKT_NODISCARD auto FindPin( PinID id ) const -> const MaterialPin*;

        MKT_NODISCARD auto GetDomain() const -> MaterialGraphDomain;
        MKT_NODISCARD auto CanConnect( PinID source, PinID destination ) const -> bool;
        MKT_NODISCARD auto Validate() const -> eastl::vector<eastl::string>;

        MKT_NODISCARD virtual auto GetRules() const -> const MaterialGraphRules& = 0;

        MKT_NODISCARD auto Serialize() const -> YAML::Node;

        MKT_NODISCARD auto AddComment( eastl::string_view name, core::float2 position ) -> NodeID;

        MKT_NODISCARD auto GetComments() -> eastl::vector<MaterialGraphComment>&;
        MKT_NODISCARD auto GetComments() const -> const eastl::vector<MaterialGraphComment>&;

        auto RemoveComment( NodeID id ) -> void;

        auto Deserialize( const YAML::Node& data ) -> void;

        auto AddNode( MaterialNodeType type, MaterialValueType valueType = MaterialValueType::eUnknown ) -> NodeID;
        auto RemoveNode( NodeID id ) -> void;

        auto AddInputPin( NodeID node, eastl::string_view name, MaterialValueType type ) -> PinID;
        auto AddOutputPin( NodeID node, eastl::string_view name, MaterialValueType type ) -> PinID;

        auto Connect( PinID source, PinID destination ) -> LinkID;
        auto Disconnect( LinkID link ) -> void;

        auto Clear() -> void;

        MKT_NODISCARD static auto GetNodeName( MaterialNodeType type ) -> core::cstr;
        MKT_NODISCARD static auto CanConnect( MaterialValueType source, MaterialValueType destination ) -> bool;

        virtual ~MaterialGraph() = default;

    protected:
        MKT_NODISCARD virtual auto CreateEmpty() const -> eastl::unique_ptr<MaterialGraph> = 0;

    private:
        PinID mNextPinID{ 1 };
        NodeID mNextNodeID{ 1 };
        LinkID mNextLinkID{ 1 };

        eastl::vector<MaterialNode> mNodes{};
        eastl::vector<MaterialLink> mLinks{};
        eastl::vector<MaterialGraphComment> mComments{};

        MaterialGraphDomain mDomain{ MaterialGraphDomain::eSurface };
    };

    /** @brief PBR surface graph with a single surface output. */
    class SurfaceMaterialGraph final : public MaterialGraph {
    public:
        SurfaceMaterialGraph();

        MKT_NODISCARD auto GetRules() const -> const MaterialGraphRules& override;

    private:
        MKT_NODISCARD auto CreateEmpty() const -> eastl::unique_ptr<MaterialGraph> override;
    };

    /** @brief Screen-space graph; scene inputs are unavailable to surface graphs. */
    class PostprocessMaterialGraph final : public MaterialGraph {
    public:
        PostprocessMaterialGraph();
        MKT_NODISCARD auto GetRules() const -> const MaterialGraphRules& override;

    private:
        MKT_NODISCARD auto CreateEmpty() const -> eastl::unique_ptr<MaterialGraph> override;
    };

    /** @brief Particle appearance graph with per-particle color/age inputs. */
    class ParticleMaterialGraph final : public MaterialGraph {
    public:
        ParticleMaterialGraph();
        MKT_NODISCARD auto GetRules() const -> const MaterialGraphRules& override;

    private:
        MKT_NODISCARD auto CreateEmpty() const -> eastl::unique_ptr<MaterialGraph> override;
    };

    struct MaterialCompilerDiagnostic {
        eastl::string mMessage{};
    };

    struct CompileResult {
        bool mSuccess{};
        /** Validated, readable Mikoto graph YAML; empty on failure. */
        eastl::string mSource{};
        eastl::vector<MaterialCompilerDiagnostic> mDiagnostics{};
    };

    class MaterialGraphCompiler {
    public:
        /** @brief Exports typed nodes, named inputs and connections. Does not generate shaders.
         * Editor persistence remains owned by MaterialGraph::Serialize/Deserialize.
         */
        MKT_NODISCARD auto Compile( const MaterialGraph& graph ) const -> CompileResult;
    };
}// namespace mikoto::material

#endif// MIKOTO_MATERIAL_MATERIAL_GRAPH_HH
