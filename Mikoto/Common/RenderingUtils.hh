/**
 * RenderingUtilities.hh
 * Created by kate on 7/21/2023.
 * */

#ifndef MIKOTO_RENDERING_UTILITIES_HH
#define MIKOTO_RENDERING_UTILITIES_HH

// C++ Standard Library
#include <memory>

// Projects Headers
#include <Models/Enums.hh>

namespace Mikoto {

    /**
     * Returns the string representing the name of a given prefab type
     * @returns string representation of a prefab type
     * */
    constexpr auto PrefabTypeStr( const PrefabSceneObject type ) -> std::string_view {
        switch ( type ) {
            case PrefabSceneObject::SPRITE_PREFAB_OBJECT:
                return "SPRITE_PREFAB_OBJECT";
            case PrefabSceneObject::CUBE_PREFAB_OBJECT:
                return "CUBE_PREFAB_OBJECT";
            case PrefabSceneObject::CONE_PREFAB_OBJECT:
                return "CONE_PREFAB_OBJECT";
            case PrefabSceneObject::CYLINDER_PREFAB_OBJECT:
                return "CYLINDER_PREFAB_OBJECT";
            case PrefabSceneObject::SPHERE_PREFAB_OBJECT:
                return "SPHERE_PREFAB_OBJECT";
            case PrefabSceneObject::SPONZA_PREFAB_OBJECT:
                return "SPONZA_PREFAB_OBJECT";
            case PrefabSceneObject::COUNT_PREFAB_OBJECT:
                return "COUNT_PREFAB_OBJECT";
            default:
                return "UNKNOWN";
        }

        return "UNKNOWN";
    }


    /**
     * Returns the PrefabSceneObject representing the given prefab name. This function is mainly
     * useful for scene objects serialization in conjunction with PrefabTypeStr(PrefabSceneObject)
     * @returns PrefabSceneObject representation of a prefab name
     * */
    constexpr auto PrefabTypeFromName( const std::string_view name ) -> PrefabSceneObject {
        if ( name == PrefabTypeStr( PrefabSceneObject::SPRITE_PREFAB_OBJECT ) ) return PrefabSceneObject::SPRITE_PREFAB_OBJECT;
        if ( name == PrefabTypeStr( PrefabSceneObject::CUBE_PREFAB_OBJECT ) ) return PrefabSceneObject::CUBE_PREFAB_OBJECT;
        if ( name == PrefabTypeStr( PrefabSceneObject::CONE_PREFAB_OBJECT ) ) return PrefabSceneObject::CONE_PREFAB_OBJECT;
        if ( name == PrefabTypeStr( PrefabSceneObject::CYLINDER_PREFAB_OBJECT ) ) return PrefabSceneObject::CYLINDER_PREFAB_OBJECT;
        if ( name == PrefabTypeStr( PrefabSceneObject::SPHERE_PREFAB_OBJECT ) ) return PrefabSceneObject::SPHERE_PREFAB_OBJECT;
        if ( name == PrefabTypeStr( PrefabSceneObject::SPONZA_PREFAB_OBJECT ) ) return PrefabSceneObject::SPONZA_PREFAB_OBJECT;
        if ( name == PrefabTypeStr( PrefabSceneObject::COUNT_PREFAB_OBJECT ) ) return PrefabSceneObject::COUNT_PREFAB_OBJECT;

        return PrefabSceneObject::NO_PREFAB_OBJECT;
    }
}

#endif// MIKOTO_RENDERING_UTILITIES_HH
