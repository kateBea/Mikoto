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

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Exception.hh>

#include <Filesystem/FileService.hh>

#include <Material/TextMaterial.hh>

namespace mikoto::material {

    using namespace mikoto::core;

    MKT_NODISCARD static auto WriteColor( const core::float4& color ) -> YAML::Node {
        YAML::Node node;

        for ( core::i32 component{}; component < 4; ++component ) {
            node.push_back( color[component] );
        }

        return node;
    }

    MKT_NODISCARD static auto ReadColor( const YAML::Node& node ) -> core::float4 {
        if ( !node.IsSequence() || node.size() != 4 ) {
            MKT_THROW_RUNTIME_ERROR( "Material color needs four components." );
        }

        core::float4 color{};

        for ( core::i32 component{}; component < 4; ++component ) {
            color[component] = node[component].as<core::f32>();
        }

        return color;
    }

    TextMaterial::TextMaterial( eastl::string_view name )
        : mName{ name } {
    }

    TextMaterial::TextMaterial( const TextMaterial& other )
        : mName{ other.mName },
          mColor{ other.mColor },
          mOpacity{ other.mOpacity } {
    }

    TextMaterial::~TextMaterial() = default;

    auto TextMaterial::SetColor( const core::float4& color ) -> void {
        mColor = glm::clamp( color, core::float4{ 0.0f }, core::float4{ 1.0f } );
    }

    auto TextMaterial::SetOpacity( core::f32 opacity ) -> void {
        mOpacity = glm::clamp( opacity, 0.0f, 1.0f );
    }

    auto TextMaterial::GetColor() const -> const core::float4& {
        return mColor;
    }

    auto TextMaterial::GetOpacity() const -> core::f32 {
        return mOpacity;
    }

    auto TextMaterial::Evaluate( const core::float4& tint ) const -> core::float4 {
        auto color{ mColor * tint };
        color.a *= mOpacity;
        return color;
    }

    auto TextMaterial::Serialize( const filesystem::Path& filename ) const -> void {
        MKT_BEGIN_PROFILER();

        YAML::Node root{};
        root["Version"] = 2;
        root["Type"] = "TextMaterial";
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

    auto TextMaterial::Serialize( filesystem::FileHandle file ) const -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Material file is null." );
        }

        Serialize( file->GetPath() );
        file->UpdateContentsFromDisk();
    }

    auto TextMaterial::Deserialize( const filesystem::Path& filename ) -> void {
        Deserialize( filesystem::FileHandle::New( filename ) );
    }

    auto TextMaterial::Deserialize( filesystem::FileHandle file ) -> void {
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

        if ( type != "TextMaterial" ) {
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
        auto value{ core::Ref<TextMaterial>::New( root["Name"].as<std::string>( "Material" ).c_str() ) };
        value->RestoreProperties( root["Material"] );
        value->SetName( root["Name"].as<std::string>( "Material" ).c_str() );

        mSourcePath = file->GetPath();
        mName = eastl::move( value->mName );
        mColor = eastl::move( value->mColor );
        mOpacity = eastl::move( value->mOpacity );
    }

    auto TextMaterial::GetName() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto TextMaterial::SetName( const eastl::string_view newName ) -> void {
        MKT_BEGIN_PROFILER();
        mName = newName;
    }

    auto TextMaterial::SerializeProperties() const -> YAML::Node {
        MKT_BEGIN_PROFILER();

        YAML::Node data{ YAML::NodeType::Map };
        data["Color"] = WriteColor( GetColor() );
        data["Opacity"] = GetOpacity();

        return data;
    }

    auto TextMaterial::RestoreProperties( const YAML::Node& node ) -> void {
        MKT_BEGIN_PROFILER();

        if ( node["Color"] ) {
            SetColor( ReadColor( node["Color"] ) );
        }

        SetOpacity( node["Opacity"].as<core::f32>( 1.0f ) );
    }

    auto TextMaterial::GetSourcePath() const -> const filesystem::Path& {
        return mSourcePath;
    }

}// namespace mikoto::material