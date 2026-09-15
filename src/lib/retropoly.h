//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROPOLY_H_
#define _RETROPOLY_H_

#include "retropalette.h"
#include "retrovector.h"

// A corner of a polygon as the drawers take it: where it landed on screen, and
// what they interpolate across the face from there. Not a Vertex - the
// projection is already done, so there is no model space here and no z, only
// the reciprocal depth that perspective correction needs.
struct PolygonPoint {
	vec2 pos;				// Screen coordinates
	float c;				// Palette index, a float because the Gouraud drawers interpolate it
	vec2 uv;				// Texture UV coordinates
	float q;				// Reciprocal projection depth
	vec3 n;					// Normal, in view space; every drawer renormalizes, so any scale will do
};

// The surface directions +u and +v run in, in view space. A bump map is a
// height field over (u, v), so its gradient tilts the normal along these and
// not along the screen axes.
struct TangentFrame {
	vec3 t, b;
};

// What RETRO_DrawPhongPolygon shades a face with: where the light is and
// which ramp to land on. The light arrives rotated and unit, so unlike a
// UnitVector, which also carries the model-space form it was rotated from,
// there is nothing here but the view-space values.
struct PhongLight {
	vec3 dir;				// Light direction, in view space, unit length
	int c, shades;			// Ramp base, and entries in it: c + shades is one past its last
};

// A shade table and the shape it was read at. The lookup is
// table[color * shades + shade], which is what RETRO_CreateShadeTable and
// RETRO_CreatePaletteShadeTable both write. The two dimensions travel with the
// pointer because they are not the same for every texture: one drawn from a
// palette built for shading has few colors and a long ramp, one that is a
// picture in its own palette has all of them and a short ramp, and a table read
// at the wrong shape is read at the wrong stride.
struct ShadeTable {
	unsigned char *table;	// Rows of shades, one row per texture color
	int colors;				// Texture colors the table has a row for
	int shades;				// Entries in each row
};

// One scanline's horizontal extent, in subpixel x. Both ends lie on the
// triangle, unlike the half-open xstart, xend the drawers derive from them.
struct TriangleSpan {
	float left, right;
};

// The pixels a drawer may write, half-open [x0, x1) by [y0, y1). Full
// screen unless a caller such as RETRO_RenderModel passes a tighter one so
// different regions can have their own renderer.
struct ClipRect {
	int x0 = 0;
	int x1 = RETRO_WIDTH;
	int y0 = 0;
	int y1 = RETRO_HEIGHT;
};

//
// Depth buffer, in q = 1 / depth
//
// The drawers already carry q per pixel for perspective-correct texturing,
// and larger q is nearer, so the same number resolves depth. Cleared to 0,
// infinitely far, once per model.
//
inline float RETRO_DepthBuffer[RETRO_WIDTH * RETRO_HEIGHT];

inline void RETRO_ClearDepthBuffer(void)
{
	memset(RETRO_DepthBuffer, 0, sizeof(RETRO_DepthBuffer));
}

// True, and claims the pixel, when q is nearer than what is there.
inline bool RETRO_DepthTest(int offset, float q)
{
	if (q <= RETRO_DepthBuffer[offset]) return false;
	RETRO_DepthBuffer[offset] = q;
	return true;
}

//
// Environment-map lookup from a view-space normal.
//
// A lighting map (CreatePhongMap) is the front disk of a sphere, sampled over
// a disk of the given radius, in texels, about the map's middle:
//
//   u = W/2 + radius * Nx
//   v = H/2 + radius * Ny
//
// The unlit hemisphere (Nz > 0) is folded onto the dark rim. A radius of W/2
// sweeps the whole of the map's own disk; less stops short of its rim.
//
// A photographic reflection map is Blinn/Newell sphere-mapping of
// R = 2(N·V)N - V with V = (0, 0, -1). For a unit N that identity
// simplifies to a hemisphere-aware scale of Nxy, with no second sqrt:
//
//   u = W (1/2 - sign(Nz) * Nx / 2)
//   v = H (1/2 - sign(Nz) * Ny / 2)
//
// The radius is unused on that path: the sphere map already covers the image.
//
inline void RETRO_GetEnvMapCoordinates(vec3 n, bool lightingmap, int envmapwidth, int envmapheight, int envmapradius, float &u, float &v)
{
	const float epsilon = 1.0e-12f;

	if (lightingmap) {
		vec2 radial;
		if (n.z > 0.0f) {
			vec2 r = { n.x, n.y };
			float radiallengthsquared = dot(r, r);
			radial = radiallengthsquared > epsilon ? r * (1.0f / sqrt(radiallengthsquared)) : vec2{ 1.0f, 0.0f };
		} else {
			vec3 normalized = normalize(n);
			radial = { normalized.x, normalized.y };
		}

		u = envmapwidth / 2.0f + envmapradius * radial.x;
		v = envmapheight / 2.0f + envmapradius * radial.y;
		return;
	}

	n = normalize(n);

	// -sign(Nz)/2, which is where the minus of the formula above went. Nz < 0 is
	// the front, toward the camera, so flipping the sign with it folds the back
	// hemisphere onto the same disk, mirrored.
	float signedhalf = n.z < 0.0f ? 0.5f : -0.5f;
	u = envmapwidth * (0.5f + signedhalf * n.x);
	v = envmapheight * (0.5f + signedhalf * n.y);
}

