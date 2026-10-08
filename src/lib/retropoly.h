//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROPOLY_H_
#define _RETROPOLY_H_

#include "retropalette.h"
#include "retroshadetable.h"
#include "retrovector.h"

// A corner of a polygon as the drawers take it: where it landed on screen, and
// what they interpolate across the face from there. Not a Vertex - the
// projection is already done, so there is no model space here and no z, only
// the reciprocal depth that perspective correction needs.
struct PolygonPoint {
	vec2 pos;				// Screen coordinates
	float c;				// Palette index, a float because the Gouraud drawers interpolate it
	vec2 uv;				// Texture UV coordinates; the texture drawers multiply by q themselves, the shader drawer takes them times q
	float q;				// Reciprocal projection depth
	vec3 n;					// Normal, in view space; every drawer renormalizes, so any scale will do
	vec3 p;					// In view space, times q: the surface point for the shader drawer, the view ray to it for the environment drawer
	float tint[RETRO_MAX_TINTS];	// Further light levels beside c, read with a shade table that has tints
	vec2 lightuv;			// Light map coordinates
};

// One pixel of a surface, as a shader is handed it
struct Fragment {
	int x, y;				// Screen pixel
	float q;				// Reciprocal projection depth, as the depth buffer holds it
	vec3 position;			// Surface point, in view space
	vec3 normal;			// Unit normal, in view space, turned to the side the viewer sees
	vec3 view;				// From the eye to position; its length is the eye's distance
	vec2 uv;				// Texture coordinates in texels, as the model holds them, or zero without any
	vec2 lightuv;			// Light map coordinates, or zero without any
	const void *data;		// What the caller handed the drawer for its shader, or NULL
};

// The surface directions +u and +v run in, in view space. A bump map is a
// height field over (u, v), so its gradient tilts the normal along these and
// not along the screen axes.
struct TangentFrame {
	vec3 t, b;
};

// A light far enough away to arrive from one direction everywhere, like the
// sun, or a point light that shines from where it stands and dims with
// distance, d away as
//
//   strength / (constant + linear d)
//
// A constant of 1 leaves it at full strength where it stands; 0 is the plain
// inverse distance, which grows without bound close in. The renderers take
// the lights in view space, the space the rotated vertices and normals are
// in, where they stay put while the model turns; a demo that lights by hand
// can keep a rig in any space its points and normals share.
struct RETRO_Light {
	vec3 direction;		// Toward a directional light, unit length
	float strength;		// 1 lights a surface facing it fully
	bool point;			// A point light at position instead
	vec3 position;		// A point light's
	float constant;		// A point light's falloff: the part that does not grow with distance
	float linear;		// and the part that does, per unit of distance
	vec3 color = { 1, 1, 1 };	// What it shines, red, green and blue from 0 to 1; only a
								// model with a color table shows any but its brightness
	bool on = true;				// Switched off, it stays in the rig but lights nothing
};

#define RETRO_MAX_LIGHTS 4

// The lights a model is shaded by, which several models can share. The
// renderers that add light up - dots, flat, Gouraud, Phong and the texture
// shades - take all of them. Glenz and the bump maps were built for one
// direction and take the first, so a rig they draw has at least one. They
// take a point light as seen from each face's center, Phong at every pixel,
// and the others at each vertex or face. The ambient light reaches every
// surface whichever way it faces, and only the renderers that add light up.
struct RETRO_Lighting {
	int lights;
	RETRO_Light light[RETRO_MAX_LIGHTS];
	float ambient;	// White light on every surface, added to what the lights give
};

// One full-strength light along the view axis from the viewer's side: a
// headlight, which a model is shaded by until it is given lights of its own
inline const RETRO_Lighting RETRO_Headlight = { 1, { { { 0, 0, -1 }, 1 } } };

// A light as it arrives at the rotated point p: its strength there, returned,
// and the unit direction toward it, written to tolight. A directional light is
// the same everywhere. A light switched off, and a point light that p sits
// on, have no direction and arrive with none of their strength
inline float RETRO_LightAt(const RETRO_Light &light, vec3 p, vec3 &tolight)
{
	if (!light.on) {
		tolight = { 0, 0, 0 };
		return 0;
	}
	if (!light.point) {
		tolight = light.direction;
		return light.strength;
	}
	vec3 l = light.position - p;
	float distance = length(l);
	if (distance <= 0) {
		tolight = { 0, 0, 0 };
		return 0;
	}
	tolight = l / distance;
	return light.strength / (light.constant + light.linear * distance);
}

// How much of one light falls on a surface at p whose normal is n: its
// strength there times the cosine of its angle to the normal, and nothing
// when the surface faces away from it
inline float RETRO_LightTerm(const RETRO_Light &light, vec3 p, vec3 n)
{
	vec3 tolight;
	float strength = RETRO_LightAt(light, p, tolight);
	return strength * MAX(dot(n, tolight), 0.0f);
}

// Whether any of the lights switched on is a point light, so that where a
// surface is matters as well as which way it faces
inline bool RETRO_HasPointLight(const RETRO_Lighting &lighting)
{
	for (int i = 0; i < lighting.lights; i++) {
		if (lighting.light[i].on && lighting.light[i].point) return true;
	}
	return false;
}

// How much light falls on a surface at the rotated point p whose rotated
// normal is n, added up over the lights, and the ambient light. A light
// behind the surface adds nothing, rather than taking away what the others
// give.
inline float RETRO_Lambert(const RETRO_Lighting &lighting, vec3 p, vec3 n)
{
	float lambert = 0;
	for (int i = 0; i < lighting.lights; i++) {
		lambert += RETRO_LightTerm(lighting.light[i], p, n);
	}
	return lambert + lighting.ambient;
}

