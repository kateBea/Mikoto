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

#ifndef MIKOTO_PHYSICS_CONTACT_EVENTS_HH
#define MIKOTO_PHYSICS_CONTACT_EVENTS_HH

#include <map>
#include <set>
#include <tuple>

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::physics {
    struct ContactEvent {
        enum class Type : core::u32 {
            eEnter,
            eStay,
            eExit
        };

        Type mType{ Type::eEnter };
        core::u64 mEntityA{};
        core::u64 mEntityB{};
        bool mIsTrigger{};

        MKT_NODISCARD auto GetCallbackIndex() const -> core::u32 {
            return static_cast<core::u32>( mType ) + ( mIsTrigger ? 3u : 0u );
        }

        MKT_NODISCARD auto GetCallbackName() const -> const char * {
            constexpr const char *names[]{ "OnCollisionEnter", "OnCollisionStay", "OnCollisionExit",
                "OnTriggerEnter", "OnTriggerStay", "OnTriggerExit" };
            return names[GetCallbackIndex()];
        }
    };

    /** Owner synchronizes access. Shape contacts are counted separately from body-pair events. */
    class ContactEventBuffer final {
    public:
        auto AddContact( core::u64 a, core::u64 b, bool trigger ) -> void {
            const auto key{ MakeKey( a, b, trigger ) };
            if ( ++mContacts[key] == 1 ) {
                mEntered.insert( key );
                Push( key, ContactEvent::Type::eEnter );
            }
        }

        auto RemoveContact( core::u64 a, core::u64 b, bool trigger ) -> void {
            const auto key{ MakeKey( a, b, trigger ) };
            const auto found{ mContacts.find( key ) };
            if ( found != mContacts.end() && --found->second == 0 ) {
                Push( key, ContactEvent::Type::eExit );
                mContacts.erase( found );
            }
        }

        auto FinishStep() -> void {
            // Jolt may visit several substeps/manifolds, or skip sleeping bodies.
            // Stay describes the continuing body pair once per simulation update.
            for ( const auto &[key, count]: mContacts ) {
                if ( !mEntered.contains( key ) ) {
                    Push( key, ContactEvent::Type::eStay );
                }
            }
            mEntered.clear();
        }

        auto RestartNotifications() -> void {
            mEvents.clear();
            mEntered.clear();
            for ( const auto &[key, count]: mContacts ) {
                mEntered.insert( key );
                Push( key, ContactEvent::Type::eEnter );
            }
        }

        auto Clear() -> void {
            mContacts.clear();
            mEntered.clear();
            mEvents.clear();
            mDropped = 0;
        }

        /** Returns the queued notifications and clears the queue. */
        MKT_NODISCARD auto GetEvents() -> eastl::vector<ContactEvent> {
            eastl::vector<ContactEvent> result{};
            result.swap( mEvents );
            return result;
        }

        MKT_NODISCARD auto GetDroppedEventCount() const -> core::usize {
            return mDropped;
        }

    private:
        using Key = std::tuple<core::u64, core::u64, bool>;

        MKT_NODISCARD static auto MakeKey( core::u64 a, core::u64 b, bool trigger ) -> Key {
            return { eastl::min( a, b ), eastl::max( a, b ), trigger };
        }

        auto Push( const Key &key, ContactEvent::Type type ) -> void {
            if ( mEvents.size() < 16384 ) {
                mEvents.push_back( { type, std::get<0>( key ), std::get<1>( key ), std::get<2>( key ) } );
            }
            else {
                ++mDropped;
            }
        }

        std::map<Key, core::usize> mContacts{};
        std::set<Key> mEntered{};
        eastl::vector<ContactEvent> mEvents{};
        core::usize mDropped{};
    };
}// namespace mikoto::physics

#endif
