// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.
#pragma once

namespace mikoto::material { class SurfaceMaterial; class VolumeMaterial; }

namespace mikoto::editor {
    /** Shared optical controls for the material editor and entity inspector. */
    auto DrawMaterialOptics( material::SurfaceMaterial& material ) -> bool;
    auto DrawVolumeMaterial( material::VolumeMaterial& material ) -> bool;
}
