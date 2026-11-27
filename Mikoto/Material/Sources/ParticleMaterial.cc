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

#include <glm/gtx/compatibility.hpp>

#include <Assets/AssetRegistryService.hh>

#include <Core/Core.hh>
#include <Core/Exception.hh>
#include <Core/Types.hh>

#include <Filesystem/FileService.hh>

#include <Material/ParticleMaterial.hh>

namespace mikoto::material {

    using namespace mikoto::core;

    static auto WriteColor( const core::float4 &color ) -> YAML::Node {
        YAML::Node node;

        for ( core::i32 component{}; component < 4; ++component ) {
            node.push_back( color[component] );
        }

        return node;
    }

    static auto ReadColor( const YAML::Node &node ) -> core::float4 {
        if ( !node.IsSequence() || node.size() != 4 ) {
            MKT_THROW_RUNTIME_ERROR( "Material color needs four components." );
        }

        core::float4 color{};

        for ( core::i32 component{}; component < 4; ++component ) {
            color[component] = node[component].as<core::f32>();
        }

        return color;
    }


    ParticleMaterial::ParticleMaterial( eastl::string_view name )
        : mName{ name } {
    }

    ParticleMaterial::ParticleMaterial( const ParticleMaterialDescription &description )
        : mName{ description.mName } {
        SetColor( description.mColor );
        SetBloomEnabled( description.mBloomEnabled );
        SetBloomIntensity( description.mBloomIntensity );
    }

    ParticleMaterial::ParticleMaterial( const ParticleMaterial &other )
        : mName{ other.mName },
          mColor{ other.mColor },
          mBloomEnabled{ other.mBloomEnabled },
          mBloomIntensity{ other.mBloomIntensity },
          mGraph{ other.mGraph },
          mTexture{ other.mTexture } {
    }

    auto ParticleMaterial::SetBloomEnabled( bool enabled ) -> void {
        mBloomEnabled = enabled;
    }

    auto ParticleMaterial::SetBloomIntensity( core::f32 intensity ) -> void {
        mBloomIntensity = std::isfinite( intensity ) ? glm::clamp( intensity, 0.0f, 1000.0f ) : 0.0f;
    }

    auto ParticleMaterial::IsBloomEnabled() const -> bool {
        return mBloomEnabled;
    }

    auto ParticleMaterial::GetBloomIntensity() const -> core::f32 {
        return mBloomIntensity;
    }

    auto ParticleMaterial::SetColor( const core::float4 &value ) -> void {
        mColor = glm::max( value, core::float4{ 0 } );
        mColor.a = glm::clamp( value.a, 0.0f, 1.0f );
    }

    auto ParticleMaterial::SetTexture( asset::AssetHandle<asset::Texture> texture ) -> void {
        mTexture = eastl::move( texture );
    }

    auto ParticleMaterial::GetTexture() const -> renderer::rhi::TextureHandle {
        const auto texture{ mTexture.GetIfReady() };
        return texture ? texture->GetResource() : renderer::rhi::TextureHandle{};
    }

    auto ParticleMaterial::GetTexturePath() const -> filesystem::Path {
        return mTexture.GetRef().GetPath();
    }

    auto ParticleMaterial::GetColor() const -> const core::float4 & {
        return mColor;
    }

    auto ParticleMaterial::GetGraph() -> MaterialGraph * {
        return &mGraph;
    }

    auto ParticleMaterial::GetGraph() const -> const MaterialGraph * {
        return &mGraph;
    }

