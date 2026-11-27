VOLUMETRIC CLOUDS — 1920 x 1080, raster

Open Clouds.mktproj from the World selector. Use the editor fly camera; its
120 m/s movement speed lets you travel below, through and above both cloud fields.
Play or Simulate advances wind. Cloud entities define placement, dimensions and
wind; their Volume materials define density, scattering, absorption and erosion.
Renderer > Volumetric clouds exposes resolution, view/light steps, temporal
weight, multiple scattering, detail erosion and density/distance/step debug views.

This is an original Nubis³-inspired implementation: locally authored voxel
distance/density/detail fields, procedural 3D detail, amortized voxel lighting,
distance-guided ray marching, wind-aware temporal reconstruction and HDR
composition. It is not Guerrilla's fluid-authoring pipeline or production solver.
At most eight active cloud volumes are supported. Surface shadows use the sun's
cloud light cache. Existing path-traced cloud transport remains available.

Reference and presentation:
https://www.guerrilla-games.com/read/nubis-cubed
https://advances.realtimerendering.com/s2023/Nubis%20Cubed%20(Advances%202023).pdf

PROCEDURAL SHAPE
The procedural profile uses a weather coverage field, a soft cloud base,
varying cloud-top height and multi-frequency cellular erosion. Density varies
through the body rather than filling a handful of ellipsoids. The local cache
uses metric noise scales and rebuilds after scale edits. View/light density
sampling and conservative distance bounds stay shared.

NUBIS-INSPIRED AUTHORING AND RENDERING UPGRADES

Material -> Cloud shape bake runs a small CPU fluid authoring job. It advects
velocity and density, applies buoyancy and vorticity confinement, projects
pressure, and retains a scalar density snapshot. It runs outside the editor
thread and never touches scene/GPU/material state from its worker. Return to
the material to apply the completed bake, then save it. The sample includes
a bundled 32-cubed updraft, with no runtime fluid simulation or external tools.

CloudsModule bakes occupancy distances after each model change. Three separable
passes compute an exact L1 transform. Dividing by sqrt(3), subtracting voxel
support and rounding down produces a conservative Euclidean skip bound. Four
8-bit distances share each buffer word: one byte per voxel. Empty space can
be tested before reading the larger model field. The scratch buffers are shared
across cloud bakes, and unchanged/wind-advected clouds keep their distance data.
This is a positive skip bound, NOT Guerrilla's signed BC1 block encoder.

Four filtered 3D noise levels provide distance/footprint-based upsampling.
Wispy and billowy low/high pairs blend differently through the density profile;
near-camera folded detail adds smaller structure. This is Mikoto's own warped
value/cellular noise, not a copy of Houdini Alligator or Curly Alligator noise.
The levels use separate 3D images through the existing RHI instead of adding
a backend-dependent mip-UAV interface. Animated march jitter is limited to a
configurable near range (250 m by default), as distant moving noise shimmers.

Two local sun samples are followed by the remaining cached light path. The
profile-based lighting adds dark edges, a forward-facing interior glow and
depth-dependent ambient illumination. These are controllable cloud-lighting
approximations; they are not an exact multiple-scattering transport solver.
All density/light/view passes use the same filtered density reconstruction.

Renderer -> Volumetric clouds exposes the distance toggle, edge darkening,
inner glow, detail mip bias and animated jitter range. These settings persist
with the project. Wispy/billowy detail belongs to each volume material.

No RHI/backend API changes are required. Packed scalar buffers avoid relying
on compressed 3D texture support. Compute, shader addresses and 3D images are
the existing Vulkan/D3D12 path; hardware ray tracing is not required.

REFERENCES
Andrew Schneider, Nubis3 (Guerrilla, Advances in Real-Time Rendering 2023):
https://www.guerrilla-games.com/read/nubis-cubed
https://d3d3g8mu99pzk9.cloudfront.net/AndrewSchneider/Nubis%20Cubed.pdf
Jos Stam, Real-Time Fluid Dynamics for Games (GDC 2003):
https://www.dgp.toronto.edu/public_user/stam/reality/Research/pdf/GDC03.pdf