//
// Horizontal coverage of a triangle at pixel centres (x+1/2, y+1/2).
//
// The value it returns,
//
//   D = (p1x - p0x)(p2y - p0y) - (p1y - p0y)(p2x - p0x)
//
// is the determinant of the triangle's two edge vectors, and every screen-space
// gradient the drawers take is Cramer's rule over it: for a value F known at
// the three corners,
//
//   dF/dx = ((F1-F0)(p2y-p0y) - (F2-F0)(p1y-p0y)) / D
//
// Zero means the two edges are parallel, so there is no interior to fill and no
// gradient to solve for, which is what the callers test before they solve
// anything. D is also twice the triangle's signed area, but nothing here wants
// it as one: the factor of two cancels against the numerator, and so does the
// sign, leaving the same gradient whichever way round the corners are listed.
// Edges are half-open in y so a shared edge is drawn by exactly one triangle.
// clip restricts the rows to a horizontal band; the full screen is the
// default, and RETRO_RenderModel passes a tighter band when a demo gives
// different rows their own renderer.
//
inline float RETRO_ScanTriangle(const PolygonPoint *p0, const PolygonPoint *p1, const PolygonPoint *p2, TriangleSpan *span, int &ystart, int &yend, ClipRect clip = {})
{
	const float epsilon = 1.0e-12f;
	float determinant = cross(p1->pos - p0->pos, p2->pos - p0->pos);
	if (fabs(determinant) <= epsilon) return 0.0f;

	// MIN/MAX are statement-expression macros: nesting one inside another
	// declares an inner temporary that shadows the outer one, so the
	// three-way reduction is chained as separate calls instead
	float ymin = MIN(p0->pos.y, p1->pos.y);
	ymin = MIN(ymin, p2->pos.y);
	float ymax = MAX(p0->pos.y, p1->pos.y);
	ymax = MAX(ymax, p2->pos.y);

	ystart = MAX((int)ceil(ymin - 0.5f), clip.y0);
	yend = MIN((int)ceil(ymax - 0.5f), clip.y1);
	if (ystart >= yend) return 0.0f;

	for (int y = ystart; y < yend; y++) {
		span[y].left = RETRO_WIDTH;
		span[y].right = 0;
	}

	const PolygonPoint *edgevertex[] = { p0, p1, p2, p0 };
	for (int edge = 0; edge < 3; edge++) {
		const PolygonPoint *a = edgevertex[edge];
		const PolygonPoint *b = edgevertex[edge + 1];
		if (b->pos.y < a->pos.y) SWAP(a, b);

		float ydiff = b->pos.y - a->pos.y;
		if (ydiff == 0.0f) continue;

		float dxdy = (b->pos.x - a->pos.x) / ydiff;
		// Include scanlines whose pixel center lies within the half-open edge.
		int edgeystart = MAX((int)ceil(a->pos.y - 0.5f), ystart);
		int edgeyend = MIN((int)ceil(b->pos.y - 0.5f), yend);
		float x = a->pos.x + ((edgeystart + 0.5f) - a->pos.y) * dxdy;

		for (int y = edgeystart; y < edgeyend; y++, x += dxdy) {
			span[y].left = MIN(span[y].left, x);
			span[y].right = MAX(span[y].right, x);
		}
	}

	return determinant;
}

//
// Flat shaded polygon
// Split a convex polygon into a triangle fan and fill it with one color.
//
inline void RETRO_DrawFlatPolygon(PolygonPoint *point, int points, unsigned char color, ClipRect clip = {})
{
	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = color;
				}
				q += dqdx;
			}
		}
	}
}

