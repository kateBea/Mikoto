CLOTH PLAZA - raster, 1920 x 1080, one unit = one metre

Five national flags: Spain, Japan, France, Germany and Italy. Fourteen-metre
poles carry 6 x 4 metre flags (Germany is 6.67 x 4 to preserve its 5:3 ratio).
Each flag has a uniform 61 x 41 particle grid, a fully pinned pole seam,
160 g/m2 fabric and moderate gusting wind. Air forces feed Jolt before its
stretch and distance-bend constraints; no visual wave overwrites solved nodes.
The original small flags' pins were correct. Their heavier loading, stronger
crosswind and coarse folds encouraged bunching. The larger mesh and retuned
mass/airflow reduce that behaviour; the vertex-dot debug overlay is unchanged.
The current Jolt soft-body path has no cloth self-collision. Extreme wind or
manually folded layers can still interpenetrate; this is not a self-contact solver.

Only the five flags have pinned vertices, along their pole seams.
The four 61 x 45 tablecloths and both 49 x 49 sphere covers have no pinned
vertices. They settle through gravity, collisions and friction against their
supports and are free to move or slide.
The original soft pressure spheres remain available for comparison.

The ground uses SurfaceMaterial's world-space procedural checkerboard with
#0A0B0B / #69757D, 2 m cells. It extends beyond the plaza into the atmosphere's
aerial haze. Warm evening light, Earth atmosphere and a skybox camera replace the
short slab-to-brown-background cutoff. Aerial LUT range is 30 km and the camera
far plane is 25 km. Camera and project start in the same overview.

Run Simulate for the cloth. Select a Wind Area to edit gusts and direction.
The dot overlay and Wind Area debug view are unchanged. Mesh pin indices are
recorded after Mikoto's production mesh optimization, not the source grid order.

ASSET SOURCES AND LICENCES
Flag PNGs downloaded from https://flagcdn.com/w1280/{es,jp,fr,de,it}.png
Flagpedia/FlagCDN documents its Wikimedia Commons flag sources:
https://flagpedia.net/download/api
https://commons.wikimedia.org/wiki/File:Flag_of_Spain.svg
https://commons.wikimedia.org/wiki/File:Flag_of_Japan.svg
https://commons.wikimedia.org/wiki/File:Flag_of_France.svg
https://commons.wikimedia.org/wiki/File:Flag_of_Germany.svg
https://commons.wikimedia.org/wiki/File:Flag_of_Italy.svg

Fabric Pattern 07 by Rob Tuytel, Poly Haven, CC0:
https://polyhaven.com/a/fabric_pattern_07
https://polyhaven.com/license
Three 1K colour variants share the same normal and roughness maps. No model
archive, large source bake or online dependency is bundled with this project.
All flag/drape meshes are generated specifically for this Mikoto sample.

PHYSICS REFERENCE
Jolt's cloth/soft-body constraint construction:
https://github.com/jrouwe/JoltPhysics/blob/master/Samples/Utils/SoftBodyCreator.cpp

VALIDATION
Standalone Jolt tests use the production optimized meshes and the same wind
kernel: pole seam fixed at 30/60/120 Hz, ongoing motion, finite positions,
bounded stretch, and six pinned table/sphere drapes simulated for ten seconds.
Full engine build and in-editor appearance are left for the user's verification.

EVENING CLOUD LAYER
The requested -7-degree Z rotation remains on the sun entity. The atmosphere
follows that directional light. Clouds keep their 1100 m base, 450 m layer
and 20 km radius, with the new stratocumulus profile and regional weather.
The cloth project uses the 256 field setting to resolve the broken banks.
The renderer quality control remains available for a lower-resolution field.

Sunset_Clouds exposes Cloud form, Weather frequency, Coverage variation,
Cloud-type variation, Cloud-top variation and Wind shear. Primary bodies
use a separate Perlin-Worley volume; fine Worley erosion breaks their edges.
Height shaping happens before coverage so bodies have rounded, uneven tops.
The RGB transport, dense-ray sun occlusion and temporal occlusion fixes remain.

Viewport settings -> Cloth vertices controls the particle markers: Hidden,
Selected or All. Hiding them leaves grabbing and simulation active.
Only the five flag seams are pinned; tablecloths and sphere covers are free.

AMBIENT OCCLUSION
This moving raster sample uses SSAO by default. Hardware RT effects are
explicitly disabled in the project. A Sky Light no longer silently enables
ray-traced AO: select that method in Renderer when desired, then the Sky Light
can supply its distance and sample budget. Moving cloth changes the RT scene
revision each frame, resetting its progressive history. Previously the Sky
Light forced raw two-sample visibility onto every object, producing black
speckles even with the Renderer SSAO toggle off. Cloth physics and raster
cloud rendering were not the source of those samples.