// RETRO_Lambert in color: each light's term in the light's own color, added
// up per channel, and the ambient light in all three
inline vec3 RETRO_ColorLambert(const RETRO_Lighting &lighting, vec3 p, vec3 n)
{
	vec3 lambert = { 0, 0, 0 };
	for (int i = 0; i < lighting.lights; i++) {
		lambert += lighting.light[i].color * RETRO_LightTerm(lighting.light[i], p, n);
	}
	return lambert + lighting.ambient;
}

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
// R = 2(N·V)N - V with V = (0, 0, -1). The map is addressed by the half-way
// vector of R and V, and R + V = 2(N·V)N, so that is sign(N·V) N. For a unit
// front face (Nz < 0) it is N itself, and the lookup is a scale of Nxy with
// no second sqrt:
//
//   u = W (1/2 + Nx / 2)
//   v = H (1/2 + Ny / 2)
//
// Past the silhouette (Nz > 0) the half-way vector is -N, and the formula
// flips Nxy. Every point of the rim is the one ray R = (0, 0, 1), so that
// flip is continuous in the room but not in the map: the rim texels of a
// baked disk differ side to side, and a normal crossing Nz = 0 would jump to
// the opposite one. The same scale of Nxy is used there instead, folding N
// onto its mirror (Nx, Ny, -Nz), so the lookup turns back inward from the rim
// and N = (0, 0, 1) reads the middle. Scaling Nxy out to the unit circle
// would pin every such normal on the horizon, so an upward one would read
// pale sky instead of the zenith and a downward one would read gray instead
// of the checker. The radius is unused on this path: the sphere map covers
// the image.
//
inline void RETRO_GetEnvMapCoordinates(vec3 n, bool lightingmap, int envmapwidth, int envmapheight, int envmapradius, float &u, float &v)
{
	const float epsilon = 1.0e-12f;

	if (lightingmap) {
		vec2 radial;
		if (n.z > 0.0f) {
			vec2 r = { n.x, n.y };
			float radiallengthsquared = dot(r, r);
			radial = radiallengthsquared > epsilon ? r * inversesqrt(radiallengthsquared) : vec2{ 1.0f, 0.0f };
		} else {
			vec3 normalized = normalize(n);
			radial = { normalized.x, normalized.y };
		}

		u = envmapwidth / 2.0f + envmapradius * radial.x;
		v = envmapheight / 2.0f + envmapradius * radial.y;
		return;
	}

	n = normalize(n);

	u = envmapwidth * (0.5f + 0.5f * n.x);
	v = envmapheight * (0.5f + 0.5f * n.y);
}

//
// The normal that reflects the view axis I0 = (0, 0, 1) to r, the half-way
// vector
//
//   N' = (r/|r| - I0) / |r/|r| - I0|
//
// N' is always front-facing, N'z ≤ 0. r = I0 has none: it is the one ray the
// whole rim of the disk shares, a ray reflected straight on past the model,
// and fallback stands in for it.
//
inline vec3 RETRO_ReflectionHalfway(vec3 r, vec3 fallback)
{
	const float epsilon = 1.0e-12f;

	vec3 h = normalize(r) - vec3{ 0.0f, 0.0f, 1.0f };
	float lengthsquared = dot(h, h);
	return lengthsquared > epsilon ? h * inversesqrt(lengthsquared) : fallback;
}

//
// The same photographic lookup, addressed by the reflected ray itself
//
// The map above reflects I0 about N, so the half-way vector of R goes
// straight into the front-face scale of Nxy. For R reflected from I0 the two
// agree only on a front face: past the silhouette R's half-way vector is -N.
// Taken as it is, that is the Blinn/Newell flip, and a ray passing I0 jumps
// to the opposite side of the rim, which in a baked disk is a different
// color. So this path folds as the one above does: n, the unit normal R was
// reflected about, says which side of the disk it is on, and a half-way
// vector facing away from it is turned round, so the lookup comes back
// inward from the rim it reached. r need not be unit. For R = I0, where there
// is no half-way vector, n itself stands in.
//
inline void RETRO_GetReflectionMapCoordinates(vec3 r, vec3 n, int envmapwidth, int envmapheight, float &u, float &v)
{
	vec3 h = RETRO_ReflectionHalfway(r, n);
	if (dot(h, n) < 0.0f) {
		h = -h;
	}

	u = envmapwidth * (0.5f + 0.5f * h.x);
	v = envmapheight * (0.5f + 0.5f * h.y);
}

//
// Horizontal coverage of a triangle at pixel centers (x+1/2, y+1/2).
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
inline void RETRO_DrawFlatPolygon(const PolygonPoint *point, int points, unsigned char color, ClipRect clip = {})
{
	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
// Masked polygon
// Fill a convex polygon writing (pixel & ~mask) | (color & mask)
//
inline void RETRO_DrawMaskedPolygon(const PolygonPoint *point, int points, unsigned char color, unsigned char mask, ClipRect clip = {})
{
	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);

			for (int x = xstart; x < xend; x++) {
				int offset = y * RETRO_WIDTH + x;
				unsigned char &pixel = RETRO.framebuffer[offset];
				pixel = (pixel & ~mask) | (color & mask);
			}
		}
	}
}

