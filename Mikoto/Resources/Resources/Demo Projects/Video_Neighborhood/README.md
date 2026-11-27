# Video Neighborhood

A self-contained **1920 × 1080 raster** sample: three furnished duplex houses, two two-storey apartment buildings, gardens, pavements, crossings and a small outdoor cinema. Open **Video Neighborhood** in World selector (`Video_Gallery.mktproj`), press Play, then focus the Game view.

WASD walks relative to the orbit camera. Hold Shift to run, Space to jump, RMB and drag to orbit, and the wheel to zoom. R resets the player; I toggles foot IK. F11 maximizes Scene and F8 maximizes Game inside the editor workspace; press the same key again to restore the dock layout. The pink player, crossfades, virtual character controller and foot placement are reused from Animation Playground. The HDR skybox and IBL are the same daylight environment.

House 01 preserves the five original media stations and three idle viewers. House 02 has an enlarged living-room TV; House 03 has its own cinema/projector. Each apartment building contains two furnished units, an external staircase and a large downstairs TV. The street billboard loops the supplied Forza footage (`sample1`); the plaza has a large outdoor screen, projector housing and benches. There are eleven independent playback surfaces sharing eight bundled video clips.

The new street, paving, walls, gardens and block-shaped trees use ordinary cube geometry. The new cube has UVs for the brick and grass textures. Indoor floors retain the editable procedural checkerboard Surface material using #0A0B0B and #69757D. Static collision covers floors, stairs, buildings and furniture. One world unit is one metre.

Ten C# pedestrians follow three authored route loops through the existing virtual character controller: both pavements and marked crossings, the cinema plaza, and house approaches. They crossfade between Idle and Walking, pause near displays, separate from nearby pedestrians and probe static obstacles. A persistently blocked pedestrian backtracks. This is sample waypoint steering, not a navmesh or a general AI navigation system. Route points and pause durations are in `VideoTownRoutes.cs`; each actor exposes its route, initial waypoint, walking speed and visual entity name in its Script component.

All screens are emissive, play automatically, loop and have distance-attenuated spatial audio. Their materials keep the video texture on the actual display surface, including curved and letterboxed panels. Raster lighting comes from matching rectangle area lights linked to each display through **Emission source**. Their diffuse illumination and rough reflections sample the current decoded frame, including the video material's tint, emission strength and UV transform. The light's own colour and intensity remain multipliers. Disable **Material emission** for a constant-colour source. This extends the existing area-light approximation; it is not an exact textured-light integral. Mesh-light tags on the displays remain available for textured emission if path tracing is enabled manually. The projector models are props paired with emissive screens, rather than physically simulated projectors. Subtle built-in bloom is assigned globally.

All eight MP4s, model buffers/textures, animations, scripts, font and HDR sky are project-local. The fourth MP4 has only the audio sample-table metadata reordered for Mikoto's current MP4 reader: compressed packets, timing, chunk offsets and file length are unchanged. The Desktop originals are untouched. No video is re-encoded, and no individual bundled file exceeds 50 MB.

## Asset credits

Imported asset licenses remain beside the models. Asset licenses are separate from Mikoto's engine source license. Existing model licenses from Video House are retained; the apartment adds the supplied **Modern apartment interior** by Katydid (CC BY 4.0). Brick texture attribution is in `Assets/Textures/Brick_Credits.txt`; grass texture attribution is in `Assets/Textures/Grass_Credits.txt`.

### House

```text
Model Information:
* title:	Low Poly House Interior
* source:	https://sketchfab.com/3d-models/low-poly-house-interior-62b1714ef66f4e0d9f42dcd12efb3f52
* author:	Paolo Mercogliano (https://sketchfab.com/paolo.mercoglia)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "Low Poly House Interior" (https://sketchfab.com/3d-models/low-poly-house-interior-62b1714ef66f4e0d9f42dcd12efb3f52) by Paolo Mercogliano (https://sketchfab.com/paolo.mercoglia) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```

### Apartment

```text
Model Information:
* title:	Modern apartment interior
* source:	https://sketchfab.com/3d-models/modern-apartment-interior-400c9069181a4342a7142433dfa3466e
* author:	Katydid (https://sketchfab.com/Katydid.)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "Modern apartment interior" (https://sketchfab.com/3d-models/modern-apartment-interior-400c9069181a4342a7142433dfa3466e) by Katydid (https://sketchfab.com/Katydid.) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```

### Projector

```text
Model Information:
* title:	Generic white digital Projector
* source:	https://sketchfab.com/3d-models/generic-white-digital-projector-0a314ba660ee405db0df4e0c422fcd12
* author:	assetfactory (https://sketchfab.com/assetfactory)

Model License:
* license type:	SKETCHFAB Standard (https://sketchfab.com/licenses)
* requirements:	Under basic restrictions, use worldwide, on all types of media, commercially or not, and in all types of derivative works
```

### Monitors

```text
Model Information:
* title:	Basic PC Monitors
* source:	https://sketchfab.com/3d-models/basic-pc-monitors-58a2dba70e4f4752962ee98a9d6827be
* author:	Sousinho (https://sketchfab.com/sousinho)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "Basic PC Monitors" (https://sketchfab.com/3d-models/basic-pc-monitors-58a2dba70e4f4752962ee98a9d6827be) by Sousinho (https://sketchfab.com/sousinho) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```

### RetroTV

```text
Model Information:
* title:	Retro TV
* source:	https://sketchfab.com/3d-models/retro-tv-6805b862171343cba7ba261ff10579f9
* author:	ItsRobin (https://sketchfab.com/Robin-Dwivedi)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "Retro TV" (https://sketchfab.com/3d-models/retro-tv-6805b862171343cba7ba261ff10579f9) by ItsRobin (https://sketchfab.com/Robin-Dwivedi) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```