VALIDATION
Standalone Vulkan tests compare every noise-mip voxel with a CPU filter,
compare distance packing against an exact CPU bound, verify sub-voxel safety,
check multiple cloud ranges, and test empty/solid fields and accelerated rays.
They also cover RGB Beer-Lambert extinction, foreground clipping, history
initialization, HDR composition, reversed Z, orthographic rays and a matching
cloud/camera translated from 0 to 16 km. Shader compilation is checked for
Vulkan and D3D12. The fluid bake is tested for determinism, buoyancy, bounds,
empty borders and parameter rejection. Full build/editor testing is left to
the user. D3D12 runtime is not tested by the Vulkan probe.

REGIONAL WEATHER AND CLOUD FORMS
Cloud materials now select Stratus, Stratocumulus, Cumulus or Cirrus, with
regional coverage/type/top-height variation and wind shear. Shape noise,
detail erosion and weather are independent cached resources. The Cloud form
selector also applies to offline bakes. The added Baked stratocumulus bank
is a 32-cubed fluid-authored snapshot (seed 94, 24 steps), with fine detail
added by the renderer; it has no runtime fluid-simulation dependency.
The original updraft remains for comparison.

CLOUD MESH SHADOWS, LOCAL LIGHTS AND LIGHTNING

The raster cloud renderer uses dedicated depth maps fitted to each cloud's
receiving box. The sunward depth range extends by Clouds Max distance; meshes
outside the camera frustum can still cast into a cloud. Alpha-tested materials,
skinned meshes and deformed cloth use the normal shadow vertex/coverage path.
Terrain continues to use exact height-field triangle visibility independently.

Up to eight relevant point/spot sources are selected by contribution near the
camera, with active internal flashes taking priority. Each cloud has a separate
quarter-field-resolution local-light cache, refreshed every frame. It integrates
finite RGB Beer-Lambert extinction through overlapping cloud intervals, range
falloff, spotlight cones, HG phase, mesh maps and terrain visibility. A light's
Cast shadows switch controls opaque/terrain visibility; cloud attenuation still
applies to light travelling through clouds. A cloud's Receive shadows switch
disables incident-light occlusion, consistent with its sun setting.

Internal lightning now has stable, seeded branching ribbons as well as its
scattering source. Brightness follows the existing multiple-pulse envelope.
Bolts are clipped by opaque scene depth and attenuated by intervening clouds,
then added in HDR before bloom/tone mapping. Length, width, branch count and
visibility live in Cloud component > Internal lightning and are saved in scenes.
The bolt geometry is an artistic discharge model, not an electrical simulation.

The Volumetric Clouds demo contains a sun-shadow canopy, a wall between local
lights and an authored formation, amber/blue sources, and branching storm
lightning. Fly towards the authored updraft at (1700,500,400) to compare the wall
and canopy shadows. Play or Simulate advances the storm's flash clock.

Budgets: 1024-square sun maps per cloud, 256-square point/spot faces, eight local
lights, 512 bolt segments and 262144 indirect commands. Command overflow falls
back to the complete caster list instead of dropping shadows. At eight clouds,
mesh maps use approximately 44 MiB plus a 4 MiB command arena. No hardware ray
tracing or backend extension is required. Thin sub-texel casters and narrow
beams remain limited by these raster/cache resolutions; this is not Guerrilla's
production lighting implementation.


CLOUD MESH SHADOWS, LOCAL LIGHTS AND LIGHTNING

The raster cloud renderer uses dedicated depth maps fitted to each cloud's
receiving box. The sunward depth range extends by Clouds Max distance; meshes
outside the camera frustum can still cast into a cloud. Alpha-tested materials,
skinned meshes and deformed cloth use the normal shadow vertex/coverage path.
Terrain continues to use exact height-field triangle visibility independently.