//
// Stencil polygon
// Fill a convex polygon with a map read in screen space: the polygon is a
// window cut onto the map, not a surface the map is stretched over. The texel
// under pixel (x, y) is (x - originx, y - originy), wrapped, so the map is
// neither turned nor foreshortened with the polygon, and moving the origin
// slides the picture behind the window.
//
inline void RETRO_DrawStencilPolygon(const PolygonPoint *point, int points, unsigned char *stencilmap, int stencilmapwidth, int stencilmapheight, int originx, int originy, ClipRect clip = {})
{
	if (stencilmap == NULL) return;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
			int v = WRAP(y - originy, stencilmapheight);

			for (int x = xstart; x < xend; x++) {
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					int u = WRAP(x - originx, stencilmapwidth);
					RETRO.framebuffer[offset] = stencilmap[v * stencilmapwidth + u];
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
inline void RETRO_DrawGlenzPolygon(const PolygonPoint *point, int points, unsigned char color, int colormax, ClipRect clip = {})
{
	colormax = CLAMP(colormax, 0, RETRO_COLORS);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
// Remapped polygon
// Pass what is already drawn through a table, where the polygon is in front
// of it. An 8-bit framebuffer holds palette entries, not colors, so blending
// is a lookup: a table that darkens every entry makes a translucent shadow,
// one that tints them a colored glass. Depth is tested but not written, so
// the polygon lies over what is there without hiding what comes after.
//
// Remapping goes in passes, and a pass takes each pixel through the table
// once however many of its polygons cover it: where a fan folds over itself,
// as a polygon that is not flat can on screen, or where polygons that make
// one shape overlap. Each polygon begins a pass of its own unless newpass is
// false, when it joins the pass before it.
//
inline void RETRO_DrawRemapPolygon(const PolygonPoint *point, int points, const unsigned char *table, bool newpass = true, ClipRect clip = {})
{
	if (table == NULL) return;

	static unsigned int stamp[RETRO_WIDTH * RETRO_HEIGHT]; // the last pass to remap each pixel
	static unsigned int pass = 1; // past the stamps' zero, so the first pass can be joined
	if (newpass && ++pass == 0) {
		memset(stamp, 0, sizeof(stamp));
		pass = 1;
	}

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
			float q = p0->q + dqdx * (xstart + 0.5f - p0->pos.x) + dqdy * (y + 0.5f - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				int offset = y * RETRO_WIDTH + x;
				if (q > RETRO_DepthBuffer[offset] && stamp[offset] != pass) {
					RETRO.framebuffer[offset] = table[RETRO.framebuffer[offset]];
					stamp[offset] = pass;
				}
				q += dqdx;
			}
		}
	}
}

//
// Gouraud shaded polygon
// Interpolate palette indices affinely in screen space to keep shared
// triangle edges continuous.
//
inline void RETRO_DrawGouraudPolygon(const PolygonPoint *point, int points, ClipRect clip = {})
{
	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
		t = t * inversesqrt(tlengthsquared);
	} else {
		// If +u is parallel to N, recover it from projected +v. Keep the sign
		// that is closest to the face's original +u direction.
		vec3 b = frame.b - n * dot(n, frame.b);
		float blengthsquared = dot(b, b);
		if (blengthsquared <= epsilon) {
			return n;
		}
		b = b * inversesqrt(blengthsquared);
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

	return bumped * inversesqrt(lengthsquared);
}

//
// Height-field gradient of a bump map at (u, v), in the map's own texels
//
// Sobel: a central difference along one axis, averaged over three rows across
// it. The weights sum to 4, so each component is four times the height
// difference across two texels. Bright texels protrude, so the gradient points
// downhill, the way RETRO_BumpNormal tilts the normal.
//
inline vec2 RETRO_BumpGradient(const unsigned char *bumpmap, int bumpmapwidth, int bumpmapheight, float u, float v)
{
	int uc = CLAMP(u, 0, bumpmapwidth);
	int um = CLAMP(u - 1.0f, 0, bumpmapwidth);
	int up = CLAMP(u + 1.0f, 0, bumpmapwidth);
	int vc = CLAMP(v, 0, bumpmapheight) * bumpmapwidth;
	int vm = CLAMP(v - 1.0f, 0, bumpmapheight) * bumpmapwidth;
	int vp = CLAMP(v + 1.0f, 0, bumpmapheight) * bumpmapwidth;
	int gx = (bumpmap[um + vm] + 2 * bumpmap[um + vc] + bumpmap[um + vp]) -
			 (bumpmap[up + vm] + 2 * bumpmap[up + vc] + bumpmap[up + vp]);
	int gy = (bumpmap[um + vm] + 2 * bumpmap[uc + vm] + bumpmap[up + vm]) -
			 (bumpmap[um + vp] + 2 * bumpmap[uc + vp] + bumpmap[up + vp]);
	return { (float)gx, (float)gy };
}

//
// Gouraud shaded polygon with a bump map
//
// The shade is affine, as in RETRO_DrawGouraudPolygon, and so are the normals
// the bump is tilted from, as in RETRO_DrawTexMapBumpPolygon; the UVs the bump
// map is read through are perspective-correct, in texels of a texmapwidth by
// texmapheight texture. Handing every point the same shade and normal draws a
// flat shaded face, one of each per point a Gouraud shaded one.
//
// The bump moves the shade by the difference it makes to the lighting: every
// light of lighting, a point light as seen from center, the face's. It is
// measured in shades, the entries one unit of shade fraction spans, and the
// pixel is held to the face's ramp, [cstart, cend).
//
// With a color light table the points carry light of a color in its three
// tints instead of a shade, the bump moves each of them by the difference it
// makes to that channel, and the pixel is the table's entry for the light on
// its one material; see RETRO_CreateColorLightTable.
//
inline void RETRO_DrawGouraudBumpPolygon(const PolygonPoint *point, int points, unsigned char *bumpmap, int bumpgrazing, int cstart, int cend, int shades, const RETRO_Lighting &lighting, vec3 center, const TangentFrame &frame, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, const RETRO_ShadeTable *colortable = NULL, ClipRect clip = {})
{
	if (bumpmap == NULL) return;

	const float epsilon = 1.0e-12f;

	// The bump map is read through the UVs at its own rate, and the Sobel
	// gradient scaled to the tilt, as in RETRO_DrawTexMapBumpPolygon
	vec2 bumptexel = { (float)bumpmapwidth / texmapwidth, (float)bumpmapheight / texmapheight };
	vec2 bumptilt = bumptexel / (4 * bumpgrazing);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
		// Light of a color is interpolated like c, a tint for each channel
		vec3 tint0 = { p0->tint[0], p0->tint[1], p0->tint[2] };
		vec3 tint1 = { p1->tint[0], p1->tint[1], p1->tint[2] };
		vec3 tint2 = { p2->tint[0], p2->tint[1], p2->tint[2] };
		vec3 dtdx = { 0.0f, 0.0f, 0.0f };
		vec3 dtdy = { 0.0f, 0.0f, 0.0f };
		if (colortable != NULL) {
			dtdx = ((tint1 - tint0) * (p2->pos.y - p0->pos.y) - (tint2 - tint0) * (p1->pos.y - p0->pos.y)) / determinant;
			dtdy = ((p1->pos.x - p0->pos.x) * (tint2 - tint0) - (p2->pos.x - p0->pos.x) * (tint1 - tint0)) / determinant;
		}

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
			vec3 t = tint0 + dtdx * (px - p0->pos.x) + dtdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					vec2 dh = RETRO_BumpGradient(bumpmap, bumpmapwidth, bumpmapheight, uv.x * inverseq * bumptexel.x, uv.y * inverseq * bumptexel.y) * bumptilt;
					// Interpolated normals must be normalized before lighting.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? inversesqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					vec3 bumpedn = RETRO_BumpNormal(unitn, dh.x, dh.y, frame);
					unsigned char color;
					// The shade moves by as much as the tilt changes the lighting
					// here, so a flat patch of the bump map is left shaded exactly
					// as it was drawn without one.
					if (colortable != NULL) {
						vec3 change = RETRO_ColorLambert(lighting, center, bumpedn) - RETRO_ColorLambert(lighting, center, unitn);
						const int *levels = colortable->tints;
						int r = CLAMP(t.x + change.x * (levels[0] - 1), 0, levels[0]);
						int g = CLAMP(t.y + change.y * (levels[1] - 1), 0, levels[1]);
						int b = CLAMP(t.z + change.z * (levels[2] - 1), 0, levels[2]);
						color = colortable->table[(r * levels[1] + g) * levels[2] + b];
					} else {
						float lambert = RETRO_Lambert(lighting, center, unitn);
						float bumpedlambert = RETRO_Lambert(lighting, center, bumpedn);
						float bumpshade = (RETRO_ShadeFractionFromLambert(bumpedlambert) - RETRO_ShadeFractionFromLambert(lambert)) * shades;
						color = CLAMP(c + bumpshade, cstart, cend);
					}
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = color;
					}
				}
				uv += duvdx;
				c += dcdx;
				n += dndx;
				q += dqdx;
				t += dtdx;
			}
		}
	}
}

