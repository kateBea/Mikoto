//
// Created by kate on 3/25/2025.
//

#ifndef SERVICE_HH
#define SERVICE_HH

#include <type_traits>

#include <Common/Common.hh>
#include <Common/Singleton.hh>

namespace Mikoto {

    /**
    * @brief Base class for services following the singleton pattern.
    * @tparam ServiceType The type of the derived service class.
    *
    * This class provides a common interface for services that follow a singleton pattern.
    * It inherits from `Singleton<ServiceType>`, ensuring that only one instance of the service exists.
    *
    * Services inherited from this class must implement the `Init` and `Shutdown` methods.
    */
    template<typename ServiceType>
    class IService : public Singleton<ServiceType> {
    public:
        /**
         * @brief Retrieves the singleton instance of the service.
         * @return A pointer to the instance of the service.
         *
         * This function provides access to the unique instance of the service.
         */
        MKT_NODISCARD static auto GetInstance() -> ServiceType* {
            return ServiceType::GetPtr();
        }

        /**
         * @brief Default virtual destructor.
         *
         * Ensures proper cleanup of derived service instances.
         */
        ~IService() override = default;

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

    protected:
        bool m_IsInitialized{ false };

    };

}// namespace Mikoto
#endif//SERVICE_HH
