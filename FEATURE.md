# Mikoto Features

Mikoto is an experimental C++20 game engine and editor under active development.
This checklist describes the current implementation, rather than feature parity
between every graphics backend or a guarantee of production readiness.

- [x] Implemented. **EXPERIMENTAL** and **WIP** entries have the limitations stated beside them.
- [ ] Not available as a complete feature. Existing scaffolding is identified where applicable.

Unchecked features are not automatically scheduled. The roadmap below separates
the next priorities from longer-term candidates; release versions are not assigned yet.

### Rendering

- [x] Vulkan rendering backend
- [x] Direct3D 12 rendering backend (feature parity and driver validation ongoing)
- [x] Graphics API abstraction through the RHI
- [x] Runtime shader compilation through Slang
- [x] Bindless resource access and buffer device addressing on supported backends
- [x] Frame graph with resource dependencies, barriers and transient resource management
- [x] Clustered forward PBR shading with a depth/normal prepass
- [x] Metallic/roughness materials, normal maps, emissive textures and alpha masking
- [x] Transparent materials with sorted alpha blending
- [x] Double-sided materials and mirrored-instance handling
- [x] GPU indirect drawing and mesh instancing
- [x] Persistent geometry/material slots and sparse GPU updates
- [x] GPU frustum culling (draw/batch bounds; conservative handling of animated meshes)
- [x] Perspective and orthographic cameras
- [x] Separate Scene and Game cameras/views
- [x] Camera render-to-texture through `TextureRenderComponent`: independent raster/path-traced captures, selectable resolution, 30/60/120 FPS capture limits, Base Color or Emissive material overrides, and serialized camera references/settings (EXPERIMENTAL; Vulkan/D3D12, compatible GPU required for path tracing)
- [x] MSAA (EXPERIMENTAL; sample counts depend on backend/device support)
- [x] Infinite editor grid
- [ ] Deferred lighting renderer (the existing G-buffer/prepass does not constitute deferred lighting)
- [ ] Dedicated transfer and compute queues in Vulkan and Direct3D 12
- [ ] Async compute scheduling and synchronization
- [ ] Order-independent transparency (evaluate per-pixel linked lists)
- [ ] Height, bump, displacement and parallax mapping
- [ ] Improve the renderer's dedicated 2D rendering path
- [ ] Hi-Z occlusion culling
- [ ] Automatic mesh LOD selection and generation
- [ ] Deferred decals
- [ ] Foliage and grass rendering with authoring tools
- [ ] Fur rendering
- [ ] Water simulation (module scaffold only)
- [ ] Ocean rendering
- [ ] Visibility buffer
- [ ] General split-screen/camera compositing workflow
- [ ] Complete OpenGL and Direct3D 11 backend parity (backend code exists)

### Terrain and landscape

- [x] Terrain entities with configurable sections, component counts, vertex spacing, height scale and base elevation (EXPERIMENTAL)
- [x] Heightmap terrain rendering with compute-generated vertices/indices, bilinear height sampling, device-local geometry buffers and vertex pulling; geometry regenerates when its inputs change
- [x] Heightmap import through the shared image loader, including HDR and preserved 16-bit PNG data
- [x] GPU generation of combined Perlin/fBm height textures with frequency, amplitude, persistence, lacunarity, octave, blend and offset controls; compute can consume the generated texture directly, with readback retained for preview, export and image retention
- [x] Landscape panel with Create New, Import from File and Generated Noise tabs, Fit to Data, a heightmap/noise image-button preview and optional generated heightmap export to HDR
- [x] Unlit height-based terrain colors with editable low/middle/high world properties and depth-tested grid/vertex debug views
- [x] Serialized landscape configuration, imported heightmap references, generated noise settings and debug options
- [ ] Terrain material shading and texture/layer blending
- [ ] Terrain sculpting, painting, brushes and edit layers
- [ ] Terrain LOD, streaming and chunk visibility culling
- [ ] Terrain integration into object picking, shadow casting and ray-traced geometry

### Lighting and shadows