//
// Glenz shaded polygon
// Add one color to the framebuffer, allowing sorted polygons to show through.
// colormax is the top of the add; a model opts into a lower one (see
// GlenzLighting::colormax) so a triple overlap fills a chosen shade instead
// of walking into white. RETRO_COLORS - 1 is the unsigned char ceiling.
//
inline void RETRO_DrawGlenzPolygon(PolygonPoint *point, int points, unsigned char color, int colormax, ClipRect clip = {})
{
	colormax = CLAMP(colormax, 0, RETRO_COLORS);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			for (int x = xstart; x < xend; x++) {
				int pixel = RETRO.framebuffer[y * RETRO_WIDTH + x] + color;
				RETRO.framebuffer[y * RETRO_WIDTH + x] = MIN(pixel, colormax);
			}
		}
	}
}

//
// Gouraud shaded polygon
// Interpolate palette indices affinely in screen space to keep shared
// triangle edges continuous.
//
inline void RETRO_DrawGouraudPolygon(PolygonPoint *point, int points, ClipRect clip = {})
{
	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		float dcdx = ((p1->c - p0->c) * (p2->pos.y - p0->pos.y) - (p2->c - p0->c) * (p1->pos.y - p0->pos.y)) / determinant;
		float dcdy = ((p1->pos.x - p0->pos.x) * (p2->c - p0->c) - (p2->pos.x - p0->pos.x) * (p1->c - p0->c)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float c = p0->c + dcdx * (px - p0->pos.x) + dcdy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = CLAMP256(c);
				}
				c += dcdx;
				q += dqdx;
			}
		}
	}
}

//
// Phong shaded polygon
//
// Vertex normals arrive as n * q. Interpolating that product and
// renormalising is the same direction as divide-by-q then renormalise
// (q > 0 in front of the near plane). The pixel is then
//
//   I = ShadeFromLambert(max(N · L, 0))
//   color = c + shades * I
//
inline void RETRO_DrawPhongPolygon(PolygonPoint *point, int points, PhongLight light, ClipRect clip = {})
{
	const float epsilon = 1.0e-12f;

	int cstart = light.c;
	int cend = MIN(light.c + light.shades, RETRO_COLORS);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				float normallengthsquared = dot(n, n);
				float intensity = 0.0f;

				// Interpolated normals must be normalized before lighting.
				if (normallengthsquared > epsilon) {
					float inversenormallength = 1.0f / sqrt(normallengthsquared);
					intensity = MAX(dot(n, light.dir) * inversenormallength, 0.0f);
				}

				float paletteintensity = RETRO_ShadeFromLambert(intensity);
				int color = light.c + light.shades * paletteintensity;
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = CLAMP(color, cstart, cend);
				}
				n += dndx;
				q += dqdx;
			}
		}
	}
}

//
// Texture mapped polygon
//
// Affine interpolation of (u, v) in screen space warps the texture because
// perspective is not linear in x, y. Interpolate the homogeneous pair instead:
//
//   uq = u * q,   vq = v * q,   q = 1 / depth
//   u  = uq / q,  v  = vq / q
//
// uq, vq and q are linear in screen space, so the divide recovers the
// perspective-correct texel.
//
// wrap says what a coordinate outside the map means. A model's are authored
// inside it and only leave by a rounding error at a face's edge, which clamping
// puts back on the nearest texel; wrapping would answer that with the texel
// from the far edge instead. A caller that means the map to tile - one whose
// faces are laid out over a plane wider than the map, and coordinates run off
// it by whole multiples - wants those folded back rather than smeared into the
// edge texel, and says so here.
//
inline void RETRO_DrawTexMapPolygon(PolygonPoint *point, int points, unsigned char *texmap, int texmapwidth, int texmapheight, bool wrap = false, ClipRect clip = {})
{
	if (texmap == NULL) return;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec2 uv0 = p0->uv * p0->q, uv1 = p1->uv * p1->q, uv2 = p2->uv * p2->q;
		vec2 duvdx = ((uv1 - uv0) * (p2->pos.y - p0->pos.y) - (uv2 - uv0) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 duvdy = ((p1->pos.x - p0->pos.x) * (uv2 - uv0) - (p2->pos.x - p0->pos.x) * (uv1 - uv0)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					vec2 texmapcoord = uv * inverseq;
					int u = wrap ? WRAP(texmapcoord.x, texmapwidth) : CLAMP(texmapcoord.x, 0, texmapwidth);
					int v = wrap ? WRAP(texmapcoord.y, texmapheight) : CLAMP(texmapcoord.y, 0, texmapheight);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = texmap[v * texmapwidth + u];
					}
				}
				uv += duvdx;
				q += dqdx;
			}
		}
	}
}

