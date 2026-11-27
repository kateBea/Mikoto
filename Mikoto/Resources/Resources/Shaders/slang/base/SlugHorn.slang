// Slang/RHI adaptation of SlugHorn example/slughorn-example-glfw.cpp.
// Pinned upstream: 7dcd7f1d101a6f9fbfb7a79808abd04b1fb09ca5
// https://github.com/AlphaPixel/slughorn
// MIT License
// 
// Copyright (c) 2026 AlphaPixel LLC
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Analytic glyph coverage from banded quadratic curves. Horizontal and vertical winding
// estimates are combined to antialias the glyph boundary without a distance-field atlas.

#pragma once

// Matches the pinned default Atlas width and indirection-table size.
#define TEX_WIDTH 9
#define SLUG_INDIRECTION_SIZE 32

uint slug_CalcRootCode(float y1, float y2, float y3) {
	uint i1 = asuint(y1) >> 31u;
	uint i2 = asuint(y2) >> 30u;
	uint i3 = asuint(y3) >> 29u;

	uint shift = (i2 & 2u) | (i1 & ~2u);
	shift = (i3 & 4u) | (shift & ~4u);

	return ((0x2E74u >> shift) & 0x0101u);
}

float2 slug_SolveHorizPoly(float4 p12, float2 p3) {
	float2 a = p12.xy - p12.zw * 2.0 + p3;
	float2 b = p12.xy - p12.zw;
	float ra = 1.0 / a.y;
	float rb = 0.5 / b.y;
	float d = sqrt(max(b.y * b.y - a.y * p12.y, 0.0));
	float t1 = (b.y - d) * ra;
	float t2 = (b.y + d) * ra;
	if(abs(a.y) < 1.0 / 65536.0) {
     t1 = p12.y * rb;
     t2 = t1;
 }
	return float2(
		(a.x * t1 - b.x * 2.0) * t1 + p12.x,
		(a.x * t2 - b.x * 2.0) * t2 + p12.x
	);
}

float2 slug_SolveVertPoly(float4 p12, float2 p3) {
	float2 a = p12.xy - p12.zw * 2.0 + p3;
	float2 b = p12.xy - p12.zw;
	float ra = 1.0 / a.x;
	float rb = 0.5 / b.x;
	float d = sqrt(max(b.x * b.x - a.x * p12.x, 0.0));
	float t1 = (b.x - d) * ra;
	float t2 = (b.x + d) * ra;
	if(abs(a.x) < 1.0 / 65536.0) {
     t1 = p12.x * rb;
     t2 = t1;
 }
	return float2(
		(a.y * t1 - b.y * 2.0) * t1 + p12.y,
		(a.y * t2 - b.y * 2.0) * t2 + p12.y
	);
}

int2 slug_CalcBandLoc(int2 glyphLoc, uint offset) {
	int2 bandLoc = int2(glyphLoc.x + int(offset), glyphLoc.y);
	bandLoc.y += bandLoc.x >> TEX_WIDTH;
	bandLoc.x &= (1 << TEX_WIDTH) - 1;
	return bandLoc;
}

// 2026-08-30: re-derive a curve's second texel location the same way slug_CalcBandLoc()
// re-derives a band fetch's row, instead of flat-adding to curveLoc.x. Packing today always
// aligns a curve's 2 texels to start at an even X so they never straddle a row, making this a
// no-op -- but endpoint-shared curve packing (planned) removes that alignment, so a curve's
// second texel CAN legitimately land at the start of the next row.
int2 slug_CalcCurveLoc(int2 curveLoc, int offset) {
	int2 loc = int2(curveLoc.x + offset, curveLoc.y);
	loc.y += loc.x >> TEX_WIDTH;
	loc.x &= (1 << TEX_WIDTH) - 1;
	return loc;
}

float slug_CalcCoverage(float xcov, float ycov, float xwgt, float ywgt) {
	float coverage = max(
		abs(xcov * xwgt + ycov * ywgt) / max(xwgt + ywgt, 1.0 / 65536.0),
		min(abs(xcov), abs(ycov))
	);
	return clamp(coverage, 0.0, 1.0);
}

