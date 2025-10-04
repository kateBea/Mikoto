//
// Created by kate on 3/25/2025.
//

#ifndef SERVICE_HH
#define SERVICE_HH

#include <Common/Common.hh>

namespace Mikoto {

    /**
    * @brief Base class for services following the singleton pattern.
    *
    * This class provides a common interface for services that follow a singleton pattern.
    * It inherits from `Singleton<ServiceType>`, ensuring that only one instance of the service exists.
    *
    * Services inherited from this class must implement the `Init` and `Shutdown` methods.
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
         * @brief Updates the service.
         *
         * This function can be overridden by derived classes to implement
         */
        virtual auto Update(float dt) -> void { }
        /**
         * @brief Checks if the service is initialized.
         *
         * @return True if the service is initialized, false otherwise.
         */
        MKT_NODISCARD auto IsInitialized() const -> bool {
            return m_IsInitialized;
        }

    protected:
        bool m_IsInitialized{ false };
    };

}// namespace Mikoto
#endif//SERVICE_HH