//
// Phong shaded polygon
//
// Vertex normals arrive as n * q. Interpolating that product and
// renormalizing is the same direction as divide-by-q then renormalize
// (q > 0 in front of the near plane). A point light is worked out at every
// pixel too, from its view-space position, which arrives as p * q and is
// divided by q the same way, so its light runs on smoothly from one face to
// the next. The pixel is then
//
//   I = ShadeFractionFromLambert(sum of max(N · L, 0) over the lights)
//   color = c + shades * I
//
// c is the ramp's base and shades the entries in it, so c + shades is one
// past its last. With a color light table the lights are added up in their
// colors instead, and the pixel is the table's entry for that light; see
// RETRO_CreateColorLightTable. The lights are summed on N as it arrives and
// scaled to its unit length afterward; the ambient light, which has no
// direction, is added after that.
//
inline void RETRO_DrawPhongPolygon(const PolygonPoint *point, int points, const RETRO_Lighting &lighting, int c, int shades, const RETRO_ShadeTable *colortable = NULL, ClipRect clip = {})
{
	const float epsilon = 1.0e-12f;

	// A point light is found from each pixel's own position, interpolated as
	// the normal is; without one the position is left alone
	bool positions = RETRO_HasPointLight(lighting);

	int cstart = c;
	int cend = MIN(c + shades, RETRO_COLORS);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;
		vec3 dpdx = { 0.0f, 0.0f, 0.0f };
		vec3 dpdy = { 0.0f, 0.0f, 0.0f };
		if (positions) {
			dpdx = ((p1->p - p0->p) * (p2->pos.y - p0->pos.y) - (p2->p - p0->p) * (p1->pos.y - p0->pos.y)) / determinant;
			dpdy = ((p1->pos.x - p0->pos.x) * (p2->p - p0->p) - (p2->pos.x - p0->pos.x) * (p1->p - p0->p)) / determinant;
		}

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);
			vec3 pq = positions ? p0->p + dpdx * (px - p0->pos.x) + dpdy * (py - p0->pos.y) : vec3{ 0.0f, 0.0f, 0.0f };

			for (int x = xstart; x < xend; x++) {
				float normallengthsquared = dot(n, n);
				vec3 p = positions ? pq / q : vec3{ 0.0f, 0.0f, 0.0f };
				unsigned char color;

				// Interpolated normals must be normalized before lighting.
				if (colortable != NULL) {
					vec3 light = { 0.0f, 0.0f, 0.0f };
					if (normallengthsquared > epsilon) {
						light = (RETRO_ColorLambert(lighting, p, n) - lighting.ambient) * inversesqrt(normallengthsquared) + lighting.ambient;
					}
					color = RETRO_ColorLightEntry(*colortable, light);
				} else {
					float intensity = 0.0f;
					if (normallengthsquared > epsilon) {
						intensity = (RETRO_Lambert(lighting, p, n) - lighting.ambient) * inversesqrt(normallengthsquared) + lighting.ambient;
					}
					int ramp = c + shades * RETRO_ShadeFractionFromLambert(intensity);
					color = CLAMP(ramp, cstart, cend);
				}

				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = color;
				}
				n += dndx;
				q += dqdx;
				if (positions) {
					pq += dpdx;
				}
			}
		}
	}
}

