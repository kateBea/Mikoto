/**
 * Random.hh
 * Created by kate on 12/16/24.
 * */

#ifndef MIKOTO_SCENE_FILE_UTILITIES_HH
#define MIKOTO_SCENE_FILE_UTILITIES_HH

#include <Library/Utility/Types.hh>
#include <Common/Common.hh>

namespace Mikoto {
    /**
     * @class FileUtilities
     * A utility class containing static methods for common file operations.
     * This class provides a method to join multiple paths into a single path.
     * */
    class FileUtilities final {
    public:
        /**
         * @brief Joins multiple paths into a single path.
         * @tparam T A type convertible to std::string_view for the first path.
         * @tparam Ts Variadic template types for additional paths.
         * @param path The first path.
         * @param paths Additional paths to join.
         * @return A combined path as a Path_T object.
         * */
        template <std::convertible_to<std::string_view> T, typename... Ts>
        static auto JoinPaths(T&& path, Ts&&... paths) -> Path_T {
            Path_T result{ path };
            std::string expansion{};

            if constexpr (sizeof...(paths)) {
                expansion = std::move(JoinPaths(paths...));
            }

            result.append(expansion);
            return result;
        }

        DISABLE_COPY_AND_MOVE_FOR(FileUtilities);
    };
}
#endif //MIKOTO_SCENE_FILE_UTILITIES_HH
