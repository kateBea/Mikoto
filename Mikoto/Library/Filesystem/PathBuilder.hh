/**
* PathBuilder.hh
 * Created by kate on 12/16/24.
 * */

#ifndef MIKOTO_STRING_UTILS_PATH_BUILDER_HH
#define MIKOTO_STRING_UTILS_PATH_BUILDER_HH

#include <Library/Utility/Types.hh>

namespace Mikoto {
    /**
     * @class PathBuilder
     * A utility class to build and finalize paths in a controlled manner.
     * The class provides a fluent API for appending parts to a path, ensuring that
     * once the path is "built" using the `Build` method, no further modifications are allowed.
     * */
    class PathBuilder final {
    public:
        /**
         * @brief Appends a new part to the path if the path has not been built yet.
         * @tparam T A type convertible to std::string_view (e.g., std::string, const char*).
         * @param path The part of the path to append.
         * @return A reference to the current PathBuilder instance to allow chaining.
         * */
        template<std::convertible_to<std::string_view> StringLikeT>
        auto WithPath( StringLikeT&& path ) -> PathBuilder& {
            if ( !build ) {
                m_Path.append( path );
            }

            return *this;// Enable method chaining.
        }

        /**
         * @brief Finalizes the path and prevents further modifications.
         * @return The finalized path as a Path_T object.
         * */
        auto Build() -> Path_T {
            build = true;
            return m_Path;
        }

    private:
        /** A flag indicating whether the path has been built (finalized). */
        bool build{};

        /** The path being built. */
        Path_T m_Path{};
    };
}
#endif //MIKOTO_STRING_UTILS_PATH_BUILDER_HH
