//    Copyright 2025 ケイト
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

#include <stdexcept>
#include <filesystem>

#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Filesystem/FileService.hh>

#include <Assets/AssetRegistryService.hh>

#include <Material/SkyboxMaterial.hh>

namespace mikoto::material {
    static auto TexturePath( const asset::AssetHandle<asset::Texture>& texture ) -> std::string {
        if ( !texture ) {
            return {};
        }

        if ( texture.GetRef().GetPath().IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Save the material's embedded texture to an image file first." );
        }

        return texture.GetRef().GetPath().GetC_Str();
    }


    auto SkyboxMaterialDescription::SetName( eastl::string_view name ) -> SkyboxMaterialDescription& {
        MKT_BEGIN_PROFILER();
        mName = name;
        return *this;
    }

    auto SkyboxMaterialDescription::SetEquirectangular( asset::AssetHandle<asset::Texture> texture ) -> SkyboxMaterialDescription& {
        MKT_BEGIN_PROFILER();
        mEquirectangularTextureHandle = texture;
        mSkyboxType = SkyboxType::eEquirectangular;
        return *this;
    }

    auto SkyboxMaterialDescription::SetCubeFace( SkyboxFace face, asset::AssetHandle<asset::Texture> texture ) -> SkyboxMaterialDescription& {
        MKT_BEGIN_PROFILER();
        mCubeFaces[face] = texture;
        mSkyboxType = SkyboxType::eCubeFaces;
        return *this;
    }

    SkyboxMaterial::SkyboxMaterial( eastl::string_view name )
        : mName{ name } {
    }

    SkyboxMaterial::SkyboxMaterial( const SkyboxMaterialDescription& desc )
        : mName{ desc.mName },
          mCubeFaces{ desc.mCubeFaces },
          mEquirectangular{ desc.mEquirectangularTextureHandle },
          mType{ desc.mSkyboxType } {
        mCubeFaces = desc.mCubeFaces;
        mEquirectangular = desc.mEquirectangularTextureHandle;
    }

    SkyboxMaterial::SkyboxMaterial( const SkyboxMaterial& other )
        : mName{ other.mName },
          mAmbientScale{ other.mAmbientScale },
          mType{ other.mType },
          mEquirectangular{ other.mEquirectangular },
          mCubeFaces{ other.mCubeFaces } {
    }

    auto SkyboxMaterial::SetEquirectangular( asset::AssetHandle<asset::Texture> texture ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !texture.IsReady() ) {
            return;
        }

