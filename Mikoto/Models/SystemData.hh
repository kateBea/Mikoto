//
// Created by kate on 1/3/25.
//

#ifndef SYSTEMDATA_HH
#define SYSTEMDATA_HH

namespace Mikoto {
    struct SystemInfo {
        Int64_T TotalRam;  // Total usable main memory size in kB
        Int64_T FreeRam;   // Available memory size in kB
        Int64_T SharedRam; // Amount of shared memory in kB
    };
}
#endif //SYSTEMDATA_HH
