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

#ifndef MIKOTO_SERVICE_HH
#define MIKOTO_SERVICE_HH

#include <Common/Common.hh>

namespace Mikoto {

    /**
    * @brief Base class for services following the singleton pattern.
    *
    */
    class IService {
    public:

        /**
         * @brief Default virtual destructor.
         *
         * Ensures proper cleanup of derived service instances.
         */
        virtual ~IService() = default;

        /**
         * @brief Initializes the service.
         *
         * This function must be implemented by derived classes to define
         * the necessary setup operations when the service starts.
         */
        virtual auto Init() -> void = 0;

        /**
         * @brief Shuts down the service.
         *
         * This function must be implemented by derived classes to handle
         * cleanup and resource deallocation when the service is stopped.
         */
        virtual auto Shutdown() -> void = 0;

        /**
         * @brief Checks if the service is initialized.
         *
         * @return True if the service is initialized, false otherwise.
         */
        MKT_NODISCARD auto IsInitialized() const -> bool {
            return m_IsInitialized;
        }

        /**
         * @brief Checks if the service is sleeping.
         *
         * @return True if the service is sleeping, false otherwise.
         */
        MKT_NODISCARD auto IsSleeping() const -> bool {
            return m_IsSleeping;
        }

        /**
         * @brief Sets the sleeping state of the service.
         *
         * @param sleep True to set the service to sleeping, false to wake it up.
         */
        auto SetSleeping( const bool sleep ) -> void {
            m_IsSleeping = sleep;
        }

    protected:
        bool m_IsSleeping{ false };

        bool m_IsInitialized{ false };
    };

}
#endif // MIKOTO_SERVICE_HH