Up to eight relevant point/spot sources are selected by contribution near the
camera, with active internal flashes taking priority. Each cloud has a separate
quarter-field-resolution local-light cache, refreshed every frame. It integrates
finite RGB Beer-Lambert extinction through overlapping cloud intervals, range
falloff, spotlight cones, HG phase, mesh maps and terrain visibility. A light's
Cast shadows switch controls opaque/terrain visibility; cloud attenuation still
applies to light travelling through clouds. A cloud's Receive shadows switch
disables incident-light occlusion, consistent with its sun setting.

Internal lightning now has stable, seeded branching ribbons as well as its
scattering source. Brightness follows the existing multiple-pulse envelope.
Bolts are clipped by opaque scene depth and attenuated by intervening clouds,
then added in HDR before bloom/tone mapping. Length, width, branch count and
visibility live in Cloud component > Internal lightning and are saved in scenes.
The bolt geometry is an artistic discharge model, not an electrical simulation.

The Volumetric Clouds demo contains a sun-shadow canopy, a wall between local
lights and an authored formation, amber/blue sources, and branching storm
lightning. Fly towards the authored updraft at (1700,500,400) to compare the wall
and canopy shadows. Play or Simulate advances the storm's flash clock.

Budgets: 1024-square sun maps per cloud, 256-square point/spot faces, eight local
lights, 512 bolt segments and 262144 indirect commands. Command overflow falls
back to the complete caster list instead of dropping shadows. At eight clouds,
mesh maps use approximately 44 MiB plus a 4 MiB command arena. No hardware ray
tracing or backend extension is required. Thin sub-texel casters and narrow
beams remain limited by these raster/cache resolutions; this is not Guerrilla's
production lighting implementation.


CLOUD MESH SHADOWS, LOCAL LIGHTS AND LIGHTNING

The raster cloud renderer uses dedicated depth maps fitted to each cloud's
receiving box. The sunward depth range extends by Clouds Max distance; meshes
outside the camera frustum can still cast into a cloud. Alpha-tested materials,
skinned meshes and deformed cloth use the normal shadow vertex/coverage path.
Terrain continues to use exact height-field triangle visibility independently.

Up to eight relevant point/spot sources are selected by contribution near the
camera, with active internal flashes taking priority. Each cloud has a separate
quarter-field-resolution local-light cache, refreshed every frame. It integrates
finite RGB Beer-Lambert extinction through overlapping cloud intervals, range
falloff, spotlight cones, HG phase, mesh maps and terrain visibility. A light's
Cast shadows switch controls opaque/terrain visibility; cloud attenuation still
applies to light travelling through clouds. A cloud's Receive shadows switch
disables incident-light occlusion, consistent with its sun setting.

Internal lightning now has stable, seeded branching ribbons as well as its
scattering source. Brightness follows the existing multiple-pulse envelope.
Bolts are clipped by opaque scene depth and attenuated by intervening clouds,
then added in HDR before bloom/tone mapping. Length, width, branch count and
visibility live in Cloud component > Internal lightning and are saved in scenes.
The bolt geometry is an artistic discharge model, not an electrical simulation.

The Volumetric Clouds demo contains a sun-shadow canopy, a wall between local
lights and an authored formation, amber/blue sources, and branching storm
lightning. Fly towards the authored updraft at (1700,500,400) to compare the wall
and canopy shadows. Play or Simulate advances the storm's flash clock.

Budgets: 1024-square sun maps per cloud, 256-square point/spot faces, eight local
lights, 512 bolt segments and 262144 indirect commands. Command overflow falls
back to the complete caster list instead of dropping shadows. At eight clouds,
mesh maps use approximately 44 MiB plus a 4 MiB command arena. No hardware ray
tracing or backend extension is required. Thin sub-texel casters and narrow
beams remain limited by these raster/cache resolutions; this is not Guerrilla's
production lighting implementation.