//
// RETRO_DrawPhongPolygon with a bump map tilting N at every pixel before it
// is lit, through the points' UVs, in texels of a texmapwidth by
// texmapheight texture, as RETRO_DrawTexMapPhongBumpPolygon does. It has a
// drawer of its own so the smooth one keeps its loop: the UVs are stepped and
// N made unit at every pixel here, for the tilt.
//
inline void RETRO_DrawPhongBumpPolygon(const PolygonPoint *point, int points, unsigned char *bumpmap, int bumpgrazing, int c, int shades, const RETRO_Lighting &lighting, const TangentFrame &frame, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, const RETRO_ShadeTable *colortable = NULL, ClipRect clip = {})
{
	if (bumpmap == NULL) return;

	const float epsilon = 1.0e-12f;

	// A point light is found from each pixel's own position, interpolated as
	// the normal is; without one the position is left alone
	bool positions = RETRO_HasPointLight(lighting);

	// The bump map is read through the UVs, at its own rate, as in
	// RETRO_DrawTexMapPhongBumpPolygon
	vec2 bumptexel = { (float)bumpmapwidth / texmapwidth, (float)bumpmapheight / texmapheight };
	vec2 bumptilt = bumptexel / (4 * bumpgrazing);

	int cstart = c;
	int cend = MIN(c + shades, RETRO_COLORS);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;
		vec3 dpdx = { 0.0f, 0.0f, 0.0f };
		vec3 dpdy = { 0.0f, 0.0f, 0.0f };
		if (positions) {
			dpdx = ((p1->p - p0->p) * (p2->pos.y - p0->pos.y) - (p2->p - p0->p) * (p1->pos.y - p0->pos.y)) / determinant;
			dpdy = ((p1->pos.x - p0->pos.x) * (p2->p - p0->p) - (p2->pos.x - p0->pos.x) * (p1->p - p0->p)) / determinant;
		}
		vec2 uv0 = p0->uv * p0->q, uv1 = p1->uv * p1->q, uv2 = p2->uv * p2->q;
		vec2 duvdx = ((uv1 - uv0) * (p2->pos.y - p0->pos.y) - (uv2 - uv0) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 duvdy = ((p1->pos.x - p0->pos.x) * (uv2 - uv0) - (p2->pos.x - p0->pos.x) * (uv1 - uv0)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);
			vec3 pq = positions ? p0->p + dpdx * (px - p0->pos.x) + dpdy * (py - p0->pos.y) : vec3{ 0.0f, 0.0f, 0.0f };
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				// Interpolated normals must be normalized before lighting, and
				// a bump tilts the unit normal
				float normallengthsquared = dot(n, n);
				bool facing = normallengthsquared > epsilon;
				vec3 unitn = facing ? n * inversesqrt(normallengthsquared) : vec3{ 0.0f, 0.0f, 0.0f };
				if (facing && fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					vec2 dh = RETRO_BumpGradient(bumpmap, bumpmapwidth, bumpmapheight, uv.x * inverseq * bumptexel.x, uv.y * inverseq * bumptexel.y) * bumptilt;
					unitn = RETRO_BumpNormal(unitn, dh.x, dh.y, frame);
				}
				vec3 p = positions ? pq / q : vec3{ 0.0f, 0.0f, 0.0f };
				unsigned char color;
				if (colortable != NULL) {
					vec3 light = facing ? RETRO_ColorLambert(lighting, p, unitn) : vec3{ 0.0f, 0.0f, 0.0f };
					color = RETRO_ColorLightEntry(*colortable, light);
				} else {
					float intensity = facing ? RETRO_Lambert(lighting, p, unitn) : 0.0f;
					int ramp = c + shades * RETRO_ShadeFractionFromLambert(intensity);
					color = CLAMP(ramp, cstart, cend);
				}

				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = color;
				}
				n += dndx;
				q += dqdx;
				uv += duvdx;
				if (positions) {
					pq += dpdx;
				}
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
inline void RETRO_DrawTexMapPolygon(const PolygonPoint *point, int points, unsigned char *texmap, int texmapwidth, int texmapheight, bool wrap = false, ClipRect clip = {})
{
	if (texmap == NULL) return;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
// drawn from all of it and not from its first thirty-two entries. A table
// with tints has each tint interpolated beside the shade, the same way.
//
inline void RETRO_DrawTexMapGouraudPolygon(const PolygonPoint *point, int points, unsigned char *texmap, int texmapwidth, int texmapheight, const RETRO_ShadeTable &shadetable, bool wrap = false, ClipRect clip = {})
{
	if (texmap == NULL || shadetable.table == NULL) return;

	int tints = 0;
	while (tints < RETRO_MAX_TINTS && shadetable.tints[tints] > 0) tints++;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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

		// Each tint is interpolated like c, and only those the table has: a
		// caller with a plain table need not have set any
		float dtdx[RETRO_MAX_TINTS], dtdy[RETRO_MAX_TINTS];
		for (int i = 0; i < tints; i++) {
			dtdx[i] = ((p1->tint[i] - p0->tint[i]) * (p2->pos.y - p0->pos.y) - (p2->tint[i] - p0->tint[i]) * (p1->pos.y - p0->pos.y)) / determinant;
			dtdy[i] = ((p1->pos.x - p0->pos.x) * (p2->tint[i] - p0->tint[i]) - (p2->pos.x - p0->pos.x) * (p1->tint[i] - p0->tint[i])) / determinant;
		}

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			float c = p0->c + dcdx * (px - p0->pos.x) + dcdy * (py - p0->pos.y);
			float t[RETRO_MAX_TINTS];
			for (int i = 0; i < tints; i++) {
				t[i] = p0->tint[i] + dtdx[i] * (px - p0->pos.x) + dtdy[i] * (py - p0->pos.y);
			}
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					vec2 texmapcoord = uv * inverseq;
					int u = wrap ? WRAP(texmapcoord.x, texmapwidth) : CLAMP(texmapcoord.x, 0, texmapwidth);
					int v = wrap ? WRAP(texmapcoord.y, texmapheight) : CLAMP(texmapcoord.y, 0, texmapheight);
					unsigned char texel = CLAMP(texmap[v * texmapwidth + u], 0, shadetable.colors);
					int shade = CLAMP(c, 0, shadetable.shades);
					int entry = texel * shadetable.shades + shade;
					for (int i = 0; i < tints; i++) {
						entry = entry * shadetable.tints[i] + CLAMP(t[i], 0, shadetable.tints[i]);
					}
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable.table[entry];
					}
				}
				uv += duvdx;
				c += dcdx;
				for (int i = 0; i < tints; i++) {
					t[i] += dtdx[i];
				}
				q += dqdx;
			}
		}
	}
}

