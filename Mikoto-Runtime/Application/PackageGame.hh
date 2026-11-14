#ifndef MIKOTO_PACKAGE_GAME_HH
#define MIKOTO_PACKAGE_GAME_HH

#include <filesystem>

#include <Project/Project.hh>

namespace mikoto::runtime {

    /**
     * @brief Validates and stages a standalone game in a new directory.
     * @details Keeps the project layout under Game/, stages engine Resources and
     * native libraries, rewrites scene asset references and writes game.yaml.
     * The destination must not exist or be inside the project/resources tree.
     * Throws on unsupported external assets, symbolic links or filesystem errors.
     */
    auto PackageGame(
        const project::Project& project,
        const std::filesystem::path& executable,
        const std::filesystem::path& resources,
        const std::filesystem::path& destination ) -> void;
}// namespace mikoto::runtime

#endif// MIKOTO_PACKAGE_GAME_HH