- [x] Point, spot and directional lights
- [x] Clustered light assignment
- [x] Dynamic spot-light shadows
- [x] Point-light shadows using six shadow views
- [x] Cascaded directional shadows
- [x] Alpha-masked shadow casters
- [x] Shadow caster culling and configurable shadow quality
- [x] HDR skyboxes and environment lighting
- [x] Image-based lighting with irradiance, prefiltered specular environment maps and a BRDF lookup
- [x] Atmospheric scattering and aerial perspective (EXPERIMENTAL; no volumetric clouds)
- [x] Screen-space ambient occlusion (SSAO)
- [x] Hardware ray-traced shadows, reflections and global illumination (EXPERIMENTAL; compatible GPU required)
- [x] Separate ray-tracing shader tables with explicit record selection, setup-time builds and command-context binding (Vulkan 1.3 / D3D12)
- [x] Progressive path tracing with sample and bounce controls (EXPERIMENTAL)
- [ ] Area lights
- [ ] Ray-traced ambient occlusion (RTAO)
- [ ] Coloured/translucent shadow transmission
- [ ] Baked lightmaps and partially baked shadows
- [ ] Local baked or dynamic reflection probes (global environment prefiltering is available)
- [ ] Planar reflections
- [ ] Ambient occlusion volumes
- [ ] Volumetric clouds and a weather system
- [ ] Complete physically based camera/lens simulation for path-traced depth of field

### Post processing

- [x] Tone mapping with selectable operators
- [x] Exposure and gamma controls
- [x] Configurable bloom
- [x] Cinematic raster depth of field (EXPERIMENTAL; foreground reconstruction needs further work)
- [x] Physical lens controls: filmback, focal length, aperture, focus distance and diaphragm shape
- [x] Depth-of-field quality settings and focus-layer visualization
- [x] Chromatic aberration with intensity and direction controls
- [x] Selection outlines using jump flooding
- [ ] Automatic exposure/eye adaptation (placeholder pass)
- [ ] LUT colour grading and expanded colour correction
- [ ] Motion blur
- [ ] FXAA, SMAA and temporal anti-aliasing
- [ ] Dedicated SSAA resolve/downsampling mode
- [ ] Temporal upscaling
- [ ] Lens flare, lens dirt, film grain, vignette and lens distortion (placeholder passes)

### Particles

- [x] Particle authoring preview with playback/step, grouped emitter settings, presets and `.mktpart` load/save; isolated CPU/GPU preview with street HDR, ground and infinite grid
- [x] Scene-owned particle emitters with CPU simulation
- [x] Instanced camera-facing particle rendering
- [x] Burst and continuous emission, lifetime, acceleration and damping controls
- [x] Colour and size changes over particle lifetime
- [x] Deterministic per-emitter random sequences
- [x] Play, pause, restart, clear and stop-emitting controls
- [x] Serialized emitter recipes and `.mktpart` presets
- [ ] Complete particle system and authoring workflow beyond the current CPU-emitter implementation
- [x] Optional GPU compute particle simulation and depth sorting (experimental; CPU/GPU selector in the Renderer panel)
- [ ] Textured particles, flipbooks and mesh particles
- [ ] Particle collisions, lighting and shadows
- [ ] Soft intersections with scene depth
- [ ] Unified sorting of particles and transparent meshes
- [ ] Particle integration into the bloom/depth-of-field chain
- [ ] Smoke

### HUD and text

- [x] Canvas drawing: rectangles, lines, sprites and text
- [x] Interactive buttons, toggles and sliders
- [x] Canvas-owned widget hierarchy with stable IDs, inspector editing and scene serialization
- [x] Pointer interaction on all canvas widget types, hover/press feedback, keyboard focus and slider capture
- [x] Lua canvas events and widget access by stable ID
- [x] Anchored/stretched layout and sprite sizing/blending options
- [x] Scene text and screen-space text
- [x] MSDF font rendering
- [x] Slug text rendering through SlugHorn curve/band atlases and dedicated Slang shaders for world-space and screen-space text (EXPERIMENTAL; previous Slug shaders retained)
- [x] Colour emoji with bundled Noto fallback and HarfBuzz shaping of supported emoji sequences (MSDF, Slug and canvas text)
- [x] Text-component outline width/colour with scene serialization (MSDF and Slug)
- [x] SlugHorn-backed Slug fills with curve-distance rounded outlines; existing layout, incremental glyph loading and colour-emoji fallback preserved. Canvas authoring, SVG and gradient features from SlugHorn are not integrated.
- [x] File-driven Unicode/emoji debug layer with side-by-side MSDF/Slug comparison
- [x] Unicode text utilities, glyph lookup and kerning
- [x] Text alignment, size and colour controls
- [x] JSON-based localization service with language selection and translation lookup
- [ ] Improve the engine's own GUI rendering and widget system
- [ ] Integrated localized-text authoring/binding in HUD components
- [ ] Rich-text markup and full complex-script shaping
- [ ] FFmpeg multimedia integration for audio/video decoding, encoding and container handling
- [ ] Video playback and video-backed UI textures using FFmpeg, with synchronized audio

