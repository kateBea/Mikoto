FOREST ISLAND — 10 km² — raster, 1920 x 1080

A separate compact version of Tanegashima Highlands. The original 40 km²
project remains available. One world unit is still one metre.

The island has about 10.01 km² of land, a 335 m mountain spine, 6,750 trees,
full-size shrubs/flowers/rock outcrops, meadow clearings and 21.84 km of roads.
Its terrain is 513 x 1025 vertices at 9 m spacing: 525,825 vertices and
1,048,576 triangles, about a quarter of the larger demo's terrain geometry.
Trees keep their full model sizes and overall density rather than being
compressed into the smaller footprint. The supplied fir LOD2 meshes remain
in use. Geometry, textures and wind deformation are shared with the existing
Terrain_Coast assets to avoid duplicating large binary files.

Road widths remain in metres. The road mesh is regenerated and clipped to
the new terrain cells/diagonals so it follows the new heightmap. Every tree,
flower, shrub and embedded rock is refitted to that same terrain surface.
The inland lake and coastal water keep refraction, shore foam, underwater
single scattering and approximate caustics. FFT ocean, volumetric clouds,
physical air scattering, atmospheric sky, Sky Light and wind areas remain.
The broad ocean and weather bounds retain a soft distant horizon.

Terrain shading keeps its four triplanar albedo/normal/height layers and
height blending. Grass retains the 48 blades/m² target, 0.95 m blade height,
camera range/LOD/buffer limits, and rooted wind response. The smaller paint
map retains the woodland/meadow layout and clears the widened road verges.
Terrain Generate/Sculpt/Paint and Inspector surface layers remain editable.

The new project owns its scene, heightmap, road mesh, materials and camera
script. Imported foliage models and textures are referenced from the bundled
Terrain_Coast demo; keep that asset folder alongside this project. Material
edits and terrain strokes are saved in this project. No IslandLayout.json
is needed by the demo or added to it.

CONTROLS
RMB + mouse looks, WASD flies, Q/E or Space moves vertically, Shift boosts.
The editor and play camera start at the same meadow-edge viewpoint, 2.5 m
above the ground. Base speed is 40 m/s. Rendering defaults to 1080p raster.

HEIGHTMAP
Assets/Textures/Forest_Island_Height.png is a 16-bit grayscale heightmap.
elevation in metres = -40 + (uint16 / 65535) * 550.
The original coast/mountain map is sampled at half its horizontal and
vertical dimensions. The island shape and relief are art directed; they
are not a reconstruction of Tanegashima's real mountains.

ASSETS AND REFERENCES
The existing foliage/texture licences still apply to these shared assets:
../Tanegashima_Highlands/Assets/Models/IslandVegetation/ATTRIBUTION.txt
../Tanegashima_Highlands/Assets/Models/FirTrees/license.txt
../Tanegashima_Highlands/Assets/Textures/Tanegashima_Mountain_Height.attribution.txt
Forest Ground 01 by Rob Tuytel, Poly Haven, CC0:
https://polyhaven.com/a/forrest_ground_01
https://polyhaven.com/license
Realistic fir trees pack by LOLIPOP, CC BY 4.0:
https://sketchfab.com/3d-models/realistic-fir-trees-pack-lods-gameready-f58e8b6d733e4b0586e5b7db847b89e7
https://creativecommons.org/licenses/by/4.0/
Visual reference: Toni Macaroni, Custom Engine Forest Showcase:
https://youtu.be/uR5EvBWxN14
Procedural World Generation of Far Cry 5, Etienne Carrier:
https://www.gdcvault.com/play/1025557/Procedural-World-Gen
Terrain Rendering in Far Cry 5, Jeremy Moore:
https://media.gdcvault.com/gdc2018/presentations/TerrainRenderingFarCry5.pdf
CloudsModule references and implementation limits are documented in
Mikoto/Renderer/ForwardRenderer/Passes/CloudsModule.hh in the source tree and ../Volumetric_Clouds/README.txt.

CURRENT LIMITS
Terrain remains fully resident; this sample does not add terrain streaming,
terrain/tree LOD switching or terrain shadow casting. Tree mesh instancing
and frustum/shadow culling remain in use. Shared ocean/cloud costs are not
reduced by shrinking the island. There is no promise of a particular frame
rate; the user performs the full engine build and editor performance checks.
Scene, road, vegetation, lake, asset paths and file budgets are checked by
standalone validation, Assimp and YAML loads before applying the sample.
The launcher preview is a map derived from scene data, not an editor render.