//
// Bump mapped shaded texture mapped polygon
// Texture coordinates are perspective-correct; the shade is affine, as in
// RETRO_DrawTexMapGouraudPolygon, and the normals the bump is tilted from
// retain the affine interpolation of the environment mappers. Handing every
// point the same shade and normal draws a flat shaded face, one of each per
// point a Gouraud shaded one.
//
// lambertshades is how far up the shade table one unit of shade fraction
// (RETRO_ShadeFractionFromLambert) carries a face, which a model sets for
// itself and which is not the table's own height. The bump moves the shade by
// the difference it makes to the lighting, so it is measured in the same steps
// the face was already shaded in: every light of lighting, a point light as
// seen from center, the face's.
//
// With a color light table the points carry light of a color in its three
// tints instead of a shade, and the bump moves each of them by the difference
// it makes to that channel, each light in its own color. The pixel is the
// entry on its texel's row of the table, with shadetable and lambertshades
// unused; see RETRO_CreateColorLightTable.
//
inline void RETRO_DrawTexMapBumpPolygon(const PolygonPoint *point, int points, unsigned char *texmap, unsigned char *bumpmap, int bumpgrazing, const RETRO_ShadeTable &shadetable, int lambertshades, const RETRO_Lighting &lighting, vec3 center, const TangentFrame &frame, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, const RETRO_ShadeTable *colortable = NULL, ClipRect clip = {})
{
	if (texmap == NULL || bumpmap == NULL) return;
	// A shade table with tints is laid out at a stride this drawer does not
	// step, so it draws nothing rather than read the wrong entries
	if (colortable == NULL && (shadetable.table == NULL || shadetable.tints[0] > 0)) return;

	const float epsilon = 1.0e-12f;

	// The bump map is addressed by the texture's UVs, so a map with a size of its own is
	// stepped through at its own rate, and its gradient divided by the surface distance
	// one of its texels covers. Resampling the map changes its detail, not its depth
	vec2 bumptexel = { (float)bumpmapwidth / texmapwidth, (float)bumpmapheight / texmapheight };

	// The Sobel weights sum to 4, leaving the height difference across two
	// texels, and bumpgrazing is the difference that tilts the normal 45°
	vec2 bumptilt = bumptexel / (4 * bumpgrazing);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
		// Light of a color is interpolated like c, a tint for each channel
		vec3 tint0 = { p0->tint[0], p0->tint[1], p0->tint[2] };
		vec3 tint1 = { p1->tint[0], p1->tint[1], p1->tint[2] };
		vec3 tint2 = { p2->tint[0], p2->tint[1], p2->tint[2] };
		vec3 dtdx = { 0.0f, 0.0f, 0.0f };
		vec3 dtdy = { 0.0f, 0.0f, 0.0f };
		if (colortable != NULL) {
			dtdx = ((tint1 - tint0) * (p2->pos.y - p0->pos.y) - (tint2 - tint0) * (p1->pos.y - p0->pos.y)) / determinant;
			dtdy = ((p1->pos.x - p0->pos.x) * (tint2 - tint0) - (p2->pos.x - p0->pos.x) * (tint1 - tint0)) / determinant;
		}

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
			vec3 t = tint0 + dtdx * (px - p0->pos.x) + dtdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uv.x * inverseq;
					float v = uv.y * inverseq;
					unsigned int texmapu = CLAMP(u, 0, texmapwidth);
					unsigned int texmapv = CLAMP(v, 0, texmapheight);
					vec2 dh = RETRO_BumpGradient(bumpmap, bumpmapwidth, bumpmapheight, u * bumptexel.x, v * bumptexel.y) * bumptilt;
					// Interpolated normals must be normalized before lighting.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? inversesqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					vec3 bumpedn = RETRO_BumpNormal(unitn, dh.x, dh.y, frame);
					unsigned char color;
					// The shade moves by as much as the tilt changes the lighting
					// here, so a flat patch of the bump map is left shaded exactly
					// as it was drawn without one.
					if (colortable != NULL) {
						vec3 change = RETRO_ColorLambert(lighting, center, bumpedn) - RETRO_ColorLambert(lighting, center, unitn);
						const int *levels = colortable->tints;
						int texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, colortable->colors);
						int r = CLAMP(t.x + change.x * (levels[0] - 1), 0, levels[0]);
						int g = CLAMP(t.y + change.y * (levels[1] - 1), 0, levels[1]);
						int b = CLAMP(t.z + change.z * (levels[2] - 1), 0, levels[2]);
						color = colortable->table[((texel * levels[0] + r) * levels[1] + g) * levels[2] + b];
					} else {
						float lambert = RETRO_Lambert(lighting, center, unitn);
						float bumpedlambert = RETRO_Lambert(lighting, center, bumpedn);
						float bumpshade = (RETRO_ShadeFractionFromLambert(bumpedlambert) - RETRO_ShadeFractionFromLambert(lambert)) * lambertshades;
						unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, shadetable.colors);
						int shade = CLAMP(c + bumpshade, 0, shadetable.shades);
						color = shadetable.table[texel * shadetable.shades + shade];
					}
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = color;
					}
				}
				uv += duvdx;
				c += dcdx;
				n += dndx;
				q += dqdx;
				t += dtdx;
			}
		}
	}
}

//
// Phong shaded texture mapped polygon
// Texture coordinates are perspective-correct, and so are the normals: they
// arrive as n * q, as in RETRO_DrawPhongPolygon, and are renormalized at
// every pixel. Each pixel is lit there by every light, a point light from the
// pixel's own position as in RETRO_DrawPhongPolygon, so a highlight smaller
// than a face still shows, and takes its level in the shade table from
//
//   level = shade + ShadeFractionFromLambert(sum of max(N · L, 0)) * lambertshades
//
// shade is the face's base level and lambertshades how far up the table one
// unit of shade fraction carries it, as for RETRO_DrawTexMapBumpPolygon.
//
// With a color light table the lights are added up in their colors instead,
// and the pixel is that light's entry on its texel's row of the table, with
// shadetable, shade and lambertshades unused; see RETRO_CreateColorLightTable.
//
inline void RETRO_DrawTexMapPhongPolygon(const PolygonPoint *point, int points, unsigned char *texmap, const RETRO_ShadeTable &shadetable, int shade, int lambertshades, const RETRO_Lighting &lighting, int texmapwidth, int texmapheight, const RETRO_ShadeTable *colortable = NULL, ClipRect clip = {})
{
	if (texmap == NULL) return;
	// A shade table with tints is laid out at a stride this drawer does not
	// step, so it draws nothing rather than read the wrong entries
	if (colortable == NULL && (shadetable.table == NULL || shadetable.tints[0] > 0)) return;

	const float epsilon = 1.0e-12f;

	// A point light is found from each pixel's own position, interpolated as
	// the normal is; without one the position is left alone
	bool positions = RETRO_HasPointLight(lighting);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
		vec3 dpdx = { 0.0f, 0.0f, 0.0f };
		vec3 dpdy = { 0.0f, 0.0f, 0.0f };
		if (positions) {
			dpdx = ((p1->p - p0->p) * (p2->pos.y - p0->pos.y) - (p2->p - p0->p) * (p1->pos.y - p0->pos.y)) / determinant;
			dpdy = ((p1->pos.x - p0->pos.x) * (p2->p - p0->p) - (p2->pos.x - p0->pos.x) * (p1->p - p0->p)) / determinant;
		}

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);
			vec3 pq = positions ? p0->p + dpdx * (px - p0->pos.x) + dpdy * (py - p0->pos.y) : vec3{ 0.0f, 0.0f, 0.0f };

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uv.x * inverseq;
					float v = uv.y * inverseq;
					unsigned int texmapu = CLAMP(u, 0, texmapwidth);
					unsigned int texmapv = CLAMP(v, 0, texmapheight);
					// Interpolated normals must be normalized before lighting.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? inversesqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					vec3 p = positions ? pq * inverseq : vec3{ 0.0f, 0.0f, 0.0f };
					unsigned char color;
					if (colortable != NULL) {
						unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, colortable->colors);
						color = RETRO_ColorLightEntry(*colortable, RETRO_ColorLambert(lighting, p, unitn), texel);
					} else {
						float lambert = RETRO_Lambert(lighting, p, unitn);
						int level = CLAMP(shade + RETRO_ShadeFractionFromLambert(lambert) * lambertshades, 0, shadetable.shades);
						unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, shadetable.colors);
						color = shadetable.table[texel * shadetable.shades + level];
					}
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = color;
					}
				}
				uv += duvdx;
				n += dndx;
				q += dqdx;
				if (positions) {
					pq += dpdx;
				}
			}
		}
	}
}