//
// Gouraud shaded texture mapped polygon
//
// Texture coordinates are perspective-correct; shade is affine so adjacent
// triangles agree along their shared edge. wrap is as above.
//
// The shade table arrives with its own shape rather than assumed to have the
// shading-palette one, so a texture that is a picture in its own palette is
// drawn from all of it and not from its first thirty-two entries.
//
inline void RETRO_DrawTexMapGouraudPolygon(PolygonPoint *point, int points, unsigned char *texmap, int texmapwidth, int texmapheight, const ShadeTable &shadetable, bool wrap = false, ClipRect clip = {})
{
	if (texmap == NULL || shadetable.table == NULL) return;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec2 uv0 = p0->uv * p0->q, uv1 = p1->uv * p1->q, uv2 = p2->uv * p2->q;
		vec2 duvdx = ((uv1 - uv0) * (p2->pos.y - p0->pos.y) - (uv2 - uv0) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 duvdy = ((p1->pos.x - p0->pos.x) * (uv2 - uv0) - (p2->pos.x - p0->pos.x) * (uv1 - uv0)) / determinant;
		float dcdx = ((p1->c - p0->c) * (p2->pos.y - p0->pos.y) - (p2->c - p0->c) * (p1->pos.y - p0->pos.y)) / determinant;
		float dcdy = ((p1->pos.x - p0->pos.x) * (p2->c - p0->c) - (p2->pos.x - p0->pos.x) * (p1->c - p0->c)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			float c = p0->c + dcdx * (px - p0->pos.x) + dcdy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					vec2 texmapcoord = uv * inverseq;
					int u = wrap ? WRAP(texmapcoord.x, texmapwidth) : CLAMP(texmapcoord.x, 0, texmapwidth);
					int v = wrap ? WRAP(texmapcoord.y, texmapheight) : CLAMP(texmapcoord.y, 0, texmapheight);
					unsigned char texel = CLAMP(texmap[v * texmapwidth + u], 0, shadetable.colors);
					int shade = CLAMP(c, 0, shadetable.shades);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable.table[texel * shadetable.shades + shade];
					}
				}
				uv += duvdx;
				c += dcdx;
				q += dqdx;
			}
		}
	}
}

//
// Tilt a unit normal by a height-field gradient, in the surface's own frame
//
//   N' = normalize(N + dhx T + dhy B)
//
// T and B are the directions +u and +v run on the surface, rotated with the
// model, so the relief turns with it. The stored frame belongs to the
// geometric face; project it onto the interpolated shading normal here so it
// is also a tangent frame for Gouraud and environment-mapped normals.
//
inline vec3 RETRO_BumpNormal(vec3 n, float dhx, float dhy, const TangentFrame &frame)
{
	if (dhx == 0.0f && dhy == 0.0f) {
		return n;
	}

	const float epsilon = 1.0e-12f;

	// T projected perpendicular to N: t - n*dot(n,t), the Gram-Schmidt step.
	vec3 t = frame.t - n * dot(n, frame.t);
	float tlengthsquared = dot(t, t);
	if (tlengthsquared > epsilon) {
		t = t * (1.0f / sqrt(tlengthsquared));
	} else {
		// If +u is parallel to N, recover it from projected +v. Keep the sign
		// that is closest to the face's original +u direction.
		vec3 b = frame.b - n * dot(n, frame.b);
		float blengthsquared = dot(b, b);
		if (blengthsquared <= epsilon) {
			return n;
		}
		b = b * (1.0f / sqrt(blengthsquared));
		t = cross(b, n);
		if (dot(t, frame.t) < 0.0f) {
			t = -t;
		}
	}

	// N and T are unit and perpendicular, so their cross product is already a
	// unit +v candidate. Choose its sign to retain mirrored UV handedness.
	vec3 b = cross(n, t);
	if (dot(b, frame.b) < 0.0f) {
		b = -b;
	}

	// With T and B perpendicular to N, this sum approaches grazing as the
	// gradient grows but cannot cross to the back of the surface.
	vec3 bumped = n + t * dhx + b * dhy;

	float lengthsquared = dot(bumped, bumped);
	if (lengthsquared <= epsilon) {
		return n;
	}

	return bumped * (1.0f / sqrt(lengthsquared));
}

