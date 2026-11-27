//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_RENDERER_CLOUDS_CLOUD_VOLUME_IO_HH
#define MIKOTO_RENDERER_CLOUDS_CLOUD_VOLUME_IO_HH

#include <Filesystem/Path.hh>

#include <Renderer/Clouds/CloudDensityBaker.hh>

namespace mikoto::renderer {

    struct CloudImportedVolume {
        CloudLayerGrid mGrid{};
        core::float3 mWorldMinimum{}, mWorldMaximum{ 1 };
        core::f32 mDensityMultiplier{ 1 };// Restores density values above the normalized voxel range.
    };

    /** Offline DCC interchange. Imported runtime voxels own their data and do not retain VDB trees. */
    // VDB is an optional CPU authoring dependency (MIKOTO_ENABLE_OPENVDB and the
    // vcpkg cloud-authoring feature). Runtime materials own bounded voxels, not VDB
    // trees; procedural clouds, saved grids and TGA import work without OpenVDB.
    // Native API: https://www.openvdb.org/documentation/doxygen/codeExamples.html
    class CloudVolumeIO final {
    public:
        /** Metadata-only query for editor channel selectors; does not load tree payloads. */
        MKT_NODISCARD static auto ListVdbGrids( const filesystem::Path &path ) -> eastl::vector<eastl::string>;
        /** Read scalar fog or profile/detail/density channels in a shared world-space domain. */
        // Float/double fog channels may have independent affine transforms. Profile
        // supplies the domain when present, otherwise density; each channel samples the
        // same world domain through its own transform. Level sets, frustum/non-affine
        // transforms and invalid samples are rejected. Values above one normalize with
        // a returned density multiplier. Use the reported bounds for metre-scale placement;
        // imported material placement is independent of the source DCC origin.
        MKT_NODISCARD static auto ImportVdb( const filesystem::Path &path,
            const CloudVolumeImportSettings &settings = {} ) -> CloudImportedVolume;
        /** Select any numbered TGA slice. Matching siblings are sorted numerically; gaps are rejected. */
        // Select any slice in a numbered stack: numeric ordering handles indices >9;
        // duplicates, gaps and inconsistent dimensions are errors. RGB means profile/type/
        // density; RGBA means distance/profile/type/density. Encoded distance is discarded
        // and rebuilt. Image X/Y map to volume X/Y and slice index to Z, with TGA orientation
        // metadata respected. Values are linear data, never gamma-decoded.
        MKT_NODISCARD static auto ImportSlices( const filesystem::Path &path,
            const CloudVolumeImportSettings &settings = {} ) -> CloudImportedVolume;
        /** Export profile/detail/density as named float grids, with physical voxel spacing and origin. */
        // Export writes three named float grids with physical spacing and origin.
        // Editor export uses Domain proportions and a zero-based box; callers needing a
        // precise DCC origin supply explicit CloudImportedVolume physical bounds.
        static auto ExportVdb( const filesystem::Path &path, const CloudImportedVolume &volume ) -> void;
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CLOUDS_CLOUD_VOLUME_IO_HH
