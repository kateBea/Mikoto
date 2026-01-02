// Simple compute to calculate prime numbers

#version 460

#include "ShaderBase.glsl"

layout(local_size_x = 64) in;

layout(set = PERPASS_SETINDEX, binding = 0) buffer OutputBuffer {
    uint data[];
};

bool isPrime(uint n) {
    if (n < 2u) return false;
    if (n == 2u) return true;
    if ((n & 1u) == 0u) return false;

    uint limit = uint(floor(sqrt(float(n))));

    for (uint d = 3u; d <= limit; d += 2u) {
        if ((n % d) == 0u) return false;
    }
    return true;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    // guard: avoid out-of-range writes if dispatch > buffer length (but you should dispatch properly)
    // we assume the host dispatches enough groups for the intended range
    if (isPrime(idx)) {
        data[idx] = idx; // prime -> write the number
    } else {
        data[idx] = 0u;   // not prime -> 0
    }
}