//
// Bump mapped shaded texture mapped polygon
// Texture coordinates are perspective-correct; the shade is affine, as in
// RETRO_DrawTexMapGouraudPolygon, and the normals the bump is tilted from
// retain the affine interpolation of the environment mappers. Handing every
// point the same shade and normal draws a flat shaded face, one of each per
// point a gouraud shaded one.
//
// lambertshades is how far up the shade table one unit of lambert carries a face,
// which a model sets for itself and which is not the table's own height. The
// bump moves the shade by the difference it makes to the lighting, so it is
// measured in the same steps the face was already shaded in.
inline void RETRO_DrawTexMapBumpPolygon(PolygonPoint *point, int points, unsigned char *texmap, unsigned char *bumpmap, int bumpgrazing, const ShadeTable &shadetable, int lambertshades, vec3 light, const TangentFrame &frame, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, ClipRect clip = {})
{
	if (texmap == NULL || bumpmap == NULL || shadetable.table == NULL) return;

	const float epsilon = 1.0e-12f;

	// The bump map is addressed by the texture's UVs, so a map with a size of its own is
	// stepped through at its own rate, and its gradient divided by the surface distance
	// one of its texels covers. Resampling the map changes its detail, not its depth
	float bumptexelu = (float)bumpmapwidth / texmapwidth;
	float bumptexelv = (float)bumpmapheight / texmapheight;

	// The Sobel weights sum to 4, and bumpgrazing is the height difference that
	// tilts to grazing
	float bumptiltu = bumptexelu / (4 * bumpgrazing);
	float bumptiltv = bumptexelv / (4 * bumpgrazing);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec2 uv0 = p0->uv * p0->q, uv1 = p1->uv * p1->q, uv2 = p2->uv * p2->q;
		vec2 duvdx = ((uv1 - uv0) * (p2->pos.y - p0->pos.y) - (uv2 - uv0) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 duvdy = ((p1->pos.x - p0->pos.x) * (uv2 - uv0) - (p2->pos.x - p0->pos.x) * (uv1 - uv0)) / determinant;
		float dcdx = ((p1->c - p0->c) * (p2->pos.y - p0->pos.y) - (p2->c - p0->c) * (p1->pos.y - p0->pos.y)) / determinant;
		float dcdy = ((p1->pos.x - p0->pos.x) * (p2->c - p0->c) - (p2->pos.x - p0->pos.x) * (p1->c - p0->c)) / determinant;
		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			float c = p0->c + dcdx * (px - p0->pos.x) + dcdy * (py - p0->pos.y);
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uv.x * inverseq;
					float v = uv.y * inverseq;
					unsigned int texmapu = CLAMP(u, 0, texmapwidth);
					unsigned int texmapv = CLAMP(v, 0, texmapheight);
					float bumpu = u * bumptexelu;
					float bumpv = v * bumptexelv;
					unsigned int bumpmapu = CLAMP(bumpu, 0, bumpmapwidth);
					unsigned int bumpmapv = CLAMP(bumpv, 0, bumpmapheight);
					// Sobel: central difference along one axis, averaged over three
					// rows across it. Weights sum to 4, which the /4 divides back out.
					// Bright texels protrude.
					int um = CLAMP(bumpu - 1.0f, 0, bumpmapwidth);
					int up = CLAMP(bumpu + 1.0f, 0, bumpmapwidth);
					int vm = CLAMP(bumpv - 1.0f, 0, bumpmapheight) * bumpmapwidth;
					int vp = CLAMP(bumpv + 1.0f, 0, bumpmapheight) * bumpmapwidth;
					int vc = bumpmapv * bumpmapwidth;
					int gx = (bumpmap[um + vm] + 2 * bumpmap[um + vc] + bumpmap[um + vp]) -
							 (bumpmap[up + vm] + 2 * bumpmap[up + vc] + bumpmap[up + vp]);
					int gy = (bumpmap[um + vm] + 2 * bumpmap[bumpmapu + vm] + bumpmap[up + vm]) -
							 (bumpmap[um + vp] + 2 * bumpmap[bumpmapu + vp] + bumpmap[up + vp]);
					float dhx = gx * bumptiltu;
					float dhy = gy * bumptiltv;
					// Interpolated normals must be normalized before lighting.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? 1.0f / sqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					// The shade moves by as much as the tilt changes the lighting
					// here, so a flat patch of the bump map is left shaded exactly
					// as it was drawn without one.
					float lambert = dot(unitn, light);
					// L and N' are unit, so the term is N' · L.
					float bumpedlambert = dot(RETRO_BumpNormal(unitn, dhx, dhy, frame), light);
					float bumpshade = (RETRO_ShadeFromLambert(bumpedlambert) - RETRO_ShadeFromLambert(lambert)) * lambertshades;
					unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, shadetable.colors);
					int shade = CLAMP(c + bumpshade, 0, shadetable.shades);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable.table[texel * shadetable.shades + shade];
					}
				}
				uv += duvdx;
				c += dcdx;
				n += dndx;
				q += dqdx;
			}
		}
	}
}

