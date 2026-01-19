//
// Created by zanet on 3/21/2025.
//

#ifndef IDEVICEOBJECT_HH
#define IDEVICEOBJECT_HH

#include <Library/Data/ResourcePool.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {
    class GpuDevice;

    struct Object {
        enum class Type {
            Pointer,
            Integer,
            None
        };

        Type type{ Type::None };
        void* pointer{ nullptr };
        UInt64 integer{ 0 };

        explicit Object(void* p) : type(Type::Pointer), pointer(p) {}
        explicit Object(UInt64 i) : type(Type::Integer), integer(i) {}
        Object() = default;

        template<typename T>
        operator T*() const {
            if (type == Type::Pointer) {
                return static_cast<T*>(pointer);
            }

            return nullptr;
        }
    };


    enum class ObjectType {
        Vk_Device,
        Vk_Buffer,
        Vk_Sampler,
        Vk_Format,
        Vk_Image,
        Vk_ImageView,
        Vk_Framebuffer,
        Vk_CmdPool,
        Vk_CmdBuffer,
        Vk_DescriptorSetLayout,
        Vk_Pipeline,
        Vk_PipelineLayout,
    };



    /**
     * @class DeviceObject
     * @brief Represents a device object associated with a GPU device.
     *
     * This class serves as a base class for GPU resources that are tied to a specific GPU device,
     * such as buffers, textures, and shaders. It manages the association with the device and handles
     * resource allocation and release. The actual allocation and release are implemented by derived classes.
     */
    class DeviceObject : public IResource {
    public:
        /**
         * @brief Default constructor for the DeviceObject class.
         *
         * Initializes a default instance of the DeviceObject. This constructor is typically used
         * when the device object is initialized later with a call to `Init`.
         */
        explicit DeviceObject() = default;

        /**
         * @brief Initializes the device object with the given GPU device.
         *
         * This method sets the device for the object and calls `Allocate` to perform any necessary
         * resource allocation specific to the derived object.
         *
         * @param device A pointer to the GPU device associated with this object.
         */
        auto Initialize( GpuDevice* device ) -> void {
            m_Device = device;

            Initialize();
        }

        /**
         * @brief Sets the debug name for the device object.
         *
         * This name can be used for debugging purposes to identify the object.
         *
         * @param name The debug name to assign to the device object.
         */
        virtual auto SetDebugName(const std::string_view name) -> void { m_DebugName = name; }
        
        /**
         * @brief Retrieves the debug name of the device object.
         *
         * @returns The debug name of the device object.
         */
        MKT_NODISCARD auto GetDebugName() const -> const std::string& { return m_DebugName; }

        /**
         * @brief Retrieves the default debug name
         *
         * @returns The default debug name
         */
        MKT_NODISCARD static auto GetDefaultDebugName() -> std::string_view { return "DeviceObject"; }

        /**
         * @brief Destructor for the DeviceObject class.
         *
         * Ensures proper cleanup of resources when the device object is destroyed.
         */
        ~DeviceObject() override = default;

        /**
         * @brief Returns the usage type of this device object.
         *
         * @returns The usage type of this device object.
         */
        MKT_NODISCARD auto GetResourceUsage() const -> ResourceUsageType {
            return m_UsageType;
        }

        /**
         * @brief Retrieves the native handle of the device object.
         *
         * This method should be overridden by derived classes to return the appropriate
         * native handle (e.g., Vulkan handle) for the specific object type.
         *
         * @param type The type of native object handle to retrieve.
         * @returns The native handle wrapped in an Object structure.
         */
        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType ) -> Object { return Object(nullptr); }
        MKT_NODISCARD virtual auto GetNativeHandle( ObjectType type ) const -> Object { return const_cast<DeviceObject*>(this)->GetNativeHandle( type ); }

    protected:

        /***
         * @brief Constructor for the DeviceObject class.
         *
         * Initializes the device object with the given GPU device and resource usage type.
         *
         * @param usageType The resource usage type (e.g., static, dynamic).
         */
        explicit DeviceObject( const ResourceUsageType usageType )
            : m_UsageType{ usageType } {}

        /**
         * @brief Allocates the resources specific to the device object.
         *
         * This method is pure virtual and must be implemented by derived classes to allocate resources
         * (e.g., buffers, textures) on the GPU. It is called when the `Init` method is invoked.
         */
        auto Initialize() -> void override = 0;

        /**
        * @brief Releases the underlying GPU resource.
        *
        * This pure virtual function must be implemented by concrete device resource classes
        * (e.g., buffers, textures, samplers) to handle cleanup of GPU-side memory or API-specific
        * handles. It ensures the resource is properly released from the graphics device.
        *
        * Once called, the object should no longer be used until reinitialized or recreated.
        */
        auto Release() -> void override = 0;

    protected:
        std::string m_DebugName{ "DeviceObject" };
        GpuDevice* m_Device{};
        ResourceUsageType m_UsageType{ ResourceUsageType::RESOURCE_USAGE_STATIC };
    };
}

#endif//IDEVICEOBJECT_HH
