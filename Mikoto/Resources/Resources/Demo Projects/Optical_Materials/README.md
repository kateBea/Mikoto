# Optical Materials

Open **Default Projects → Optical_Materials**, including in sandbox mode. The menu discovers the project manifest automatically.

The project owns its models, materials and Open Sans font (license in Assets/Fonts/OFL.txt). One scene unit is one meter. Closed outward-facing spheres test dielectric boundaries and PBRT photon-beam diffusion; 2 mm slabs make subsurface transmission easier to inspect.

- Back row: Air, Water, Ice, Quartz, Glass, Sapphire and Diamond IOR presets.
- Middle: PBRT Skin1, Skin2, Marble and Wholemilk coefficients, in inverse millimeters.
- Front: rough transmission, Beer-Lambert absorption and a single thin glass sheet.
- Frontmost: conductor presets using PBRT optical data and an unlit blue sample.
- Left: homogeneous fog bounded by an IOR-one dielectric interface, lit from inside.
- The left corner tests AO/contact occlusion. Use Output → Ray-traced AO in either renderer. Disable path tracing and enable ray-traced AO lighting to test raster IBL occlusion.
- The finite area, directional and point lights test soft shadows. Bloom follows path tracing in the HDR display pipeline.

Skin transport is tabulated photon-beam diffusion, not layered measured human skin. Preset IORs are approximate visible-light values. The fog uses a homogeneous RGB medium, not a density grid. Hair, cloth, spectral dispersion and motion blur are not demonstrated here.

References: [PBRT dielectric BSDF](https://pbr-book.org/4ed/Reflection_Models/Dielectric_BSDF), [PBRT BSSRDF code](https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/bssrdf.h), [beam diffusion](https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/bssrdf.cpp), [measured coefficients](https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/media.cpp), [AOIntegrator](https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/cpu/integrators.cpp#L1281).
