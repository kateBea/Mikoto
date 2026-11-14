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

#include <Threading/ProfilerObserver.hh>

#if defined( MIKOTO_ENABLE_TRACY )
#include <cstdio>

namespace mikoto::threading {
    auto ProfilerObserver::set_up( core::usize workers ) -> void {
        mWorkers.reserve( workers );

        for ( core::usize i{}; i < workers; ++i ) {
            mWorkers.emplace_back( eastl::make_unique<WorkerScopes>() );
        }
    }

    auto ProfilerObserver::on_entry( tf::WorkerView worker, tf::TaskView task ) -> void {
        auto& scopes{ *mWorkers[worker.id()] };

        if ( !scopes.mNamed ) {
            char name[64]{};
            std::snprintf( name, sizeof( name ), "Mikoto worker %u", static_cast<core::u32>( worker.id() ) );
            MKT_PROFILE_THREAD( name );
            scopes.mNamed = true;
        }

        static constexpr tracy::SourceLocationData source{
            "Taskflow task", "ProfilerObserver::on_entry", __FILE__, __LINE__, 0
        };
        // One stack per worker also handles nested tasks executed by corun.
        // ScopedZone remembers the connection, so a disconnect/reconnect between
        // entry and exit cannot emit an unmatched end into the next capture.
        if ( scopes.mDepth == scopes.mZones.size() ) {
            // Allocate only when a worker reaches a new nesting depth. Normal
            // tasks reuse the same slot without copying or moving ScopedZone.
            scopes.mZones.emplace_back( eastl::make_unique<ZoneSlot>() );
        }

        auto& zone{ scopes.mZones[scopes.mDepth++]->mZone.emplace( &source, 0 ) };
        const auto& name{ task.name() };

        if ( !name.empty() ) {
            const core::usize length{ name.size() < 65535 ? name.size() : 65534 };
            zone.Name( name.data(), length );
        }
    }

    auto ProfilerObserver::on_exit( tf::WorkerView worker, tf::TaskView ) -> void {
        auto& scopes{ *mWorkers[worker.id()] };
        scopes.mZones[--scopes.mDepth]->mZone.reset();
    }
}// namespace mikoto::threading
#endif
