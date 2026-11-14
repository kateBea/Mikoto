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

#ifndef MIKOTO_CORE_SERVICE_HH
#define MIKOTO_CORE_SERVICE_HH

#include <Core/Core.hh>
#include <Core/Profiler.hh>

namespace mikoto::core {

    /**
    * @brief Base class for services following the singleton pattern.
    */
    class IService : public INonCopyable, public INonMoveable {
    public:

        /**
         * @brief Default virtual destructor.
         * Ensures proper cleanup of derived service instances.
         */
        virtual ~IService() = default;

        /**
         * @brief Initializes the service.
         *
         * This function must be implemented by derived classes to define
         * the necessary setup operations when the service starts.
         * Set mIsInitialized only after setup succeeds. If setup throws before
         * that point, the implementation must release its partial resources.
         * Engine rolls back services which reached the initialized state.
         */
        virtual auto Initialize() -> void = 0;

        /**
         * @brief Shuts down the service.
         *
         * This function must be implemented by derived classes to handle
         * cleanup and resource deallocation when the service is stopped.
         * Initialization dependencies are still alive during this call. Finish
         * owned asynchronous work before releasing resources it can access.
         */
        virtual auto Shutdown() -> void = 0;

        /**
         * @brief Checks if the service is initialized.
         * @return True if the service is initialized, false otherwise.
         */
        MKT_NODISCARD auto IsInitialized() const -> bool;

        /**
         * @brief Checks if the service is sleeping.
         * @return True if the service is sleeping, false otherwise.
         */
        MKT_NODISCARD auto Sleeping() const -> bool;

        /**
         * @brief Sets the sleeping state of the service.
         */
        auto Sleep() -> void;

        /**
         * @brief Sets the sleeping state of the service.
         */
        auto Wake() -> void;

    protected:
        bool mIsSleeping{ false };
        bool mIsInitialized{ false };
    };

    class ISubsystem : public IService {
    public:

        virtual auto Update(float ts) -> void = 0;
    };
}

#endif// MIKOTO_CORE_SERVICE_HH
