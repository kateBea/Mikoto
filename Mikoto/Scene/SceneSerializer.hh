//
// Created by kate on 1/26/2025.
//

#ifndef SCENESERIALIZER_HH
#define SCENESERIALIZER_HH

#include <Scene/Scene.hh>
#include <Core/Serializer.hh>

namespace Mikoto {

    /**
    * Serializer object for Scenes. Can load a scene from a file
    * and serialize it too.
    * */
    class SceneSerializer final : public ISerializer<Scene> {
    public:
        auto Serialize( const Scene& scene, const Path& saveFilePath ) -> void override;
        auto Deserialize( const Path& saveFilePath ) -> Unique<Scene> override;
    };
}
#endif // SCENESERIALIZER_HH