        mEquirectangular = texture;
    }

    auto SkyboxMaterial::SetFace( SkyboxFace face, asset::AssetHandle<asset::Texture> texture ) -> void {
        MKT_BEGIN_PROFILER();
        mCubeFaces[face] = texture;
    }

    auto SkyboxMaterial::SetType( SkyboxType type ) -> void {
        MKT_BEGIN_PROFILER();
        mType = type;
    }

    auto SkyboxMaterial::IsType( SkyboxType type ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mType == type;
    }

    auto SkyboxMaterial::GetType() const -> SkyboxType {
        MKT_BEGIN_PROFILER();
        return mType;
    }

    auto SkyboxMaterial::GetFace( SkyboxFace face ) const -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        const auto found{ mCubeFaces.find( face ) };
        const auto resource{ found == mCubeFaces.end() ? core::Ref<asset::Texture>{} : found->second.GetIfReady() };
        return resource ? resource->GetResource() : renderer::rhi::TextureHandle{};
    }

    auto SkyboxMaterial::GetFaceTextures() const -> const ankerl::unordered_dense::map<SkyboxFace, asset::AssetHandle<asset::Texture>>& {
        return mCubeFaces;
    }

    auto SkyboxMaterial::GetEquirectangular() const -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        const auto resource{ mEquirectangular.GetIfReady() };
        return resource ? resource->GetResource() : renderer::rhi::TextureHandle{};
    }

    auto SkyboxMaterial::GetEquirectangularAsset() const -> const asset::AssetHandle<asset::Texture>& {
        return mEquirectangular;
    }

    auto SkyboxMaterial::GetFaceAsset( SkyboxFace face ) const -> asset::AssetHandle<asset::Texture> {
        const auto found{ mCubeFaces.find( face ) };
        return found == mCubeFaces.end() ? asset::AssetHandle<asset::Texture>{} : found->second;
    }

    auto SkyboxMaterial::SetAmbientScale( core::f32 scale ) -> void {
        MKT_BEGIN_PROFILER();
        mAmbientScale = scale;
    }

    auto SkyboxMaterial::HasRequiredTextures() const -> bool {
        MKT_BEGIN_PROFILER();

        switch ( mType ) {
            case SkyboxType::eCubeFaces:
                if ( mCubeFaces.size() != 6 ) {
                    return false;
                }

                // Six entries alone are insufficient: each semantic face must
                // be present, otherwise projection can reuse stale cube layers.
                for ( const auto face: { SkyboxFace::eFront, SkyboxFace::eBack, SkyboxFace::eLeft,
                          SkyboxFace::eRight, SkyboxFace::eTop, SkyboxFace::eBottom } ) {
                    const auto found{ mCubeFaces.find( face ) };

                    if ( found == mCubeFaces.end() || !found->second.IsReady() ) {
                        return false;
                    }
                }

                return true;
            case SkyboxType::eEquirectangular:
                return mEquirectangular.IsReady();
            default:;
        }

        return false;
    }

    auto SkyboxMaterial::GetAmbientScale() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mAmbientScale;
    }


    auto SkyboxMaterial::Serialize( const filesystem::Path& filename ) const -> void {
        MKT_BEGIN_PROFILER();

        YAML::Node root{};
        root["Version"] = 2;
        root["Type"] = "SkyboxMaterial";
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

    auto SkyboxMaterial::Serialize( filesystem::FileHandle file ) const -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Material file is null." );
        }

        Serialize( file->GetPath() );
        file->UpdateContentsFromDisk();
    }

    auto SkyboxMaterial::Deserialize( const filesystem::Path& filename ) -> void {
        Deserialize( filesystem::FileHandle::New( filename ) );
    }

    auto SkyboxMaterial::Deserialize( filesystem::FileHandle file ) -> void {
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

        if ( type != "SkyboxMaterial" ) {
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
        auto value{ core::Ref<SkyboxMaterial>::New( root["Name"].as<std::string>( "Material" ).c_str() ) };
        value->RestoreProperties( root["Material"] );
        value->SetName( root["Name"].as<std::string>( "Material" ).c_str() );

        mSourcePath = file->GetPath();
        mName = eastl::move( value->mName );
        mAmbientScale = eastl::move( value->mAmbientScale );
        mType = eastl::move( value->mType );
        mEquirectangular = eastl::move( value->mEquirectangular );
        mCubeFaces = eastl::move( value->mCubeFaces );
    }

    auto SkyboxMaterial::GetName() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto SkyboxMaterial::SetName( const eastl::string_view newName ) -> void {
        MKT_BEGIN_PROFILER();
        mName = newName;
    }

    auto SkyboxMaterial::SerializeProperties() const -> YAML::Node {
        MKT_BEGIN_PROFILER();

        YAML::Node data{ YAML::NodeType::Map };
        data["SkyboxType"] = static_cast<core::i32>( GetType() );
        data["AmbientScale"] = GetAmbientScale();
        const auto append{ [&]( const asset::AssetHandle<asset::Texture>& texture, core::i32 face ) {
            if ( texture ) {
                YAML::Node image;
                image["Face"] = face;
                image["TexturePath"] = TexturePath( texture );
                data["Images"].push_back( image );
            }
        } };
        append( GetEquirectangularAsset(), -1 );

        for ( core::i32 face{}; face < 6; ++face ) {
            append( GetFaceAsset( static_cast<SkyboxFace>( face ) ), face );
        }

        return data;
    }

    auto SkyboxMaterial::RestoreProperties( const YAML::Node& node ) -> void {
        MKT_BEGIN_PROFILER();

        const auto skyType{ node["SkyboxType"].as<core::i32>( 0 ) };
        const auto ambient{ node["AmbientScale"].as<core::f32>( 1.0f ) };

        if ( skyType < 0 || skyType >= static_cast<core::i32>( SkyboxType::eCount ) || ambient < 0 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid skybox material settings." );
        }

        SetType( static_cast<SkyboxType>( skyType ) );
        SetAmbientScale( ambient );
        const auto images{ node["Images"] };

        if ( images && !images.IsSequence() ) {
            MKT_THROW_RUNTIME_ERROR( "Skybox images must be a sequence." );
        }

        bool seen[7]{};

        for ( const auto& image: images ) {
            const auto face{ image["Face"].as<core::i32>() };

            if ( face < -1 || face > 5 || seen[face + 1] ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid or duplicate skybox face." );
            }

            seen[face + 1] = true;
            const filesystem::Path path{ image["TexturePath"].as<std::string>().c_str() };

            if ( !path.Exists() ) {
                MKT_THROW_RUNTIME_ERROR( "Skybox image could not be found." );
            }

            auto texture{ asset::AssetRegistryService::Get()->LoadAssetHandle<asset::Texture>( path, renderer::rhi::TextureDimension::eTexture2D ) };

            if ( !texture.IsReady() ) {
                MKT_THROW_RUNTIME_ERROR( "Skybox image could not be loaded." );
            }

            if ( face == -1 ) {
                SetEquirectangular( texture );
            }
            else {
                SetFace( static_cast<SkyboxFace>( face ), texture );
            }
        }
    }

    auto SkyboxMaterial::GetSourcePath() const -> const filesystem::Path& {
        return mSourcePath;
    }

}// namespace mikoto
