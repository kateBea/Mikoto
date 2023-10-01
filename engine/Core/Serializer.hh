/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

#ifndef MIKOTO_SERIALIZER_HH
#define MIKOTO_SERIALIZER_HH

// C++ Standard Library
#include <memory>

// Project Headers
#include <Utility/Types.hh>
#include <Scene/Scene.hh>

namespace Mikoto::Serializer {
    class SceneSerializer {
    public:
        explicit SceneSerializer(std::shared_ptr<Scene> scene);

        auto Serialize(const Path_T& saveFilePath) -> void;
        auto Deserialize(const Path_T& saveFilePath) -> void;

    private:
        std::shared_ptr<Scene> m_Scene{};
    };
}


#endif // MIKOTO_SERIALIZER_HH
