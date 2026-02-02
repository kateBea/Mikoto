#version 450

layout(location = 0) in vec3 v_CameraPos;
layout(location = 1) in vec2 v_Coords;

layout(location = 0) out vec4 outColor;

// Grid settings
const float cellSize      = 1.0;
const float subcellSize   = 0.1;

const float halfCell      = cellSize * 0.5;
const float halfSubcell   = subcellSize * 0.5;

const float cellThickness    = 0.005;
const float subcellThickness = 0.001;

const vec4 cellColor    = vec4(0.75, 0.75, 0.75, 0.9);
const vec4 subcellColor = vec4(0.50, 0.50, 0.50, 0.9);

const float heightToFadeRatio = 25.0;
const float minFadeDistance   = 100.0 * 0.05;
const float maxFadeDistance   = 100.0 * 0.5;

void main() {
    // First: compute periodic cell coordinates
    vec2 cellCoords    = mod(v_Coords + halfCell, cellSize);
    vec2 subcellCoords = mod(v_Coords + halfSubcell, subcellSize);

    // Distance to nearest line
    vec2 distCell    = abs(cellCoords    - halfCell);
    vec2 distSubcell = abs(subcellCoords - halfSubcell);

    // Stabilize line thickness using fwidth
    vec2 d = fwidth(v_Coords);
    float adjCellThickness    = 0.5 * (cellThickness    + max(d.x, d.y));
    float adjSubcellThickness = 0.5 * (subcellThickness + max(d.x, d.y));

    vec4 color = vec4(0.0);

    if (any(lessThan(distSubcell, vec2(adjSubcellThickness))))
        color = subcellColor;

    if (any(lessThan(distCell, vec2(adjCellThickness))))
        color = cellColor;

    // Fade near the camera to hide seams
    float distToCam = length(v_Coords - v_CameraPos.xz);

    float fadeDist = abs(v_CameraPos.y) * heightToFadeRatio;
    fadeDist = clamp(fadeDist, minFadeDistance, maxFadeDistance);

    float opacity = smoothstep(1.0, 0.0, distToCam / fadeDist);

    outColor = color * opacity;
}
