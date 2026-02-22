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

#include <string>
#include <array>
#include <string_view>

#include <slang/slang.h>
#include <slang/slang-com-ptr.h>

#include <Core/Profiler.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderService.hh>

#include <Material/ShaderLibrary.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

namespace Mikoto {

    ShaderLibrary::ShaderLibrary( const ShaderLibraryDescription &options )
        : m_Device{ options.Device }, m_RootPath{ options.RootPath } {}

    auto ShaderLibrary::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();
        slang::createGlobalSession( m_SlangGlobalSession.writeRef() );

        m_IsInitialized = true;
    }

    auto ShaderLibrary::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_IsInitialized ) {
            return;
        }

        m_Shaders.clear();

        m_IsInitialized = false;
    }

    auto ShaderLibrary::GetShader( const std::string_view uri ) -> ShaderModuleHandle {
        const std::string fullPath{ Filesystem::GetGetAbsolutePathString( uri ) };
        if ( auto it{ m_Shaders.find( fullPath ) }; it != m_Shaders.end() ) {
            return it->second;
        }

        return ShaderModuleHandle::CreateEmpty();
    }

    auto ShaderLibrary::GetSlangGlobalSession() const -> Slang::ComPtr<slang::IGlobalSession> {
        return m_SlangGlobalSession;
    }

    auto ShaderLibrary::LoadShader( const Path &path, ShaderStage stage ) -> ShaderModuleHandle {
        MKT_BEGIN_PROFILER_NAMED();

        const std::string fullPath{ Filesystem::GetGetAbsolutePathString( path ) };
        auto it{ m_Shaders.find( fullPath ) };
        if ( it != m_Shaders.end() ) {
            return it->second;
        }

        ShaderModuleHandle shaderModuleHandle{ m_Device->LoadShader( path, stage ) };
        if ( !shaderModuleHandle.IsEmpty() ) {
            m_Shaders.try_emplace( fullPath, shaderModuleHandle );
            return m_Shaders.at( fullPath );
        }

        return ShaderModuleHandle::CreateEmpty();
    }

    auto ShaderLibrary::LoadShader( const ShaderModuleDescription &description ) -> ShaderModuleHandle {
        return this->LoadShader( description.ShaderFile->GetPath(), description.Stage );
    }
}// namespace Mikoto