    auto ParticleMaterial::Serialize( const filesystem::Path &filename ) const -> void {
        MKT_BEGIN_PROFILER();

        YAML::Node root{};
        root["Version"] = 2;
        root["Type"] = "ParticleMaterial";
        root["Name"] = GetName().c_str();
        root["Material"] = SerializeProperties();

        // Store texture paths relative to this material file, including graph references.
        const auto directory{ filename.ToAbsolute().GetPathTyped<std::filesystem::path>().parent_path() };
        const auto relativize{ [&]( const auto &self, YAML::Node node ) -> void {
            if ( node.IsMap() ) {
                for ( const auto &entry: node ) {
                    if ( entry.first.as<std::string>() == "TexturePath" ) {
                        const auto texture{ std::filesystem::absolute( std::filesystem::u8path( entry.second.as<std::string>() ) ) };
                        const auto relative{ texture.lexically_relative( directory ) };
                        const auto utf8{ ( relative.empty() ? texture : relative ).generic_u8string() };
                        node["TexturePath"] = std::string{ reinterpret_cast<const char *>( utf8.data() ), utf8.size() };
                    }
                    else {
                        self( self, entry.second );
                    }
                }
            }
            else if ( node.IsSequence() ) {
                for ( const auto &entry: node ) {
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

    auto ParticleMaterial::Serialize( filesystem::FileHandle file ) const -> void {
        if ( !file ) {
            MKT_THROW_RUNTIME_ERROR( "Material file is null." );
        }

        Serialize( file->GetPath() );
        file->UpdateContentsFromDisk();
    }

    auto ParticleMaterial::Deserialize( const filesystem::Path &filename ) -> void {
        Deserialize( filesystem::FileHandle::New( filename ) );
    }

    auto ParticleMaterial::Deserialize( filesystem::FileHandle file ) -> void {
        if ( !file ) {
            return;
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

        if ( type != "ParticleMaterial" ) {
            MKT_THROW_RUNTIME_ERROR( "Material asset has a different type." );
        }

        // Version 1 paths were relative to the working directory; version 2 uses the file.
        const auto directory{ file->GetDirectory().GetPathTyped<std::filesystem::path>() };
        const auto resolve{ [&]( const auto &self, YAML::Node node ) -> void {
            if ( node.IsMap() ) {
                for ( const auto &entry: node ) {
                    if ( entry.first.as<std::string>() == "TexturePath" ) {
                        auto path{ std::filesystem::u8path( entry.second.as<std::string>() ) };

                        if ( path.is_relative() && version >= 2 ) {
                            path = directory / path;
                        }

                        const auto utf8{ path.lexically_normal().generic_u8string() };
                        node["TexturePath"] = std::string{ reinterpret_cast<const char *>( utf8.data() ), utf8.size() };
                    }
                    else {
                        self( self, entry.second );
                    }
                }
            }
            else if ( node.IsSequence() ) {
                for ( const auto &entry: node ) {
                    self( self, entry );
                }
            }
        } };
        resolve( resolve, root["Material"] );

        // Restore into a candidate so malformed properties leave this material unchanged.
        auto value{ core::Ref<ParticleMaterial>::New( root["Name"].as<std::string>( "Material" ).c_str() ) };
        value->RestoreProperties( root["Material"] );
        value->SetName( root["Name"].as<std::string>( "Material" ).c_str() );

        mSourcePath = file->GetPath();
        mName = eastl::move( value->mName );
        mColor = value->mColor;
        mBloomEnabled = value->mBloomEnabled;
        mBloomIntensity = value->mBloomIntensity;
        mGraph = eastl::move( value->mGraph );
        mTexture = eastl::move( value->mTexture );
    }

    auto ParticleMaterial::GetName() const -> const eastl::string & {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto ParticleMaterial::SetName( const eastl::string_view newName ) -> void {
        MKT_BEGIN_PROFILER();
        mName = newName;
    }


    auto ParticleMaterial::SerializeProperties() const -> YAML::Node {
        MKT_BEGIN_PROFILER();

        YAML::Node data{ YAML::NodeType::Map };
        data["Color"] = WriteColor( GetColor() );
        data["BloomEnabled"] = IsBloomEnabled();
        data["BloomIntensity"] = GetBloomIntensity();

        if ( const auto path{ GetTexturePath() }; !path.IsEmpty() ) {
            data["TexturePath"] = path.GetC_Str();
        }

        if ( !GetGraph()->GetNodes().empty() ) {
            data["Graph"] = GetGraph()->Serialize();
        }

        return data;
    }

    auto ParticleMaterial::RestoreProperties( const YAML::Node &node ) -> void {
        MKT_BEGIN_PROFILER();

        SetColor( ReadColor( node["Color"] ) );
        SetBloomEnabled( node["BloomEnabled"].as<bool>( false ) );
        SetBloomIntensity( node["BloomIntensity"].as<core::f32>( 4.0f ) );

        if ( const auto path{ node["TexturePath"] } ) {
            const asset::AssetRef<asset::Texture> reference{ filesystem::Path{ path.as<std::string>().c_str() } };
            SetTexture( asset::AssetRegistryService::Get()->Request<asset::Texture>( reference ) );
        }

        if ( node["Graph"] ) {
            GetGraph()->Deserialize( node["Graph"] );
        }
    }

    auto ParticleMaterial::GetSourcePath() const -> const filesystem::Path & {
        return mSourcePath;
    }

}// namespace mikoto::material
