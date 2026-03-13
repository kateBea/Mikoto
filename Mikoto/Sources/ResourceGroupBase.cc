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

#include <string_view>

#include <Renderer/Core/ResourceGroupBase.hh>

namespace Mikoto {

    auto CommonResourceGroup::SetBuffer( std::string_view name, UInt32 binding ) -> void {
        if (m_Resources.contains( std::string{ name } )) {
            return;
        }

        Entry newEntry{
            .Name{ name },
            .Binding{ binding },
            .Type{ ShaderResourceType::BUFFER }
        };

        m_Resources.emplace( std::string{ name }, newEntry);
    }

    auto CommonResourceGroup::SetTexture( std::string_view textureName, std::string_view samplerName, UInt32 binding ) -> void {
        const auto it{ m_Resources.find( std::string{ textureName } ) };
        const auto itS{ m_Resources.find( std::string{ samplerName } ) };

        // Check if combined image sampler already exists
        if ( it != m_Resources.end() && itS != m_Resources.end() ) {
            return;
        }

        Entry newEntry{
            .Name{ textureName },
            .Binding{ binding },
        };

        m_Resources.emplace( std::string{ textureName }, Entry{.Name{ textureName }, .SamplerName{ samplerName }, .Binding{ binding }, .Type{ ShaderResourceType::COMBINED_IMAGE_SAMPLER } } );
        m_Resources.emplace( std::string{ samplerName }, Entry{.Name{ samplerName }, .Binding{ binding }, .Type{ ShaderResourceType::SAMPLER } } );
    }

    auto ConstantsGroup::SetData( const void *ptr, Size size ) -> void {
        m_Data = ptr;
        m_SizeBytes = size;
    }

    auto ConstantsGroup::Clear() -> void {
        m_Data = nullptr;
        m_SizeBytes = 0;
    }
}// namespace Mikoto