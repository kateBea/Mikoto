/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

#ifndef MIKOTO_FILE_MANAGER_HH
#define MIKOTO_FILE_MANAGER_HH

// C++ Standard Library
#include <memory>
#include <unordered_map>
#include <initializer_list>

// Project Headers
#include <STL/Utility/Types.hh>
#include <Scene/Scene.hh>

namespace Mikoto::FileManager {
    class Assets {
    public:
        /**
         * @brief
         *
         * @param path
         * */
        static auto SetRootPath(const Path_T& path) -> void {
            s_AssetRootPath = path;
        }


        /**
         * @brief
         * @returns
         * */
        MKT_NODISCARD static auto GetRootPath() -> const Path_T& { return s_AssetRootPath; }
    private:

        inline static Path_T s_AssetRootPath{};
    };


    /**
     * Initializes the serializer utilities and some libraries it requires
     * like NFD. The later is very important to be initialized after any
     * platform windowing abstraction framework such as SDL or GLFW.
     * */
    auto Init() -> void;


    /**
     * Releases resources from the Serializer namespace and shuts down
     * associated libraries.
     * */
    auto Shutdown() -> void;


    /**
     * Opens a save file dialog with the given filters. Every filter has a name
     * followed by the extension for that filter (coma separated). An
     * example of filter would be: { "Source File", "cc,c,cpp,cxx" }.
     * It offers the possibility to specify a default name when saving the file
     * @param filename default save file name
     * @param filters dialog filters
     * */
    auto SaveDialog(const std::string& filename, const std::initializer_list<std::pair<std::string, std::string>>& filters) -> std::string;


    /**
     * Opens a file dialog with the given filters. Every filter has a name
     * followed by the extension for that filter (coma separated). An
     * example of filter would be: { "Source File", "cc,c,cpp,cxx" }
     * @param filters dialog filters
     * */
    auto OpenDialog(const std::initializer_list<std::pair<std::string, std::string>>& filters) -> std::string;


    /**
     * Serializer object for Scenes. Can load a scene from a file
     * and serialize it too.
     * */
    class SceneSerializer {
    public:
        static auto Serialize( Scene& scene, const Path_T& saveFilePath ) -> void;
        static auto Deserialize( const Path_T& saveFilePath ) -> void;
    };
}


#endif// MIKOTO_FILE_MANAGER_HH