//
// Environment shaded texture mapped polygon
// Texture coordinates and lighting normals are perspective-correct;
// reflection normals retain the original affine interpolation.
//
// Texture coordinates are clamped, not wrapped: nothing tiles a map through
// this drawer, and an env map has a rim rather than a seam.
inline void RETRO_DrawTexMapEnvMapPolygon(PolygonPoint *point, int points, unsigned char *texmap, unsigned char *envmap, const ShadeTable &shadetable, unsigned char shade, bool lightingmap, int envmapwidth, int envmapheight, int envmapradius, int texmapwidth, int texmapheight, ClipRect clip = {})
{
	if (texmap == NULL || shadetable.table == NULL) return;

	const float epsilon = 1.0e-12f;
	bool envmapshading = envmap != NULL;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec2 uv0 = p0->uv * p0->q, uv1 = p1->uv * p1->q, uv2 = p2->uv * p2->q;
		vec2 duvdx = ((uv1 - uv0) * (p2->pos.y - p0->pos.y) - (uv2 - uv0) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 duvdy = ((p1->pos.x - p0->pos.x) * (uv2 - uv0) - (p2->pos.x - p0->pos.x) * (uv1 - uv0)) / determinant;
		vec3 dndx = { 0.0f, 0.0f, 0.0f };
		vec3 dndy = { 0.0f, 0.0f, 0.0f };
		if (envmapshading) {
			dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
			dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		}
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			vec3 n = envmapshading ? p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y) : vec3{ 0.0f, 0.0f, 0.0f };
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					unsigned int u = CLAMP(uv.x * inverseq, 0, texmapwidth);
					unsigned int v = CLAMP(uv.y * inverseq, 0, texmapheight);
					unsigned char texel = CLAMP(texmap[v * texmapwidth + u], 0, shadetable.colors);
					unsigned char pixelshade = CLAMP(shade, 0, shadetable.shades);
					if (envmapshading) {
						float e, w;
						RETRO_GetEnvMapCoordinates(n, lightingmap, envmapwidth, envmapheight, envmapradius, e, w);
						unsigned int envmapu = CLAMP(e, 0, envmapwidth);
						unsigned int envmapv = CLAMP(w, 0, envmapheight);
						pixelshade = CLAMP(envmap[envmapv * envmapwidth + envmapu], 0, shadetable.shades);
					}
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable.table[texel * shadetable.shades + pixelshade];
					}
				}
				uv += duvdx;
				if (envmapshading) {
					n += dndx;
				}
				q += dqdx;
			}
		}
	}
}

