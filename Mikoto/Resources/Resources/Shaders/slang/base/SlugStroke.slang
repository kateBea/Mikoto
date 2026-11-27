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

// Mikoto stroke extension, independent of the upstream Slug fill implementation.
// References: https://github.com/EricLengyel/Slug (curve/band representation)
// https://pomax.github.io/bezierinfo/#projections (closest-point formulation).

// Compute analytic stroke coverage from the nearest quadratic-curve distance. Polynomial/root
// helpers locate curve extrema and candidates for the closest point.

#pragma once

// Curve-based outline, evaluated per fragment (no MSDF/MTSDF sampling):
//
// 1. Search the glyph's stroke bands intersecting the fragment's outline radius
//    plus its antialiasing footprint. These bands include horizontal edges that
//    the fill algorithm omits when computing horizontal-ray winding numbers.
// 2. Reject curves whose control-point bounding boxes are farther away than the
//    best distance found so far. The boxes conservatively enclose their curves.
// 3. Find the closest point on each remaining quadratic Bezier:
//        B(t) = (1-t)^2 P0 + 2t(1-t) P1 + t^2 P2,  0 <= t <= 1.
//    A minimum of squared distance to fragment p is at an endpoint or satisfies
//        dot(B(t) - p, B'(t)) = 0.
//    This is a cubic. Split it into monotone intervals at its derivative roots,
//    bisect intervals containing a root, and compare candidates and endpoints.
//    SlugNearestCurve uses sample-relative points, so p is already subtracted.
// 4. Compare the smallest distance with the outline radius in em units. Project
//    screen-space coordinate derivatives onto the nearest-point direction to
//    estimate an antialiasing width, then produce smooth stroke coverage.
// 5. SlugText_Frag.slang combines this coverage with Slug's analytic fill using
//    max(fill, stroke), then CompositeTextOutline places the border behind the
//    fill. Distance offsets produce rounded joins. SlugText_Vert.slang expands
//    the glyph quad by the radius and an antialiasing margin to avoid clipping.
//
// Band filtering and bounding-box rejection reduce curve evaluations, but this
// still costs more arithmetic than an atlas lookup. The distance is evaluated
// numerically from vector curves; it is independent of distance-field resolution.

float SlugStrokePolynomial( float4 c, float t ) {
    return ((c.x * t + c.y) * t + c.z) * t + c.w;
}

// Sample-relative quadratic B(t)=a*t*t+b*t+c. Extrema of squared distance
// satisfy dot(B,B')=0 (a cubic). Partition at the cubic's derivative roots,
// then bisect each monotone interval so multiple minima cannot be missed.
// Fixed refinement budget for a nearest-curve root.
static const uint kSlugStrokeRootIterations = 20;

float2 SlugNearestCurve( float2 p0, float2 p1, float2 p2 ) {
    float2 a = p0 - 2 * p1 + p2;
    float2 b = 2 * (p1 - p0);
    float2 c = p0;
    float2 nearest = dot(p0,p0) < dot(p2,p2) ? p0 : p2;
    float4 poly = float4(2*dot(a,a), 3*dot(a,b), dot(b,b)+2*dot(a,c), dot(b,c));
    // Exact linear/point cases avoid dividing by a zero leading coefficient.
    if ( poly.x == 0 ) {
        float t = dot(b,b) > 0 ? saturate(-dot(b,c)/dot(b,b)) : 0;
        return c + b*t;
    }
    float cuts[4];
    cuts[0] = 0;
    uint count = 1;
    float A = 3*poly.x;
    float B = 2*poly.y;
    float C = poly.z;
    float discriminant = B*B - 4*A*C;
    if ( discriminant > 0 ) {
        // Stable quadratic formula avoids cancellation for nearly linear curves.
        float q = -0.5 * (B + (B >= 0 ? 1 : -1) * sqrt(discriminant));
        float r0 = q/A;
        float r1 = q != 0 ? C/q : r0;
        float lo = min(r0,r1);
        float hi = max(r0,r1);
        if ( lo > 0 && lo < 1 ) cuts[count++] = lo;
        if ( hi > 0 && hi < 1 ) cuts[count++] = hi;
    }
    cuts[count++] = 1;
    for ( uint i = 0; i + 1 < count; ++i ) {
        float lo = cuts[i];
        float hi = cuts[i+1];
        // Include partition boundaries, including repeated stationary roots.
        float2 point = (a*lo+b)*lo+c;
        if ( dot(point,point) < dot(nearest,nearest) ) nearest = point;
        float fLo = SlugStrokePolynomial(poly,lo);
        float fHi = SlugStrokePolynomial(poly,hi);
        if ( (fLo < 0) == (fHi < 0) ) continue;
        for ( uint iteration = 0; iteration < kSlugStrokeRootIterations; ++iteration ) {
            float mid = (lo+hi)*0.5;
            float fMid = SlugStrokePolynomial(poly,mid);
            if ( (fMid < 0) == (fLo < 0) ) {
                lo = mid;
                fLo = fMid;
            }
            else hi = mid;
        }
        float t = (lo+hi)*0.5;
        point = (a*t+b)*t+c;
        if ( dot(point,point) < dot(nearest,nearest) ) nearest = point;
    }
    return nearest;
}

float SlugStrokeCoverage( Texture2D curves, Texture2D<uint4> bands,
                          float2 position, float4 transform, int4 glyph, float radius ) {
    // Compute derivatives before entering pixel-dependent curve-search branches.
    float2 dx = ddx(position);
    float2 dy = ddy(position);
    float footprint = max(length(abs(dx)+abs(dy)), 1e-7);
    float reach = radius + footprint;
    int bandMax = glyph.w & 0xff;
    int first = clamp(int(floor((position.y-reach)*transform.y+transform.w)),0,bandMax);
    int last = clamp(int(floor((position.y+reach)*transform.y+transform.w)),0,bandMax);
    float2 nearest = float2(reach,0);
    for ( int band = first; band <= last; ++band ) {
        uint2 header = bands.Load(int3(glyph.x + 2*(bandMax+1) + band, glyph.y, 0)).xy;
        int2 list = CalcBandLoc(glyph.xy,header.y);
        for ( uint i = 0; i < header.x; ++i ) {
            int2 address = int2(bands.Load(int3(list.x+int(i),list.y,0)).xy);
            float4 p01 = curves.Load(int3(address,0));
            float2 p2 = curves.Load(int3(address+int2(1,0),0)).xy;
            float2 lower = min(min(p01.xy,p01.zw),p2)-position;
            float2 upper = max(max(p01.xy,p01.zw),p2)-position;
            // Control-point AABB encloses the entire curve. Lists descend in max X.
            if ( upper.x < -reach ) break;
            float2 boxDistance = max(max(lower,-upper),float2(0));
            if ( dot(boxDistance,boxDistance) > dot(nearest,nearest) ) continue;
            float2 candidate = SlugNearestCurve(p01.xy-position,p01.zw-position,p2-position);
            if ( dot(candidate,candidate) < dot(nearest,nearest) ) nearest = candidate;
        }
    }
    float distance = length(nearest);
    float2 normal = distance > 1e-7 ? nearest/distance : float2(1,0);
    float aa = max(abs(dot(normal,dx))+abs(dot(normal,dy)),1e-7);
    return saturate(0.5 + (radius-distance)/aa);
}
