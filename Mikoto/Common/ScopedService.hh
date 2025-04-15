//
// Created by zanet on 4/6/2025.
//

#ifndef SCOPEDSERVICE_HH
#define SCOPEDSERVICE_HH

namespace Mikoto {
    /**
    * @brief Base class for services that require scoped instantiation.
    * @tparam ServiceType The type of the derived service class.
    *
    * Unlike singleton services, scoped services can be created multiple times.
    * This class provides a consistent interface for initializing and shutting down
    * such services. Scoped services are useful in contexts like scene-local renderers,
    * where each scene may have its own isolated instance.
    */
    template<typename ServiceType>
    class IScopedService {
    public:
        /**
        * @brief Default virtual destructor.
        *
        * Ensures proper cleanup of derived service instances.
        */
        virtual ~IScopedService() = default;

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

}
#endif//SCOPEDSERVICE_HH