//
// Bump and environment shaded texture mapped polygon
// Lighting and reflection maps are read at the tilted unit normal N'. The
// bump map is addressed by the texture's UVs, so a map with a size of its
// own is stepped through at its own rate. Resampling the map changes its
// detail, not its depth.
//
// Texture coordinates are clamped, not wrapped: nothing tiles a map through
// this drawer, and an env map has a rim rather than a seam.
inline void RETRO_DrawTexMapEnvMapBumpPolygon(PolygonPoint *point, int points, unsigned char *texmap, unsigned char *envmap, unsigned char *bumpmap, int bumpgrazing, const ShadeTable &shadetable, bool lightingmap, const TangentFrame &frame, int envmapwidth, int envmapheight, int envmapradius, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, ClipRect clip = {})
{
	if (texmap == NULL || envmap == NULL || bumpmap == NULL || shadetable.table == NULL) return;

	const float epsilon = 1.0e-12f;

	float bumptexelu = (float)bumpmapwidth / texmapwidth;
	float bumptexelv = (float)bumpmapheight / texmapheight;

	// The Sobel weights sum to 4, and bumpgrazing is the height difference that
	// tilts to grazing
	float bumptiltu = bumptexelu / (4 * bumpgrazing);
	float bumptiltv = bumptexelv / (4 * bumpgrazing);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec2 uv0 = p0->uv * p0->q, uv1 = p1->uv * p1->q, uv2 = p2->uv * p2->q;
		vec2 duvdx = ((uv1 - uv0) * (p2->pos.y - p0->pos.y) - (uv2 - uv0) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 duvdy = ((p1->pos.x - p0->pos.x) * (uv2 - uv0) - (p2->pos.x - p0->pos.x) * (uv1 - uv0)) / determinant;
		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uv.x * inverseq;
					float v = uv.y * inverseq;
					unsigned int texmapu = CLAMP(u, 0, texmapwidth);
					unsigned int texmapv = CLAMP(v, 0, texmapheight);
					float bumpu = u * bumptexelu;
					float bumpv = v * bumptexelv;
					unsigned int bumpmapu = CLAMP(bumpu, 0, bumpmapwidth);
					unsigned int bumpmapv = CLAMP(bumpv, 0, bumpmapheight);
					// Sobel: central difference along one axis, averaged over three
					// rows across it. Weights sum to 4, which the /4 divides back out.
					// Bright texels protrude.
					int um = CLAMP(bumpu - 1.0f, 0, bumpmapwidth);
					int up = CLAMP(bumpu + 1.0f, 0, bumpmapwidth);
					int vm = CLAMP(bumpv - 1.0f, 0, bumpmapheight) * bumpmapwidth;
					int vp = CLAMP(bumpv + 1.0f, 0, bumpmapheight) * bumpmapwidth;
					int vc = bumpmapv * bumpmapwidth;
					int gx = (bumpmap[um + vm] + 2 * bumpmap[um + vc] + bumpmap[um + vp]) -
							 (bumpmap[up + vm] + 2 * bumpmap[up + vc] + bumpmap[up + vp]);
					int gy = (bumpmap[um + vm] + 2 * bumpmap[bumpmapu + vm] + bumpmap[up + vm]) -
							 (bumpmap[um + vp] + 2 * bumpmap[bumpmapu + vp] + bumpmap[up + vp]);
					float e, w;
					// Both maps are functions of the unit normal, so both tilt N and
					// look the result up. A unit N' always lands inside either map.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? 1.0f / sqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					vec3 bumpednormal = RETRO_BumpNormal(unitn, gx * bumptiltu, gy * bumptiltv, frame);
					RETRO_GetEnvMapCoordinates(bumpednormal, lightingmap, envmapwidth, envmapheight, envmapradius, e, w);
					unsigned int envmapu = CLAMP(e, 0, envmapwidth);
					unsigned int envmapv = CLAMP(w, 0, envmapheight);
					unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, shadetable.colors);
					unsigned char pixelshade = CLAMP(envmap[envmapv * envmapwidth + envmapu], 0, shadetable.shades);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable.table[texel * shadetable.shades + pixelshade];
					}
				}
				uv += duvdx;
				n += dndx;
				q += dqdx;
			}
		}
	}
}

//
// Environment mapped polygon
// Lighting normals are perspective-correct and normalized at lookup;
// reflection normals retain the original affine interpolation.
//
inline void RETRO_DrawEnvMapPolygon(PolygonPoint *point, int points, unsigned char *envmap, bool lightingmap, int envmapwidth, int envmapheight, int envmapradius, ClipRect clip = {})
{
	if (envmap == NULL) return;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				float e, w;
				RETRO_GetEnvMapCoordinates(n, lightingmap, envmapwidth, envmapheight, envmapradius, e, w);
				unsigned int envmapu = CLAMP(e, 0, envmapwidth);
				unsigned int envmapv = CLAMP(w, 0, envmapheight);
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = envmap[envmapv * envmapwidth + envmapu];
				}
				n += dndx;
				q += dqdx;
			}
		}
	}
}

