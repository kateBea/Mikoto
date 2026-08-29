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

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Profiler.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Material/ShaderLibrary.hh>

#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::material {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    ShaderLibrary::ShaderLibrary( const ShaderLibraryDescription &options )
        : mDevice{ options.mDevice }, mRootPath{ options.mRootPath } {}

    auto ShaderLibrary::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();
        mIsInitialized = true;
    }

    auto ShaderLibrary::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        mShaders.clear();
        mIsInitialized = false;
    }

    auto ShaderLibrary::GetShader( const eastl::string_view uri ) -> ShaderModuleHandle {
        const auto fullPath{ Path{ uri } };
        if ( auto it{ mShaders.find( fullPath ) }; it != mShaders.end() ) {
            return it->second;
        }

        return ShaderModuleHandle::CreateEmpty();
    }

    auto ShaderLibrary::LoadShader( eastl::string_view uri, rhi::ShaderType type ) -> ShaderModuleHandle {
        MKT_BEGIN_PROFILER_NAMED();

        auto path{ PathBuilder{}
            .SetPath( mRootPath )
            .SetPath( uri.data() )
            .Build()};

        auto it{ mShaders.find( path ) };
        if ( it != mShaders.end() ) {
            return it->second;
        }

        // When shader is cached we look in the asset cache folders see if we find the
        // compiled byte code and feed it to the API, shader compilation takes long,
        // we already have a way to do it on the fly at runtime, but it is ideal not to do it
        // everytime we launch the app
        FileHandle shader{ FileService::Get()->LoadFile( path ) };
        auto shaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( shader )
            .SetModuleName( shader->GetName() )
            .SetModulePath( shader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( type ) };

        ShaderModuleHandle shaderModuleHandle{ mDevice->CreateShader( shaderDescription ) };
        if ( !shaderModuleHandle.IsEmpty() ) {
            mShaders.try_emplace( path, shaderModuleHandle );
            return mShaders.at( path );
        }

        return ShaderModuleHandle::CreateEmpty();
    }
}// namespace Mikoto