float SlugHornRender(Texture2D curveTexture, Texture2D<uint4> bandTexture, float2 renderCoord, float4 bandTransform, int2 glyphLoc, int2 bandMax) {
	float2 emsPerPixel = max(fwidth(renderCoord), float2(1e-7));
	float2 pixelsPerEm = 1.0 / emsPerPixel;

	// O(1) band index via indirection tables (2 fetches per axis).
	int qY = clamp(int(renderCoord.y * bandTransform.y + bandTransform.w), 0, SLUG_INDIRECTION_SIZE - 1);
	int qX = clamp(int(renderCoord.x * bandTransform.x + bandTransform.z), 0, SLUG_INDIRECTION_SIZE - 1);
	int bandY = int(bandTexture.Load(int3(int2(glyphLoc.x + qY, glyphLoc.y), 0)).r);
	int bandX = int(bandTexture.Load(int3(int2(glyphLoc.x + SLUG_INDIRECTION_SIZE + qX, glyphLoc.y), 0)).r);

	// Horizontal bands -- headers at glyphLoc + 2*IS + bandY
	float xcov = 0.0;
	float xwgt = 0.0;
	uint2 hbandData = bandTexture.Load(int3(int2(glyphLoc.x + 2 * SLUG_INDIRECTION_SIZE + bandY, glyphLoc.y), 0)).xy;

	for(int ci = 0; ci < int(hbandData.x); ci++) {
		// Re-derive this fetch's row every iteration (instead of computing hbandLoc once
		// and flat-adding ci to its X) so a band's curve-index list can span more than one
		// texture row -- removes the old "count > texWidth" hard limit on shape complexity.
		// Costs up to ~10us on complex scenes vs. the single-fetch version; worth it.
		int2 hbandLoc = slug_CalcBandLoc(glyphLoc, hbandData.y + uint(ci));
		int2 curveLoc = int2(bandTexture.Load(int3(hbandLoc, 0)).xy);
		float4 p12 = curveTexture.Load(int3(curveLoc, 0)) - float4(renderCoord, renderCoord);

		// 2026-08-31: same re-derive-per-fetch pattern as slug_CalcBandLoc above, now applied to
		// a curve's own second texel -- needed once packTextures() (slughorn.cpp) started
		// sharing texels between connected curves (Lengyel's Slug convention: a chain of N
		// connected curves costs N+1 texels instead of 2N), which packs densely with no
		// per-curve row alignment. Adds a handful of ALU ops per curve iteration; negligible
		// next to the texture fetch and sqrt-based polynomial solve already in this loop.
		// Shipped as the default (no opt-in flag) -- lossless, and verified bit-identical
		// against the pre-sharing renderer at real CJK-atlas scale.
		float2 p3 = curveTexture.Load(int3(slug_CalcCurveLoc(curveLoc, 1), 0)).xy - renderCoord;

		if(max(max(p12.x, p12.z), p3.x) * pixelsPerEm.x < -0.5) break;

		uint code = slug_CalcRootCode(p12.y, p12.w, p3.y);
		if(code != 0u) {
			float2 r = slug_SolveHorizPoly(p12, p3) * pixelsPerEm.x;
			if((code & 1u) != 0u) {
       xcov += clamp(r.x + 0.5, 0.0, 1.0);
       xwgt = max(xwgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
   }
			if(code > 1u) {
       xcov -= clamp(r.y + 0.5, 0.0, 1.0);
       xwgt = max(xwgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
   }
		}
	}

	// Vertical bands -- headers at glyphLoc + 2*IS + numHBands + bandX
	float ycov = 0.0;
	float ywgt = 0.0;
	uint2 vbandData = bandTexture.Load(int3(int2(glyphLoc.x + 2 * SLUG_INDIRECTION_SIZE + bandMax.y + 1 + bandX, glyphLoc.y), 0)).xy;

	for(int ci = 0; ci < int(vbandData.x); ci++) {
		// See the matching comment in the horizontal-band loop above.
		int2 vbandLoc = slug_CalcBandLoc(glyphLoc, vbandData.y + uint(ci));
		int2 curveLoc = int2(bandTexture.Load(int3(vbandLoc, 0)).xy);
		float4 p12 = curveTexture.Load(int3(curveLoc, 0)) - float4(renderCoord, renderCoord);

		// See the matching endpoint-sharing tradeoff comment in the horizontal-band loop above.
		float2 p3 = curveTexture.Load(int3(slug_CalcCurveLoc(curveLoc, 1), 0)).xy - renderCoord;

		if(max(max(p12.y, p12.w), p3.y) * pixelsPerEm.y < -0.5) break;

		uint code = slug_CalcRootCode(p12.x, p12.z, p3.x);
		if(code != 0u) {
			float2 r = slug_SolveVertPoly(p12, p3) * pixelsPerEm.y;
			if((code & 1u) != 0u) {
       ycov -= clamp(r.x + 0.5, 0.0, 1.0);
       ywgt = max(ywgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
   }
			if(code > 1u) {
       ycov += clamp(r.y + 0.5, 0.0, 1.0);
       ywgt = max(ywgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
   }
		}
	}

	return slug_CalcCoverage(xcov, ycov, xwgt, ywgt);
}


#undef TEX_WIDTH
#undef SLUG_INDIRECTION_SIZE