//
// Bump-mapped environment polygon
// Lighting and reflection maps are read at the tilted unit normal N'.
//
inline void RETRO_DrawEnvMapBumpPolygon(PolygonPoint *point, int points, unsigned char *envmap, unsigned char *bumpmap, int bumpgrazing, bool lightingmap, const TangentFrame &frame, int envmapwidth, int envmapheight, int envmapradius, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, ClipRect clip = {})
{
	if (envmap == NULL || bumpmap == NULL) return;

	const float epsilon = 1.0e-12f;

	float bumptexelu = (float)bumpmapwidth / texmapwidth;
	float bumptexelv = (float)bumpmapheight / texmapheight;

	// The Sobel weights sum to 4, and bumpgrazing is the height difference that
	// tilts to grazing
	float bumptiltu = bumptexelu / (4 * bumpgrazing);
	float bumptiltv = bumptexelv / (4 * bumpgrazing);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec2 uv0 = p0->uv * p0->q, uv1 = p1->uv * p1->q, uv2 = p2->uv * p2->q;
		vec2 duvdx = ((uv1 - uv0) * (p2->pos.y - p0->pos.y) - (uv2 - uv0) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 duvdy = ((p1->pos.x - p0->pos.x) * (uv2 - uv0) - (p2->pos.x - p0->pos.x) * (uv1 - uv0)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;
		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uv.x * inverseq;
					float v = uv.y * inverseq;
					float bumpu = u * bumptexelu;
					float bumpv = v * bumptexelv;
					unsigned int bumpmapu = CLAMP(bumpu, 0, bumpmapwidth);
					unsigned int bumpmapv = CLAMP(bumpv, 0, bumpmapheight);
					// Sobel: central difference along one axis, averaged over three
					// rows across it. Weights sum to 4, which the /4 divides back out.
					// Bright texels protrude.
					int um = CLAMP(bumpu - 1.0f, 0, bumpmapwidth);
					int up = CLAMP(bumpu + 1.0f, 0, bumpmapwidth);
					int vm = CLAMP(bumpv - 1.0f, 0, bumpmapheight) * bumpmapwidth;
					int vp = CLAMP(bumpv + 1.0f, 0, bumpmapheight) * bumpmapwidth;
					int vc = bumpmapv * bumpmapwidth;
					int gx = (bumpmap[um + vm] + 2 * bumpmap[um + vc] + bumpmap[um + vp]) -
							 (bumpmap[up + vm] + 2 * bumpmap[up + vc] + bumpmap[up + vp]);
					int gy = (bumpmap[um + vm] + 2 * bumpmap[bumpmapu + vm] + bumpmap[up + vm]) -
							 (bumpmap[um + vp] + 2 * bumpmap[bumpmapu + vp] + bumpmap[up + vp]);
					float e, w;
					// Both maps are functions of the unit normal, so both tilt N and
					// look the result up. A unit N' always lands inside either map.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? 1.0f / sqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					vec3 bumpednormal = RETRO_BumpNormal(unitn, gx * bumptiltu, gy * bumptiltv, frame);
					RETRO_GetEnvMapCoordinates(bumpednormal, lightingmap, envmapwidth, envmapheight, envmapradius, e, w);
					unsigned int envmapu = CLAMP(e, 0, envmapwidth);
					unsigned int envmapv = CLAMP(w, 0, envmapheight);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = envmap[envmapv * envmapwidth + envmapu];
					}
				}
				uv += duvdx;
				q += dqdx;
				n += dndx;
			}
		}
	}
}

//
// A sprite, depth tested against a surface of its own.
//
// The map is flat; what it stands for need not be. A second map gives, per
// texel, how far in front of the sprite's own depth that texel's surface
// sits, as a fraction of thickness, so the pixel is written at
//
//   q' = 1 / (1 / q - thickness * depthmap)
//
// and not at the depth the sprite was placed at. Two sprites then resolve on
// the curve where the surfaces they stand for meet, and that curve moves as
// they move. Ordering whole sprites by their placed depth instead hands the
// entire overlap to whichever is nearer, so the moment two of them cross it
// flips over in a single frame.
//
// A ball is the obvious case: RETRO_CreateBallMap fills the depth map with
// the front hemisphere and thickness is the ball's radius. Anything else with
// a front surface works the same way, and a depth map of zeroes is a flat
// sprite standing square on at the depth it was given.
//
// thickness is in the units depth is measured in, which is a length in the
// model times whatever scale the projection was given. Transparency is the
// alpha entry, as in RETRO_DrawSprite, not the shape of the map.
//
inline void RETRO_DrawDepthSprite(vec2 spos, float q, float size, float thickness, unsigned char *map, float *depthmap, int mapsize, unsigned char alpha = 0, unsigned char *buffer = RETRO.framebuffer)
{
	if (q <= 0.0f || size <= 0.0f) return;

	float half = size / 2;
	int xstart = MAX((int)ceil(spos.x - half - 0.5f), 0);
	int xend = MIN((int)ceil(spos.x + half - 0.5f), RETRO_WIDTH);
	int ystart = MAX((int)ceil(spos.y - half - 0.5f), 0);
	int yend = MIN((int)ceil(spos.y + half - 0.5f), RETRO_HEIGHT);
	float spritedepth = 1.0f / q;

	for (int y = ystart; y < yend; y++) {
		int v = CLAMP((int)(((y + 0.5f) - spos.y + half) * mapsize / size), 0, mapsize);
		for (int x = xstart; x < xend; x++) {
			int u = CLAMP((int)(((x + 0.5f) - spos.x + half) * mapsize / size), 0, mapsize);

			unsigned char color = map[v * mapsize + u];
			if (color == alpha) continue;

			// The surface the texel stands for, not the depth the sprite was placed at
			float pixeldepth = spritedepth - thickness * depthmap[v * mapsize + u];
			if (pixeldepth <= 1.0f) continue;

			int offset = y * RETRO_WIDTH + x;
			if (RETRO_DepthTest(offset, 1.0f / pixeldepth)) {
				buffer[offset] = color;
			}
		}
	}
}

#endif
