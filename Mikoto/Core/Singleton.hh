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

#ifndef MIKOTO_CORE_SINGLETON_HH
#define MIKOTO_CORE_SINGLETON_HH

#include <Core/Core.hh>

#include <Logging/Assert.hh>

namespace mikoto::core {

    template<typename Derived>
    class Singleton : public INonCopyable, public INonMoveable {
    public:
        using Value = Derived;
        using ValueRef = Derived&;
        using ValuePtr = Value*;

    public:
        explicit Singleton() {
            MKT_BEGIN_PROFILER();
            MKT_ASSERT( !sInstance, "Singleton - Instance already exists." );
            sInstance = static_cast<ValuePtr>( this );
        }


        /**
         * Returns a reference to the single instance
         * @returns Reference to the single instance
         * */
        MKT_NODISCARD static auto Get() -> ValueRef {
            return *sInstance;
        }


        /**
         * Returns a pointer to the single instance
         * @returns Pointer to the single instance
         * */
        MKT_NODISCARD static auto GetPtr() -> ValuePtr {
            return sInstance;
        }

        /**
         * Returns true if the singleton instance exists
         * @returns True if the singleton instance exists
         * */
        MKT_NODISCARD static auto HasInstance() -> bool {
            return sInstance != nullptr;
        }


        ValuePtr operator->() {
            return sInstance;
        }


        /**
         * Called when this instance is destroyed.
         * */
        virtual ~Singleton() {
            // A singleton can be owned by a short-lived Engine instance.  Do not
            // leave a pointer to a destroyed service behind when that instance
            // is torn down (or when a subsequent engine is created in-process).
            if ( sInstance == static_cast<ValuePtr>( this ) ) {
                sInstance = nullptr;
            }
        }

    protected:
        /**
         * Pointer to the single instance allowed for Singleton objects
         * */
        inline static ValuePtr sInstance;
    };
}

#endif// MIKOTO_CORE_SINGLETON_HH
