//
// Created by kate on 1/4/25.
//

#ifndef APPLICATIONDATA_HH
#define APPLICATIONDATA_HH

#include <vector>
#include <string>

#include <STL/Utility/Types.hh>

#include <Models/Enums.hh>

namespace Mikoto {
    /**
     * @brief Holds application initialization data.
     * This struct is mostly relevant at application initialization.
     * */
    struct ApplicationData {
        Int32_T WindowWidth{};      /**< The width of the application window. */
        Int32_T WindowHeight{};     /**< The height of the application window. */
        std::string Name{};         /**< The name of the application. */
        Path_T WorkingDirectory{};  /**< The path to the working directory. */
        Path_T Executable{};        /**< The path to the executable. */
        GraphicsAPI RenderingBackend{}; /**< The selected graphics rendering backend. */
        std::vector<std::string> CommandLineArguments{}; /**< Command-line arguments. */
    };
}
#endif //APPLICATIONDATA_HH
