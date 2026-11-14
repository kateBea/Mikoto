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

#ifndef MIKOTO_CORE_ENGINE_INL
#define MIKOTO_CORE_ENGINE_INL

#include <stdexcept>

#include <EASTL/utility.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

namespace mikoto::core {

    template<ServiceType Type, typename... Args>
        requires std::constructible_from<Type, Args...>
    auto Engine::PushService( bool mainThread, Args&&... args ) -> Type* {
        const std::type_index type{ typeid( Type ) };

        // Check BEFORE construction: most services register their singleton in
        // the constructor, so replacing an existing owning pointer is too late.
        if ( mNodeIndices.contains( type ) ) {
            MKT_THROW_LOGIC_ERROR( "Cannot register the same service twice" );
        }

        using Dependencies = ServiceDependencies<Type>;
        using UpdateAfter = typename Dependencies::UpdateAfter;
        using Initialization = typename Dependencies::Initialization;

        ServiceNode node{
            .mType = type,
            .mUpdateAfter = UpdateAfter::template Resolve<Type, true>(),
            .mInitialization = Initialization::template Resolve<Type>(),
            .mRunsOnMainThread = mainThread,
        };

        node.mService = eastl::make_unique<Type>( eastl::forward<Args>( args )... );
        auto* instance{ static_cast<Type*>( node.mService.get() ) };
        if constexpr ( std::derived_from<Type, ISubsystem> ) {
            node.mSubsystem = instance;
        }

        mNodeIndices.emplace( type, mNodes.size() );
        mNodes.push_back( eastl::move( node ) );
        return instance;
    }
}// namespace mikoto::core

#endif// MIKOTO_CORE_ENGINE_INL
