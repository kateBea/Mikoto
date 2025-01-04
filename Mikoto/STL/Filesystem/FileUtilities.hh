/**
 * Random.hh
 * Created by kate on 12/16/24.
 * */

#ifndef MIKOTO_SCENE_FILE_UTILITIES_HH
#define MIKOTO_SCENE_FILE_UTILITIES_HH

#include <STL/Utility/Types.hh>
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

        /**
         * Returns a string containing the data from a file
         * @param path path to the file
         * @returns contents of the file
         * */
        static auto GetFileData(const Path_T& path) -> std::string {
            std::ifstream file{ path, std::ios::binary };

            if (!file.is_open()) {
                MKT_THROW_RUNTIME_ERROR(fmt::format("Failed to open file [ {} ]!", path.string()));
            }

            return std::string{ std::istreambuf_iterator<std::vector<char>::value_type>(file),
                    std::istreambuf_iterator<std::vector<char>::value_type>() };
        }

        /**
         * @brief Determines the size of a file.
         * @param path Absolute or relative path to the file.
         * @returns Size in KB of the given file, -1 if the file is not valid (not a directory or does not exist).
         * */
        MKT_NODISCARD static auto GetFileSize(const Path_T& path) -> Int64_T {
            std::ifstream file{ path };

            if (!file.is_open()) {
                return -1;
            }

            return static_cast<Int64_T>(std::distance(
                std::istreambuf_iterator<char>( file ),
                std::istreambuf_iterator<char>() ) / 1'000);
        }

        DISABLE_COPY_AND_MOVE_FOR(FileUtilities);
    };
}
#endif //MIKOTO_SCENE_FILE_UTILITIES_HH