### FlatTV

```text
Model Information:
* title:	TV
* source:	https://sketchfab.com/3d-models/tv-1b7eff20a86b4cc692bc4222ac1ac252
* author:	CN Entertainment (https://sketchfab.com/cn-entertainment)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "TV" (https://sketchfab.com/3d-models/tv-1b7eff20a86b4cc692bc4222ac1ac252) by CN Entertainment (https://sketchfab.com/cn-entertainment) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```

### Character

```text
Model Information:
* title:	Xbot multiple animations
* source:	https://sketchfab.com/3d-models/xbot-multiple-animations-d96647512d9249b083d70e383944bdbf
* author:	nitwit.friends (https://sketchfab.com/nitwit.friends)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "Xbot multiple animations" (https://sketchfab.com/3d-models/xbot-multiple-animations-d96647512d9249b083d70e383944bdbf) by nitwit.friends (https://sketchfab.com/nitwit.friends) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```

The daylight HDR is farm_field_puresky_4k by Poly Haven (CC0). JetBrains Mono retains its included font license. The eight recordings were supplied by the project author.

## Greenery and updated apartments

The apartment lounges retain the complete upholstered sofa and bookshelf from the supplied apartment model. Their fitted kitchens, tables and benches now have aligned, supported cube geometry with rough stone counters. Material batches containing disconnected walls, phones, frames and counter fragments are no longer used as standalone furniture.

Scanned grass, clover and flowers fill the front gardens and cinema borders, with the approaches and pedestrian routes left clear. Hanging vines frame the houses and apartment facades. Imported greenery remains non-colliding decoration and retains its authored cutout materials.

`Assets/Particles/Falling leaves.mktpart` contains two CPU emitters using an RGBA billboard baked from the supplied single lemon leaf's actual geometry and UVs. Six garden instances have distinct seeds, downward motion, a small sideways acceleration, spin and lifetime fades. This is a billboard effect rather than a fluid wind simulation or a leaf collision solver. The source leaf model and texture remain bundled alongside the baked image.

| Added recording | Screen |
| --- | --- |
| Video Project 8 → sample6 | House 02 large TV |
| Video Project 9 → sample7 | House 03 cinema |
| Video Project 10 → sample8 | Outdoor cinema |

All eleven displays retain their sound and spatial attenuation, with the listener on the orbit camera. Added MP4s are not re-encoded: only sample-table box order is adapted when required by the current reader, preserving video/audio packets and timing. The Forza street advertisement remains sample1.

### Plants

```text
Model Information:
* title:	Scanned Plants and Grass (Free Download)
* source:	https://sketchfab.com/3d-models/scanned-plants-and-grass-free-download-d0fbc8fecfbb40bf9029bc97b6e9e122
* author:	Kless (https://sketchfab.com/klessgyzen)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "Scanned Plants and Grass (Free Download)" (https://sketchfab.com/3d-models/scanned-plants-and-grass-free-download-d0fbc8fecfbb40bf9029bc97b6e9e122) by Kless (https://sketchfab.com/klessgyzen) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```

### Vines

```text
Model Information:
* title:	Realistic Vine plants 3d model free
* source:	https://sketchfab.com/3d-models/realistic-vine-plants-3d-model-free-d7a591201b904a66a114ebb9347bc6b8
* author:	iGauravRajput (https://sketchfab.com/iGauravRajput)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "Realistic Vine plants 3d model free" (https://sketchfab.com/3d-models/realistic-vine-plants-3d-model-free-d7a591201b904a66a114ebb9347bc6b8) by iGauravRajput (https://sketchfab.com/iGauravRajput) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```

### Leaf

```text
Model Information:
* title:	Lemon Leaf
* source:	https://sketchfab.com/3d-models/lemon-leaf-0ecdce2705c649638c7d4d791899e1bd
* author:	YouniqueĪdeaStudio (https://sketchfab.com/sinnervoncrawsz)

Model License:
* license type:	CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
* requirements:	Author must be credited. Commercial use is allowed.

If you use this 3D model in your project be sure to copy paste this credit wherever you share it:
This work is based on "Lemon Leaf" (https://sketchfab.com/3d-models/lemon-leaf-0ecdce2705c649638c7d4d791899e1bd) by YouniqueĪdeaStudio (https://sketchfab.com/sinnervoncrawsz) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/)
```


Stochastic screen-space reflections are enabled for this raster sample. The checkerboard floor uses 0.4 roughness so nearby video screens, furniture, and characters can reflect without turning the town into a mirror. Renderer → Stochastic screen-space reflections exposes the tracing budget, half-resolution mode, roughness cutoff, metre-based distance/thickness, spatial reuse, sampling bias, temporal weight, and intensity.

The implementation traces GGX rays against a conservative min-depth hierarchy, reuses neighboring hits with normalized BRDF/PDF weights, filters HDR scene radiance by the ray footprint, and reprojects reflection history. Misses keep the existing environment specular lobe. It runs before tone mapping, independently of hardware ray tracing; path tracing and hardware ray-traced reflections take precedence. Clear coat and stylized receivers retain their existing environment path. Off-screen/occluded geometry, transparent receivers, terrain, water, and particles are not SSR receivers. Temporal rejection uses depth/normals and neighborhood clamping; it does not have per-object motion vectors or Frostbite's adaptive tile ray allocation.

References: [Frostbite, Stochastic Screen-Space Reflections (2015)](https://advances.realtimerendering.com/s2015/Stochastic%20Screen-Space%20Reflections.pptx), [AMD FidelityFX SSSR](https://github.com/GPUOpen-Effects/FidelityFX-SSSR), [Wicked Engine SSR shaders](https://github.com/turanszkij/WickedEngine/tree/master/WickedEngine/shaders).
