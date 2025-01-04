//
// Created by kate on 12/16/24.
//

#ifndef MIKOTO_SCENE_ENTITY_CREATE_INFO_HH
#define MIKOTO_SCENE_ENTITY_CREATE_INFO_HH

#include <Scene/Entity.hh>

namespace Mikoto {
    struct EntityCreateInfo {
        std::string Name{};
        Entity* Root{ nullptr };
        PrefabSceneObject PrefabType{};

        MKT_NODISCARD inline auto IsPrefab() const -> bool {
            return PrefabType != PrefabSceneObject::NO_PREFAB_OBJECT;
        }

        MKT_NODISCARD inline auto IsCustomModel() const -> bool {
            return PrefabType == PrefabSceneObject::CUSTOM_MODEL_PREFAB_OBJECT;
        }
    };
}

#endif //MIKOTO_SCENE_ENTITY_CREATE_INFO_HH
