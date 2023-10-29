#!/bin/bash

submodule_links=(
    "https://github.com/glfw/glfw.git"
    "https://github.com/ocornut/imgui.git"
    "https://github.com/g-truc/glm.git"
    "https://github.com/nothings/stb.git"
    "https://github.com/fmtlib/fmt.git"
    "https://github.com/gabime/spdlog.git"
    "https://github.com/skypjack/entt.git"
    "https://github.com/zeux/volk.git"
    "https://github.com/assimp/assimp.git"
    "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git"
    "https://github.com/CedricGuillemet/ImGuizmo.git"
    "https://github.com/jbeder/yaml-cpp.git"
    "https://github.com/btzy/nativefiledialog-extended.git"
    "https://github.com/jrouwe/JoltPhysics.git"
)

for submodule_url in "${submodule_links[@]}"
do
    submodule_path=$(basename "$submodule_url" .git)
    echo "Adding submodule from $submodule_url"
    git submodule add "$submodule_url" "$submodule_path"
done
