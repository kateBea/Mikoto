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

#include <filesystem>

#include <EASTL/string_view.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Filesystem/FileService.hh>

#include <Material/Material.hh>
#include <Material/PostProcessMaterial.hh>

#include <Renderer/Rhi/Types.hh>

namespace mikoto::material {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;


    static auto WriteColor( const core::float4& color ) -> YAML::Node {
        YAML::Node node;

        for ( core::i32 component{}; component < 4; ++component ) {
            node.push_back( color[component] );
        }

        return node;
    }

    static auto ReadColor( const YAML::Node& node ) -> core::float4 {
        if ( !node.IsSequence() || node.size() != 4 ) {
            MKT_THROW_RUNTIME_ERROR( "Material color needs four components." );
        }

        core::float4 color{};

        for ( core::i32 component{}; component < 4; ++component ) {
            color[component] = node[component].as<core::f32>();
        }

        return color;
    }


    PostProcessMaterial::PostProcessMaterial( eastl::string_view name )
        : mName{ name } {
    }

    PostProcessMaterial::PostProcessMaterial( const PostProcessMaterialDescription& desc )
        : mName{ desc.mName } {
    }

    PostProcessMaterial::PostProcessMaterial( const PostProcessMaterial& other )
        : mName{ other.mName },
          mContrast{ other.mContrast },
          mSaturation{ other.mSaturation },
          mTintColor{ other.mTintColor },
          mFilmGrainEnabled{ other.mFilmGrainEnabled },
          mStage{ other.mStage },
          mCategory{ other.mCategory },
          mGraph{ other.mGraph },
          mChromaIntensity{ other.mChromaIntensity },
          mChromaDirection{ other.mChromaDirection },
          mRedChannelOffset{ other.mRedChannelOffset },
          mBlueChannelOffset{ other.mBlueChannelOffset },
          mGreenChannelOffset{ other.mGreenChannelOffset } {
    }

    PostProcessMaterial::~PostProcessMaterial() {
    }

