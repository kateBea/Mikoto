//
// Created by kate on 11/3/23.
//

#ifndef MIKOTO_FILE_MANAGER_HH
#define MIKOTO_FILE_MANAGER_HH

// Project Headers
#include <Common/Types.hh>
#include <Common/Common.hh>

namespace Mikoto {
    class FileManager {
    public:
        /**
         * @brief
         *
         * @param path
         * */
        static auto SetAssetsRootPath(const Path_T& path) -> void {
            s_AssetRootPath = path;
        }


        /**
         * @brief
         * @returns
         * */
        MKT_NODISCARD static auto GetAssetsRootPath() -> const Path_T& { return s_AssetRootPath; }

    private:
        inline static Path_T s_AssetRootPath{};
    };
}

#endif // MIKOTO_FILE_MANAGER_HH
