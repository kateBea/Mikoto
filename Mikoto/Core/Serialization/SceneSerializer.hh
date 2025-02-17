//
// Created by zanet on 1/26/2025.
//

#ifndef SCENESERIALIZER_HH
#define SCENESERIALIZER_HH

#include <Scene/Scene/Scene.hh>
#include <Core/Serialization/Serializer.hh>

namespace Mikoto {
    /**
    * Serializer object for Scenes. Can load a scene from a file
    * and serialize it too.
    * */
    class SceneSerializer final : public ISerializer<Scene> {
    public:
        auto Serialize( const Scene& scene, const Path_T& saveFilePath ) -> void override;
        auto Deserialize( const Path_T& saveFilePath ) -> Scope_T<Scene> override;
    };
}
#endif // SCENESERIALIZER_HH