### Scene and editor

- [x] EnTT entity component system
- [x] Entity hierarchy, parenting and world transforms
- [x] YAML scene/project serialization
- [x] Dockable Dear ImGui editor panels
- [x] Scene viewport and Game preview
- [x] Play, pause and simulation workflows
- [x] Object picking, selection and transform gizmos
- [x] Independent HighlightComponent outlines using antialiased silhouettes and separable jump flood with MSAA
- [x] Inspector editing for scene components
- [x] Content browser with filtering, thumbnails and drag-and-drop workflows
- [x] Prefab creation/instantiation and a prefab browser (editing workflows remain limited)
- [x] Material editor with live preview and material asset persistence
- [x] Scene/project workspace operations with save/discard/cancel handling and progress reporting
- [x] Console, runtime console and compiler-status panels
- [x] Editor themes and preferences
- [x] Dockable editing Viewport panel with a top-level editor toolbar, flat theme-aware buttons and fading tooltips. Multiple independent scenes/views are not implemented yet.
- [x] Syntax-highlighted Lua, C#, and C++ script previews using the maintained goossens/ImGuiColorTextEdit, with Mikoto identifiers, line numbers, and shared Inspector/Hierarchy/Script Graph integration
- [ ] Multiple-entity selection
- [ ] Select entities by clicking their viewport overlay icons
- [ ] Replace Dear ImGui's backend renderers with a Mikoto-owned renderer through the RHI
- [ ] Translate the remaining editor UI using the existing localization system
- [ ] Complete undo/redo workflow (menu entries are placeholders)
- [ ] Material browser panel with asset tracking (panel scaffold only)
- [ ] Animation browser panel with asset tracking (panel scaffold only)
- [ ] General asset dependency/reference inspector
- [ ] Nested prefab overrides, variants and a dedicated prefab editing workflow
- [ ] Material node graph with generated Slang shaders
- [x] Material graph authoring: surface/post-process/particle domain rules, typed connections, cycle/output validation, texture previews, named comment groups and saved layout
- [x] Validated Mikoto material graph YAML export with typed values, explicit connections, editor preview and copy
- [x] SurfaceMaterial naming and standalone ParticleMaterial assets; legacy PhysicalMaterial asset loading preserved
- [ ] Slang function generation and pipeline integration for authored material graphs
- [ ] Theme system to select and apply, serialize and load themes

### Animation

- [x] Skeletal animation import and playback through Ozz
- [x] Multiple glTF skeletons per model, mesh-to-rig skin bindings and independent character poses with synchronized clip playback
- [x] GPU skinning cache with vertex-shader fallback
- [x] Clip playback speed, looping and crossfades
- [x] Single-layer animation controllers with states and transitions
- [x] Float, integer, boolean and trigger parameters with transition conditions
- [x] Animation graph editor
- [x] Keyframe timeline for transforms and light colour/intensity
- [x] Animation/controller serialization
- [x] Aim and two-bone inverse kinematics
- [x] Skeleton/debug visualization
- [ ] Complete bone/armature visualization in the editor, including per-rig visibility for split skeletons, beyond the existing skeleton debug view
- [ ] Improve animation-to-mesh binding and animator-based playback/control workflows
- [ ] Morph targets/blend shapes
- [ ] Blend spaces, additive layers and animation masks
- [ ] Complete root-motion workflow
- [ ] Animation retargeting tools

### Scripting and input

- [x] Lua scripting through sol2
- [x] C# scripting through .NET
- [x] Shared scripting system with language-specific creation/loading services
- [x] Native scene, entity and input bindings
- [x] GLM-backed math bindings, including the C# native bridge
- [x] C# source compilation, file-change monitoring and compiler diagnostics
- [x] Game-level C# projects and Visual Studio solutions, project-reference builds on Play, and Visual Studio/VS Code project opening
- [x] Script lifecycle integration with scene playback
- [x] Fly-camera and first-person C# examples
- [x] Native C++ engine/application development
- [x] Keyboard, mouse and clipboard input
- [ ] Native C++ scripting with script loading and hot reload
- [ ] Complete gamepad/controller input API
- [ ] Input action maps and rebinding UI

### Physics