//
// RETRO_DrawTexMapPhongPolygon with a bump map tilting N at every pixel before
// it is lit, through the texture's UVs as in RETRO_DrawTexMapBumpPolygon. It
// has a drawer of its own so the smooth one keeps its loop.
//
inline void RETRO_DrawTexMapPhongBumpPolygon(const PolygonPoint *point, int points, unsigned char *texmap, unsigned char *bumpmap, int bumpgrazing, const RETRO_ShadeTable &shadetable, int shade, int lambertshades, const RETRO_Lighting &lighting, const TangentFrame &frame, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, const RETRO_ShadeTable *colortable = NULL, ClipRect clip = {})
{
	if (texmap == NULL || bumpmap == NULL) return;
	// A shade table with tints is laid out at a stride this drawer does not
	// step, so it draws nothing rather than read the wrong entries
	if (colortable == NULL && (shadetable.table == NULL || shadetable.tints[0] > 0)) return;

	const float epsilon = 1.0e-12f;

	// A point light is found from each pixel's own position, interpolated as
	// the normal is; without one the position is left alone
	bool positions = RETRO_HasPointLight(lighting);

	vec2 bumptexel = { (float)bumpmapwidth / texmapwidth, (float)bumpmapheight / texmapheight };
	vec2 bumptilt = bumptexel / (4 * bumpgrazing);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
		vec3 dpdx = { 0.0f, 0.0f, 0.0f };
		vec3 dpdy = { 0.0f, 0.0f, 0.0f };
		if (positions) {
			dpdx = ((p1->p - p0->p) * (p2->pos.y - p0->pos.y) - (p2->p - p0->p) * (p1->pos.y - p0->pos.y)) / determinant;
			dpdy = ((p1->pos.x - p0->pos.x) * (p2->p - p0->p) - (p2->pos.x - p0->pos.x) * (p1->p - p0->p)) / determinant;
		}

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec2 uv = uv0 + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);
			vec3 pq = positions ? p0->p + dpdx * (px - p0->pos.x) + dpdy * (py - p0->pos.y) : vec3{ 0.0f, 0.0f, 0.0f };

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uv.x * inverseq;
					float v = uv.y * inverseq;
					unsigned int texmapu = CLAMP(u, 0, texmapwidth);
					unsigned int texmapv = CLAMP(v, 0, texmapheight);
					// Interpolated normals must be normalized before lighting.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? inversesqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					vec2 dh = RETRO_BumpGradient(bumpmap, bumpmapwidth, bumpmapheight, u * bumptexel.x, v * bumptexel.y) * bumptilt;
					unitn = RETRO_BumpNormal(unitn, dh.x, dh.y, frame);
					vec3 p = positions ? pq * inverseq : vec3{ 0.0f, 0.0f, 0.0f };
					unsigned char color;
					if (colortable != NULL) {
						unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, colortable->colors);
						color = RETRO_ColorLightEntry(*colortable, RETRO_ColorLambert(lighting, p, unitn), texel);
					} else {
						float lambert = RETRO_Lambert(lighting, p, unitn);
						int level = CLAMP(shade + RETRO_ShadeFractionFromLambert(lambert) * lambertshades, 0, shadetable.shades);
						unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, shadetable.colors);
						color = shadetable.table[texel * shadetable.shades + level];
					}
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = color;
					}
				}
				uv += duvdx;
				n += dndx;
				q += dqdx;
				if (positions) {
					pq += dpdx;
				}
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
//
inline void RETRO_DrawTexMapEnvMapPolygon(const PolygonPoint *point, int points, unsigned char *texmap, unsigned char *envmap, const RETRO_ShadeTable &shadetable, unsigned char shade, bool lightingmap, int envmapwidth, int envmapheight, int envmapradius, int texmapwidth, int texmapheight, ClipRect clip = {})
{
	// A table with tints is laid out at a stride this drawer does not step, so
	// it draws nothing rather than read the wrong entries
	if (texmap == NULL || shadetable.table == NULL || shadetable.tints[0] > 0) return;

	const float epsilon = 1.0e-12f;
	bool envmapshading = envmap != NULL;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
//
inline void RETRO_DrawTexMapEnvMapBumpPolygon(const PolygonPoint *point, int points, unsigned char *texmap, unsigned char *envmap, unsigned char *bumpmap, int bumpgrazing, const RETRO_ShadeTable &shadetable, bool lightingmap, const TangentFrame &frame, int envmapwidth, int envmapheight, int envmapradius, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, ClipRect clip = {})
{
	// A table with tints is laid out at a stride this drawer does not step, so
	// it draws nothing rather than read the wrong entries
	if (texmap == NULL || envmap == NULL || bumpmap == NULL || shadetable.table == NULL || shadetable.tints[0] > 0) return;

	const float epsilon = 1.0e-12f;

	vec2 bumptexel = { (float)bumpmapwidth / texmapwidth, (float)bumpmapheight / texmapheight };

	// The Sobel weights sum to 4, leaving the height difference across two
	// texels, and bumpgrazing is the difference that tilts the normal 45°
	vec2 bumptilt = bumptexel / (4 * bumpgrazing);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
					vec2 dh = RETRO_BumpGradient(bumpmap, bumpmapwidth, bumpmapheight, u * bumptexel.x, v * bumptexel.y) * bumptilt;
					float e, w;
					// Both maps are functions of the unit normal, so both tilt N and
					// look the result up. A unit N' always lands inside either map.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? inversesqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					vec3 bumpednormal = RETRO_BumpNormal(unitn, dh.x, dh.y, frame);
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
// point.n is the interpolated normal, multiplied by q so it is
// perspective-correct, and normalized at lookup. With reflectedray point.p is
// the view ray to the vertex, also times q, and each pixel reflects it about
// its own normal, which only a reflection map is read by. Reflecting at the
// corners and interpolating the rays would be exact only on a flat face: where
// the normals differ, the corners' rays can average out to one pointing
// nowhere near what the middle of the face reflects.
//
inline void RETRO_DrawEnvMapPolygon(const PolygonPoint *point, int points, unsigned char *envmap, bool lightingmap, bool reflectedray, int envmapwidth, int envmapheight, int envmapradius, ClipRect clip = {})
{
	if (envmap == NULL) return;

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		vec3 dpdx = { 0.0f, 0.0f, 0.0f };
		vec3 dpdy = { 0.0f, 0.0f, 0.0f };
		if (reflectedray) {
			dpdx = ((p1->p - p0->p) * (p2->pos.y - p0->pos.y) - (p2->p - p0->p) * (p1->pos.y - p0->pos.y)) / determinant;
			dpdy = ((p1->pos.x - p0->pos.x) * (p2->p - p0->p) - (p2->pos.x - p0->pos.x) * (p1->p - p0->p)) / determinant;
		}
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			vec3 p = reflectedray ? p0->p + dpdx * (px - p0->pos.x) + dpdy * (py - p0->pos.y) : vec3{ 0.0f, 0.0f, 0.0f };
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				float e, w;
				if (reflectedray) {
					vec3 normal = normalize(n);
					RETRO_GetReflectionMapCoordinates(reflect(p, normal), normal, envmapwidth, envmapheight, e, w);
				} else {
					RETRO_GetEnvMapCoordinates(n, lightingmap, envmapwidth, envmapheight, envmapradius, e, w);
				}
				unsigned int envmapu = CLAMP(e, 0, envmapwidth);
				unsigned int envmapv = CLAMP(w, 0, envmapheight);
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = envmap[envmapv * envmapwidth + envmapu];
				}
				n += dndx;
				if (reflectedray) {
					p += dpdx;
				}
				q += dqdx;
			}
		}
	}
}

