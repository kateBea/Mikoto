# Clouds and Fog

Open **Default Projects → Volumes** in the editor or sandbox mode.
The scene uses only project-local meshes and materials. A Game camera is included.
This project currently demonstrates volume configuration and editor/data wiring.
Cloud/fog shading is disabled in both raster and path tracing; the registered
volume rendering passes are empty while their implementation is reconsidered.

- **Sky clouds**: procedural cloud layer above the scene.
- **Local storm cloud**: a movable ellipsoid with a denser volume material.
- **Ground mist**: height-dependent fog inside a local box.
- **Global haze**: a weak height medium with an explicit finite horizon radius.
- **Grid fog**: a sphere containing a trilinear 3D density field. The importable
  source is `Assets/Volumes/SoftSphere.mktvol`; saved material voxels are embedded.
- **Shadow pillars**: geometry retained for future volume-lighting tests.

Select an entity, edit its Cloud/Fog shape, and click its material field to edit
the shared density/scattering properties. Drop another volume `.mktmtl` onto that
field to reuse it. Play/Simulate advances the wind values in the collected data.

The normal Sandbox startup scene keeps its original skybox and contains no
Cloud/Fog entities. Use this separate project to inspect the retained components,
materials, serialization, collection and upload.

Sources: [PBRT media](https://pbr-book.org/4ed/Volume_Scattering/Media),
[CloudMedium](https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/media.h#L391),
[HG phase functions](https://pbr-book.org/4ed/Volume_Scattering/Phase_Functions),
[volume integrators](https://pbr-book.org/4ed/Light_Transport_II_Volume_Rendering/Volume_Scattering_Integrators).