- [x] Jolt-based 3D physics
- [x] Scene-owned physics worlds and simulation controls
- [x] Static, kinematic and dynamic rigid bodies
- [x] Mass, friction, restitution and linear/angular velocity controls (inertia calculated from collider geometry)
- [x] World gravity, gravity presets and per-body gravity enable/disable
- [x] Automatic sleeping and activation on relevant edits or vehicle input
- [x] Linear-cast continuous collision detection (no per-body mode selector)
- [x] Box, sphere and capsule colliders with dimensions, offsets and trigger settings
- [x] Convex mesh colliders for moving bodies and static triangle-mesh colliders
- [x] Collider scaling/offsets and a vehicle-specific vertical center-of-mass offset
- [x] Trigger sensors without physical contact response
- [x] Native contact enter/exit event queue (per-subshape events; no script dispatch or stay events)
- [x] Moving/static collision layers and bridge-neighbor collision filtering
- [x] Distance constraints for bridge segments (no general joint component)
- [x] Four-wheel vehicles with front-wheel/AWD drive, ray/cylinder-tested suspension, front steering, braking, rear handbrake and wheel visuals (EXPERIMENTAL)
- [x] Shared vehicle suspension settings, anti-roll bars and serialized inspector settings
- [x] Jolt engine, automatic gearbox, clutch, differential and tire simulation (most detailed settings use Jolt defaults)
- [x] Vehicle Driving test scene with WASD controls, a mouse-controlled orbit camera, suspension bumps and a constrained bridge
- [x] Multithreaded physics simulation through Jolt's job thread pool
- [x] Physics debug rendering with body wireframes, activity colors and constraint/limit/reference-frame views
- [ ] Gameplay APIs for applying forces, impulses and torques
- [ ] Advanced rigid-body tuning: damping, custom inertia, allowed motion axes and sleep thresholds
- [ ] Kinematic target-over-time movement (velocity controls exist; transform edits currently set poses directly)
- [ ] Dedicated cylinder, tapered-shape and plane collider components
- [ ] Compound collider authoring (multiple collider components do not combine into one body shape)
- [ ] Height-field terrain colliders and cooking
- [ ] General collider hierarchies and full center-of-mass editing
- [ ] Script collision/trigger callbacks, stay events and entity-pair contact aggregation
- [ ] User-defined collision layers, collision matrices and collision-group authoring
- [ ] General physics raycast API (vehicle casts are internal; the general RayCast class is a scaffold)
- [ ] General shape-sweep API and a vehicle sphere-cast option (vehicle cylinder casts are available)
- [ ] Point/shape overlap and broadphase query APIs
- [ ] Reusable distance-joint components and spring controls (bridge-specific distance links exist)
- [ ] Fixed, point, hinge, slider, cone, swing-twist and 6-DOF joint components and authoring tools
- [ ] Gear, rack-and-pinion, pulley and path constraints
- [ ] Joint motors and drive-target APIs
- [ ] Rigid-body and virtual character controllers with dedicated authoring workflows
- [ ] Ragdoll construction, skeleton mapping and animation/physics blending
- [ ] Arbitrary vehicle wheel layouts and per-wheel dimensions, suspension and alignment settings
- [ ] Detailed engine, gearbox, clutch, differential and tire tuning, plus manual shifting
- [ ] Motorcycle and tracked-vehicle integration
- [ ] Soft-body simulation, mesh binding and constraint authoring
- [ ] Cloth simulation and authoring workflow
- [ ] Jolt GPU strand-hair simulation and rendering integration
- [ ] Rigid-body buoyancy coupled to water surfaces (rendered waves do not make bodies float)
- [ ] Wind simulation
- [ ] Physics snapshots, deterministic replay and state restoration (current stepping uses bounded frame delta and collision substeps, without a fixed-tick accumulator)
- [ ] End-to-end double-precision large-world physics (scene transforms and physics conversions currently use floats)

### Audio

- [x] Audio playback through miniaudio
- [x] Scene audio sources and listeners
- [x] Spatial audio and distance attenuation
- [x] Steam Audio HRTF headphone spatialization for 3D sources; stereo bypass for 2D audio
- [x] Volume, pitch, looping, mute and Doppler controls
- [x] Audio mixer with master gain, per-source gain/mute, playback and 2D/3D controls
- [x] Audio source inspector with playback and spatial settings; live voice monitoring
- [ ] Audio bus routing and effects editor
- [ ] Audio occlusion, reverb zones and acoustic simulation

### Assets