//
// Shader polygon
// Every pixel is described as a Fragment and handed to shader, and what it
// returns is written. point.p, point.n, point.uv and point.lightuv arrive
// times q, so all four are perspective-correct: p and the coordinates are
// divided by q at each pixel, and n is only normalized, which dividing it
// first would not change. A constant n is a flat face; an interpolated one
// bends smoothly across it. data reaches the shader in every fragment, so
// what it shades with can belong to this one polygon.
//
inline void RETRO_DrawShaderPolygon(const PolygonPoint *point, int points, vec3 eye, unsigned char (*shader)(const Fragment &fragment), const void *data = NULL, ClipRect clip = {})
{
	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend, clip);
		if (determinant == 0.0f) continue;

		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;
		vec3 dpdx = ((p1->p - p0->p) * (p2->pos.y - p0->pos.y) - (p2->p - p0->p) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dpdy = ((p1->pos.x - p0->pos.x) * (p2->p - p0->p) - (p2->pos.x - p0->pos.x) * (p1->p - p0->p)) / determinant;
		vec2 duvdx = ((p1->uv - p0->uv) * (p2->pos.y - p0->pos.y) - (p2->uv - p0->uv) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 duvdy = ((p1->pos.x - p0->pos.x) * (p2->uv - p0->uv) - (p2->pos.x - p0->pos.x) * (p1->uv - p0->uv)) / determinant;
		vec2 dlightuvdx = ((p1->lightuv - p0->lightuv) * (p2->pos.y - p0->pos.y) - (p2->lightuv - p0->lightuv) * (p1->pos.y - p0->pos.y)) / determinant;
		vec2 dlightuvdy = ((p1->pos.x - p0->pos.x) * (p2->lightuv - p0->lightuv) - (p2->pos.x - p0->pos.x) * (p1->lightuv - p0->lightuv)) / determinant;
		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), clip.x0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), clip.x1);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			vec3 n = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);
			vec3 p = p0->p + dpdx * (px - p0->pos.x) + dpdy * (py - p0->pos.y);
			vec2 uv = p0->uv + duvdx * (px - p0->pos.x) + duvdy * (py - p0->pos.y);
			vec2 lightuv = p0->lightuv + dlightuvdx * (px - p0->pos.x) + dlightuvdy * (py - p0->pos.y);
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					float depth = 1.0f / q;
					Fragment fragment;
					fragment.x = x;
					fragment.y = y;
					fragment.q = q;
					fragment.position = p * depth;
					fragment.normal = normalize(n);
					fragment.view = fragment.position - eye;
					fragment.uv = uv * depth;
					fragment.lightuv = lightuv * depth;
					fragment.data = data;
					RETRO.framebuffer[offset] = shader(fragment);
				}
				n += dndx;
				p += dpdx;
				uv += duvdx;
				lightuv += dlightuvdx;
				q += dqdx;
			}
		}
	}
}

//
// Bump-mapped environment polygon
// Lighting and reflection maps are read at the tilted unit normal N'.
//
inline void RETRO_DrawEnvMapBumpPolygon(const PolygonPoint *point, int points, unsigned char *envmap, unsigned char *bumpmap, int bumpgrazing, bool lightingmap, const TangentFrame &frame, int envmapwidth, int envmapheight, int envmapradius, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight, ClipRect clip = {})
{
	if (envmap == NULL || bumpmap == NULL) return;

	const float epsilon = 1.0e-12f;

	vec2 bumptexel = { (float)bumpmapwidth / texmapwidth, (float)bumpmapheight / texmapheight };

	// The Sobel weights sum to 4, leaving the height difference across two
	// texels, and bumpgrazing is the difference that tilts the normal 45°
	vec2 bumptilt = bumptexel / (4 * bumpgrazing);

	for (int triangle = 1; triangle < points - 1; triangle++) {
		const PolygonPoint *p0 = &point[0];
		const PolygonPoint *p1 = &point[triangle];
		const PolygonPoint *p2 = &point[triangle + 1];
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
					vec2 dh = RETRO_BumpGradient(bumpmap, bumpmapwidth, bumpmapheight, u * bumptexel.x, v * bumptexel.y) * bumptilt;
					float e, w;
					// Both maps are functions of the unit normal, so both tilt N and
					// look the result up. A unit N' always lands inside either map.
					float normallengthsquared = dot(n, n);
					float inversenormallength = normallengthsquared > epsilon ? inversesqrt(normallengthsquared) : 0.0f;
					vec3 unitn = n * inversenormallength;
					vec3 bumpednormal = RETRO_BumpNormal(unitn, dh.x, dh.y, frame);
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
	if (map == NULL || depthmap == NULL || q <= 0.0f || size <= 0.0f) return;

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
			// At or behind the near plane, one unit in front of the eye
			if (pixeldepth <= 1.0f) continue;

			int offset = y * RETRO_WIDTH + x;
			if (RETRO_DepthTest(offset, 1.0f / pixeldepth)) {
				buffer[offset] = color;
			}
		}
	}
}

#endif