    auto PostProcessMaterial::Serialize( const filesystem::Path& filename ) const -> void {
        MKT_BEGIN_PROFILER();

        YAML::Node root{};
        root["Version"] = 2;
        root["Type"] = "PostProcessMaterial";
        root["Name"] = GetName().c_str();
        root["Material"] = SerializeProperties();

        // Store texture paths relative to this material file, including graph references.
        const auto directory{ filename.ToAbsolute().GetPathTyped<std::filesystem::path>().parent_path() };
        const auto relativize{ [&]( const auto& self, YAML::Node node ) -> void {
            if ( node.IsMap() ) {
                for ( const auto& entry: node ) {
                    if ( entry.first.as<std::string>() == "TexturePath" ) {
                        const auto texture{ std::filesystem::absolute( std::filesystem::path{ entry.second.as<std::string>() } ) };
                        const auto relative{ texture.lexically_relative( directory ) };
                        node["TexturePath"] = ( relative.empty() ? texture : relative ).generic_string();
                    }
                    else {
                        self( self, entry.second );
                    }
                }
            }
            else if ( node.IsSequence() ) {
                for ( const auto& entry: node ) {
                    self( self, entry );
                }
            }
        } };
        relativize( relativize, root["Material"] );

        // Publish the complete document before updating the material's source path.
        YAML::Emitter emitter{};
        emitter << root;

        if ( !emitter.good() ) {
            MKT_THROW_RUNTIME_ERROR( "Could not encode material." );
        }

        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( filename ) };

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open material file for writing." );
        }

        file->Write( emitter.c_str() );
        mSourcePath = filename.ToAbsolute();
    }

    auto PostProcessMaterial::Serialize( filesystem::FileHandle file ) const -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Material file is null." );
        }

        Serialize( file->GetPath() );
        file->UpdateContentsFromDisk();
    }

    auto PostProcessMaterial::Deserialize( const filesystem::Path& filename ) -> void {
        Deserialize( filesystem::FileHandle::New( filename ) );
    }

    auto PostProcessMaterial::Deserialize( filesystem::FileHandle file ) -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Material file is null." );
        }

        MKT_BEGIN_PROFILER();

        // Validate the file envelope before restoring any material state.
        auto root{ YAML::Load( file->GetContentsString().c_str() ) };

        if ( !root.IsMap() ) {
            MKT_THROW_RUNTIME_ERROR( "Material asset must be a map." );
        }

        const auto version{ root["Version"].as<core::i32>( 0 ) };
        const auto type{ root["Type"].as<std::string>() };

        if ( ( version != 1 && version != 2 ) || !root["Material"].IsMap() ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported material file version or missing properties." );
        }

        if ( type != "PostProcessMaterial" ) {
            MKT_THROW_RUNTIME_ERROR( "Material asset has a different type." );
        }

        // Version 1 paths were relative to the working directory; version 2 uses the file.
        const auto directory{ file->GetDirectory().GetPathTyped<std::filesystem::path>() };
        const auto resolve{ [&]( const auto& self, YAML::Node node ) -> void {
            if ( node.IsMap() ) {
                for ( const auto& entry: node ) {
                    if ( entry.first.as<std::string>() == "TexturePath" ) {
                        std::filesystem::path path{ entry.second.as<std::string>() };

                        if ( path.is_relative() && version >= 2 ) {
                            path = directory / path;
                        }

                        node["TexturePath"] = path.lexically_normal().string();
                    }
                    else {
                        self( self, entry.second );
                    }
                }
            }
            else if ( node.IsSequence() ) {
                for ( const auto& entry: node ) {
                    self( self, entry );
                }
            }
        } };
        resolve( resolve, root["Material"] );

        // Restore into a candidate so malformed properties leave this material unchanged.
        auto value{ core::Ref<PostProcessMaterial>::New( root["Name"].as<std::string>( "Material" ).c_str() ) };
        value->RestoreProperties( root["Material"] );
        value->SetName( root["Name"].as<std::string>( "Material" ).c_str() );

        mSourcePath = file->GetPath();
        mGraph = eastl::move( value->mGraph );
        mName = eastl::move( value->mName );
        mContrast = eastl::move( value->mContrast );
        mSaturation = eastl::move( value->mSaturation );
        mTintColor = eastl::move( value->mTintColor );
        mStage = value->mStage;
        mCategory = value->mCategory;
        mFilmGrainEnabled = value->mFilmGrainEnabled;
    }

    auto PostProcessMaterial::SetStage( PostProcessStage stage ) -> void {
        if ( stage != PostProcessStage::eBeforeToneMapping && stage != PostProcessStage::eAfterToneMapping ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid post-process material stage." );
        }

        mStage = stage;
    }

    auto PostProcessMaterial::GetStage() const -> PostProcessStage {
        return mStage;
    }

    auto PostProcessMaterial::SetCategory( PostProcessMaterialCategory category ) -> void {
        if ( category != PostProcessMaterialCategory::eBuiltIn && category != PostProcessMaterialCategory::eCustom ) {
            MKT_THROW_INVALID_ARGUMENT( "Invalid post-process material category." );
        }

        mCategory = category;
    }

    auto PostProcessMaterial::GetCategory() const -> PostProcessMaterialCategory {
        return mCategory;
    }

    auto PostProcessMaterial::SetContrast( core::f32 value ) -> void {
        if ( value < 0.0f ) {
            MKT_THROW_INVALID_ARGUMENT( "Contrast must be nonnegative." );
        }

        mContrast = value;
    }

    auto PostProcessMaterial::GetContrast() const -> core::f32 {
        return mContrast;
    }

    auto PostProcessMaterial::SetSaturation( core::f32 value ) -> void {
        if ( value < 0.0f ) {
            MKT_THROW_INVALID_ARGUMENT( "Saturation must be nonnegative." );
        }

        mSaturation = value;
    }

    auto PostProcessMaterial::GetSaturation() const -> core::f32 {
        return mSaturation;
    }

    auto PostProcessMaterial::SetTintColor( const core::float4& value ) -> void {

        mTintColor = glm::clamp( value, core::float4{ 0.0f }, core::float4{ 1.0f } );
    }

    auto PostProcessMaterial::GetTintColor() const -> const core::float4& {
        return mTintColor;
    }

    auto PostProcessMaterial::SetFilmGrainEnabled( bool enabled ) -> void {
        mFilmGrainEnabled = enabled;
    }

    auto PostProcessMaterial::IsFilmGrainEnabled() const -> bool {
        return mFilmGrainEnabled;
    }

    auto PostProcessMaterial::Evaluate( const core::float3& color ) const -> core::float3 {
        const auto luminance{ glm::dot( color, core::float3{ 0.2126f, 0.7152f, 0.0722f } ) };
        const auto saturated{ glm::mix( core::float3{ luminance }, color, mSaturation ) };
        return glm::max( ( saturated - 0.5f ) * mContrast + 0.5f, core::float3{ 0.0f } ) * core::float3{ mTintColor };
    }

    auto PostProcessMaterial::GetGraph() -> MaterialGraph* {
        return &mGraph;
    }

    auto PostProcessMaterial::GetGraph() const -> const MaterialGraph* {
        return &mGraph;
    }

    auto PostProcessMaterial::GetName() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto PostProcessMaterial::SetName( const eastl::string_view newName ) -> void {
        MKT_BEGIN_PROFILER();
        mName = newName;
    }

    auto PostProcessMaterial::SerializeProperties() const -> YAML::Node {
        MKT_BEGIN_PROFILER();

        YAML::Node data{ YAML::NodeType::Map };
        data["Contrast"] = GetContrast();
        data["Saturation"] = GetSaturation();
        data["TintColor"] = WriteColor( GetTintColor() );
        data["Stage"] = core::as<core::u32>( GetStage() );
        data["Category"] = core::as<core::u32>( GetCategory() );
        data["FilmGrain"]["Enabled"] = IsFilmGrainEnabled();

        if ( !GetGraph()->GetNodes().empty() ) {
            data["Graph"] = GetGraph()->Serialize();
        }

        return data;
    }

    auto PostProcessMaterial::RestoreProperties( const YAML::Node& node ) -> void {
        MKT_BEGIN_PROFILER();

        SetContrast( node["Contrast"].as<core::f32>( 1.0f ) );
        SetSaturation( node["Saturation"].as<core::f32>( 1.0f ) );

        // Older material assets keep their original appearance until grain is enabled.
        if ( const auto grain{ node["FilmGrain"] } ) {
            SetFilmGrainEnabled( grain["Enabled"].as<bool>( false ) );
        }

        const auto stage{ node["Stage"].as<core::u32>( 0 ) };

        if ( stage > core::as<core::u32>( PostProcessStage::eAfterToneMapping ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid post-process material stage." );
        }

        SetStage( core::as<PostProcessStage>( stage ) );
        const auto category{ node["Category"].as<core::u32>( 0 ) };

        if ( category > core::as<core::u32>( PostProcessMaterialCategory::eCustom ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid post-process material category." );
        }

        SetCategory( core::as<PostProcessMaterialCategory>( category ) );

        if ( node["TintColor"] ) {
            SetTintColor( ReadColor( node["TintColor"] ) );
        }

        if ( node["Graph"] ) {
            GetGraph()->Deserialize( node["Graph"] );
        }
    }

    auto PostProcessMaterial::GetSourcePath() const -> const filesystem::Path& {
        return mSourcePath;
    }

}