- [x] glTF/GLB import through fastgltf and additional formats through Assimp
- [x] Shared bounded import/compression workers, unique texture loading, shared mesh geometry and batched model uploads
- [x] Incremental queued model entity creation and asynchronous model preview preparation
- [x] Procedural mesh creation
- [x] Packed mesh attributes and shared geometry allocation
- [x] Texture, model, font, audio and script asset loading
- [x] Asset caches and content-browser previews
- [x] HDR environment import and cubemap generation
- [x] BC7 compression for LDR texture imports
- [x] BC6H unsigned compression for supported HDR texture imports
- [x] KTX2 texture cache serialization and compressed GPU uploads
- [x] Compressed-source detection by file signature for supported KTX2 inputs
- [x] Project YAML compression toggle shared by editor and runtime
- [x] Parallel block compression with configurable worker count
- [x] Texture compression benchmark/debug layer
- [ ] Complete the asset HDR image processor: load an equirectangular HDR image from a file path and project it onto six cubemap faces (initial implementation reference: [ivarout/HdriToCubemap](https://github.com/ivarout/HdriToCubemap)).
- [ ] General BC1–BC5 import compression options
- [ ] Full mip-chain generation/storage in the compressed import cache
- [ ] General KTX2 cubemap/array, supercompression and Basis transcoding support
- [ ] Native compressed DDS upload preservation through the import/cache path
- [x] Initial asynchronous model residency requests, bounded import scheduling, pending scene roots and unused-model cache eviction
- [ ] Complete asset streaming and residency-budget management (texture mip/mesh LOD streaming, byte-budget admission and reclaimable geometry remain pending)

### Networking

- [x] Asio-based TCP/UDP sockets and client/server building blocks
- [x] HTTP/HTTPS utilities with OpenSSL support
- [x] GameNetworkingSockets 1.6.0 exposed through the Networking module
- [x] Direct access to GameNetworkingSockets connections/messages (application-owned initialization, callbacks and shutdown)
- [ ] Mikoto-level multiplayer session and replication APIs
- [ ] Networked entity ownership, RPCs and state synchronization
- [ ] Prediction, reconciliation and interpolation
- [ ] Matchmaking/lobbies and application-provided P2P signaling
- [ ] Steamworks integration (GameNetworkingSockets alone does not provide this)

### Runtime, platforms and diagnostics

- [x] Standalone runtime application with `game.yaml` configuration
- [x] Command-line game packaging/staging (validates project assets and relocates scene references)
- [x] Modular application configuration through CMake
- [x] Windows build presets, including RelWithDebInfo for profiling
- [x] Linux build configuration and Vulkan path (platform validation ongoing)
- [x] vcpkg manifest and FetchContent dependency providers
- [x] Task-based background work through Taskflow
- [x] Logging, assertions, file utilities and file watching
- [x] Custom memory allocators and reference-counted resources
- [x] CPU instrumentation and optional Tracy profiling
- [x] GPU queries/statistics, frame-graph statistics and pass visualization
- [x] Frustum, cascade, physics and depth-of-field debug views
- [ ] Editor game-export wizard and complete shipping/deployment workflow
- [ ] macOS/Metal support
- [ ] Mobile and console platforms

## Known issues and limitations

- **Window controls:** minimizing and maximizing the window are currently not working.
- **Depth of field:** foreground/silhouette leakage has been reported. The effect remains experimental; a complete fix has not been verified.
- **Barrier allocations:** local Vulkan barrier vectors can exceed their inline capacity and allocate. Disabling overflow is not a fix unless every batch is guaranteed to fit.
- **Large scenes:** slot retirement and other per-entity CPU work still warrant profiling even after the geometry-cache and redundant-initialization improvements.
- **Render to texture:** each component owns a complete renderer; capture frequency is capped by scene updates. The receiving surface uses its authored material during its own capture to prevent texture feedback. Inspector renderer-configuration options remain drafts.
- **Texture cache:** currently accepts single-level 2D KTX2 with supported formats and no supercompression. Unsupported HDR values/alpha use an uncompressed fallback; BC6H does not store alpha.
- **Particles:** CPU simulation is the default; optional GPU compute simulation supports up to 256 emitters and 65,536 reserved particle slots per view. Switching backends or rebuilding the GPU frame graph restarts GPU particles; GPU live/dropped counts are not read back. Particles are raster-only, are not interleaved with transparent meshes and are not included in the existing bloom/DoF chain.
- **Feature parity:** an RHI type, dependency or empty pass does not imply a complete feature on every backend. Experimental rendering paths still need scene/device coverage.
- **D3D11 and OpenGL:** These two graphics APIs are currently in an experimental state as they do not play well with how the framegraph is designed, they only have the corresponding backend RHI implementations and do not render anything through the framegraph.


## Planned for future Development
- **Android Support:**: Support for Android. 
