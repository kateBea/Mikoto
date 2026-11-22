# Animation Playground

A 1920 x 1080 raster demo using a daylight HDR skybox and matching image-based lighting. Open it from the World selector, press Play, then click the Game view.

WASD moves relative to the orbit camera. Hold the right mouse button and drag over the focused Game image to orbit; use the wheel to zoom. Hold Shift while moving to crossfade to running; release it to crossfade back to walking. Space triggers the Jump clip and a physics jump, I toggles foot IK, and R resets the player.

The character uses Jolt's virtual character controller for collisions, steps, slopes and jumping. Ozz applies two-bone constraints after the blended animation pose. The foot script probes static collision geometry and fades the constraint during swing and airborne motion. The foot authoring script compares ankle/heel support to actor support and applies terrain offsets only. Flat-ground animation is unchanged. On uneven ground, knees use a stable forward hint and feet follow their sampled directions; a separate visual child supplies the small pelvis offset needed to reach a lower tread. The collision capsule follows physics. The demo does not implement a blend space. Idle/walk/run use the existing timed clip crossfades.

Exhibits demonstrate head aim IK, an arm reach constraint, automatic controller transitions, independent idle/hover playback, multi-mesh skinning and an authored jump clip. Deadpool plays the imported `ymca` dance across his skinned meshes, and Miss Galaxy loops her imported `walking` clip.

The ground, steps and ramp use Surface material procedural checkerboards. Cell size and offset use world units so scaled boxes retain consistent square sizes. Open the ground or stairs material to edit the colours, cell size, offset or enable checkbox.

Demo-local glTF files add normalization parents and reference the original shared prefab buffers/textures. The source assets and their licenses remain in Prefabs; no large mesh buffers are copied. Moving this demo outside the bundled Resources tree requires copying or relinking those shared assets.

## Asset credits

- Xbot: {"author":"nitwit.friends (https://sketchfab.com/nitwit.friends)","license":"CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)","source":"https://sketchfab.com/3d-models/xbot-multiple-animations-d96647512d9249b083d70e383944bdbf","title":"Xbot multiple animations"}
- Bee: {"author":"Jakob Henerey (https://sketchfab.com/hellonintendo8)","license":"CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)","source":"https://sketchfab.com/3d-models/bee-d4683f98637745f1a989bb013215c8de","title":"Bee"}
- Deadpool: {"author":"nitwit.friends (https://sketchfab.com/nitwit.friends)","license":"CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)","source":"https://sketchfab.com/3d-models/deadpool-dancing-0257cc47a19f46cd9f1e7dbf61b18dd7","title":"Deadpool Dancing"}
- MissGalaxy: {"author":"Donte_Loves_Art (https://sketchfab.com/Donte_Loves_Art)","license":"CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)","source":"https://sketchfab.com/3d-models/miss-galaxy-32ca79bf7c4043dcbc36d28b22436303","title":"Miss Galaxy"}

Xbot multiple animations by nitwit.friends, CC BY 4.0: https://sketchfab.com/3d-models/xbot-multiple-animations-d96647512d9249b083d70e383944bdbf

The glTF importer preserves clip names (`Idle`, `Walking`, `Running`, `Jump`). The player-only `PlayerXbot.gltf` uses a 372-byte override track to remove upward hip travel from Jump while retaining crouch/landing movement. Physics supplies the actual jump height. Exhibit characters keep the original source animation.

Checkerboard projections follow the geometric surface normal so cell-height boundaries on flat treads do not cause speckling. Distant cells retain analytic filtering.
