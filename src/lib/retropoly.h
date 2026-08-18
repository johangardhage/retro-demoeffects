//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROPOLY_H_
#define _RETROPOLY_H_

#include "retrocolor.h"
#include "retromodel.h"

struct PolygonPoint {
	float x, y;
	float c;
	float u, v;				// Texture UV coordinates
	float q;				// Reciprocal projection depth
	float nx, ny, nz;
};

// The surface directions +u and +v run in, in view space. A bump map is a
// height field over (u, v), so its gradient tilts the normal along these and
// not along the screen axes.
struct TangentFrame {
	float tx, ty, tz;
	float bx, by, bz;
};

struct LightSourcePoint {
	float nx, ny, nz, nn;	// Normal coordinates
	int c, cintensity;		// Min, max color
};

struct TriangleSpan {
	float x1, x2;
};

//
// Depth buffer, in q = 1 / depth
//
// The drawers already carry q per pixel for perspective-correct texturing,
// and larger q is nearer, so the same number resolves depth. Cleared to 0,
// infinitely far, once per model.
//
float RETRO_DepthBuffer[RETRO_WIDTH * RETRO_HEIGHT];

void RETRO_ClearDepthBuffer(void)
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
// A lighting map (CreatePhongMap) is the front disk of a sphere:
//
//   u = W/2 + I * Nx
//   v = H/2 + I * Ny
//
// The unlit hemisphere (Nz > 0) is folded onto the dark rim.
//
// A photographic reflection map is Blinn/Newell sphere-mapping of
// R = 2(N·V)N - V with V = (0, 0, -1). For a unit N that identity
// simplifies to a hemisphere-aware scale of Nxy, with no second sqrt:
//
//   u = W (1/2 - sign(Nz) * Nx / 2)
//   v = H (1/2 - sign(Nz) * Ny / 2)
//
// I is unused on that path: the sphere map already covers the full image.
//
void RETRO_GetEnvMapCoordinates(float nx, float ny, float nz, bool lightingmap, int envmapwidth, int envmapheight, int intensity, float &u, float &v)
{
	const float epsilon = 1.0e-12f;

	if (lightingmap) {
		if (nz > 0.0f) {
			float radiallengthsquared = nx * nx + ny * ny;
			if (radiallengthsquared > epsilon) {
				float inverseradiallength = 1.0f / sqrt(radiallengthsquared);
				nx *= inverseradiallength;
				ny *= inverseradiallength;
			} else {
				nx = 1.0f;
				ny = 0.0f;
			}
		} else {
			float normallengthsquared = nx * nx + ny * ny + nz * nz;
			float inversenormallength = normallengthsquared > epsilon ? 1.0f / sqrt(normallengthsquared) : 0.0f;
			nx *= inversenormallength;
			ny *= inversenormallength;
		}

		u = envmapwidth / 2 + intensity * nx;
		v = envmapheight / 2 + intensity * ny;
		return;
	}

	float normallengthsquared = nx * nx + ny * ny + nz * nz;
	float inversenormallength = normallengthsquared > epsilon ? 1.0f / sqrt(normallengthsquared) : 0.0f;
	nx *= inversenormallength;
	ny *= inversenormallength;
	nz *= inversenormallength;

	// sign(Nz) < 0 is the front (toward the camera).
	float half = nz < 0.0f ? 0.5f : -0.5f;
	u = envmapwidth * (0.5f + half * nx);
	v = envmapheight * (0.5f + half * ny);
}

//
// Horizontal coverage of a triangle at pixel centres (x+1/2, y+1/2).
//
// The signed area
//
//   A = (p1x - p0x)(p2y - p0y) - (p1y - p0y)(p2x - p0x)
//
// is twice the geometric area, and is the denominator of every screen-space
// gradient: dF/dx = ((F1-F0)(p2y-p0y) - (F2-F0)(p1y-p0y)) / A.
// Edges are half-open in y so a shared edge is drawn by exactly one triangle.
//
float RETRO_ScanTriangle(const PolygonPoint *p0, const PolygonPoint *p1, const PolygonPoint *p2, TriangleSpan *span, int &ystart, int &yend)
{
	const float epsilon = 1.0e-12f;
	float area = (p1->x - p0->x) * (p2->y - p0->y) - (p1->y - p0->y) * (p2->x - p0->x);
	if (fabs(area) <= epsilon) return 0.0f;

	ystart = MAX((int)ceil(MIN(p0->y, MIN(p1->y, p2->y)) - 0.5f), 0);
	yend = MIN((int)ceil(MAX(p0->y, MAX(p1->y, p2->y)) - 0.5f), RETRO_HEIGHT);
	if (ystart >= yend) return 0.0f;

	for (int y = ystart; y < yend; y++) {
		span[y].x1 = RETRO_WIDTH;
		span[y].x2 = 0;
	}

	const PolygonPoint *edgevertices[] = { p0, p1, p2, p0 };
	for (int edge = 0; edge < 3; edge++) {
		const PolygonPoint *a = edgevertices[edge];
		const PolygonPoint *b = edgevertices[edge + 1];
		if (b->y < a->y) SWAP(a, b);

		float ydiff = b->y - a->y;
		if (ydiff == 0.0f) continue;

		float dxdy = (b->x - a->x) / ydiff;
		// Include scanlines whose pixel center lies within the half-open edge.
		int edgeystart = MAX((int)ceil(a->y - 0.5f), ystart);
		int edgeyend = MIN((int)ceil(b->y - 0.5f), yend);
		float x = a->x + ((edgeystart + 0.5f) - a->y) * dxdy;

		for (int y = edgeystart; y < edgeyend; y++, x += dxdy) {
			span[y].x1 = MIN(span[y].x1, x);
			span[y].x2 = MAX(span[y].x2, x);
		}
	}

	return area;
}

//
// Flat shaded polygon
// Split a convex polygon into a triangle fan and fill it with one color.
//
void RETRO_DrawFlatPolygon(PolygonPoint *vertices, int numvertices, unsigned char color)
{
	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

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
//
void RETRO_DrawGlenzPolygon(PolygonPoint *vertices, int numvertices, unsigned char color)
{
	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			for (int x = xstart; x < xend; x++) {
				RETRO.framebuffer[y * RETRO_WIDTH + x] = MIN(RETRO.framebuffer[y * RETRO_WIDTH + x] + color, 255);
			}
		}
	}
}

//
// Gouraud shaded polygon
// Interpolate palette indices affinely in screen space to keep shared
// triangle edges continuous.
//
void RETRO_DrawGouraudPolygon(PolygonPoint *vertices, int numvertices)
{
	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float dcdx = ((p1->c - p0->c) * (p2->y - p0->y) - (p2->c - p0->c) * (p1->y - p0->y)) / area;
		float dcdy = ((p1->x - p0->x) * (p2->c - p0->c) - (p2->x - p0->x) * (p1->c - p0->c)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float c = p0->c + dcdx * (px - p0->x) + dcdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

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
//   color = c + cintensity * I
//
void RETRO_DrawPhongPolygon(PolygonPoint *vertices, int numvertices, LightSourcePoint light)
{
	const float epsilon = 1.0e-12f;

	float inverselightlength = light.nn > 0.0f ? 1.0f / light.nn : 0.0f;
	float lx = light.nx * inverselightlength;
	float ly = light.ny * inverselightlength;
	float lz = light.nz * inverselightlength;
	int cmin = light.c;
	int cmax = MIN(light.c + light.cintensity, RETRO_COLORS - 1);

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float dnxdx, dnxdy, dnydx, dnydy, dnzdx, dnzdy;
		dnxdx = ((p1->nx - p0->nx) * (p2->y - p0->y) - (p2->nx - p0->nx) * (p1->y - p0->y)) / area;
		dnxdy = ((p1->x - p0->x) * (p2->nx - p0->nx) - (p2->x - p0->x) * (p1->nx - p0->nx)) / area;
		dnydx = ((p1->ny - p0->ny) * (p2->y - p0->y) - (p2->ny - p0->ny) * (p1->y - p0->y)) / area;
		dnydy = ((p1->x - p0->x) * (p2->ny - p0->ny) - (p2->x - p0->x) * (p1->ny - p0->ny)) / area;
		dnzdx = ((p1->nz - p0->nz) * (p2->y - p0->y) - (p2->nz - p0->nz) * (p1->y - p0->y)) / area;
		dnzdy = ((p1->x - p0->x) * (p2->nz - p0->nz) - (p2->x - p0->x) * (p1->nz - p0->nz)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float nx = p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y);
			float ny = p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y);
			float nz = p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				float normallengthsquared = nx * nx + ny * ny + nz * nz;
				float intensity = 0.0f;

				// Interpolated normals must be normalized before lighting.
				if (normallengthsquared > epsilon && inverselightlength > 0.0f) {
					float inversenormallength = 1.0f / sqrt(normallengthsquared);
					intensity = MAX((nx * lx + ny * ly + nz * lz) * inversenormallength, 0.0f);
				}

				float paletteintensity = RETRO_ShadeFromLambert(intensity);
				int color = light.c + light.cintensity * paletteintensity;
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = CLAMP(color, cmin, cmax);
				}
				nx += dnxdx;
				ny += dnydx;
				nz += dnzdx;
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
void RETRO_DrawTexMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, int texmapwidth, int texmapheight)
{
	if (texmap == NULL) return;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					unsigned int u = CLAMP(uq * inverseq, 0, texmapwidth);
					unsigned int v = CLAMP(vq * inverseq, 0, texmapheight);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = texmap[v * texmapwidth + u];
					}
				}
				uq += duqdx;
				vq += dvqdx;
				q += dqdx;
			}
		}
	}
}

//
// Gouraud shaded texture mapped polygon
// Texture coordinates are perspective-correct; shade is affine so adjacent
// triangles agree along their shared edge.
//
void RETRO_DrawTexMapGouraudPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, int texmapwidth, int texmapheight, unsigned char *shadetable = NULL)
{
	if (texmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dcdx = ((p1->c - p0->c) * (p2->y - p0->y) - (p2->c - p0->c) * (p1->y - p0->y)) / area;
		float dcdy = ((p1->x - p0->x) * (p2->c - p0->c) - (p2->x - p0->x) * (p1->c - p0->c)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float c = p0->c + dcdx * (px - p0->x) + dcdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					unsigned int u = CLAMP(uq * inverseq, 0, texmapwidth);
					unsigned int v = CLAMP(vq * inverseq, 0, texmapheight);
					unsigned char texel = CLAMP(texmap[v * texmapwidth + u], 0, RETRO_TEXTURE_COLORS);
					int shade = CLAMP128(c);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable[texel * RETRO_SHADES + shade];
					}
				}
				uq += duqdx;
				vq += dvqdx;
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
void RETRO_BumpNormal(float nx, float ny, float nz, float dhx, float dhy, const TangentFrame &frame, float *outnx, float *outny, float *outnz)
{
	if (dhx == 0.0f && dhy == 0.0f) {
		*outnx = nx;
		*outny = ny;
		*outnz = nz;
		return;
	}

	const float epsilon = 1.0e-12f;

	float tx = frame.tx, ty = frame.ty, tz = frame.tz;
	float nt = nx * tx + ny * ty + nz * tz;
	tx -= nx * nt;
	ty -= ny * nt;
	tz -= nz * nt;
	float tlengthsquared = tx * tx + ty * ty + tz * tz;
	if (tlengthsquared > epsilon) {
		float inversetlength = 1.0f / sqrt(tlengthsquared);
		tx *= inversetlength;
		ty *= inversetlength;
		tz *= inversetlength;
	} else {
		// If +u is parallel to N, recover it from projected +v. Keep the sign
		// that is closest to the face's original +u direction.
		float bx = frame.bx, by = frame.by, bz = frame.bz;
		float nb = nx * bx + ny * by + nz * bz;
		bx -= nx * nb;
		by -= ny * nb;
		bz -= nz * nb;
		float blengthsquared = bx * bx + by * by + bz * bz;
		if (blengthsquared <= epsilon) {
			*outnx = nx;
			*outny = ny;
			*outnz = nz;
			return;
		}
		float inverseblength = 1.0f / sqrt(blengthsquared);
		bx *= inverseblength;
		by *= inverseblength;
		bz *= inverseblength;
		tx = by * nz - bz * ny;
		ty = bz * nx - bx * nz;
		tz = bx * ny - by * nx;
		if (tx * frame.tx + ty * frame.ty + tz * frame.tz < 0.0f) {
			tx = -tx;
			ty = -ty;
			tz = -tz;
		}
	}

	// N and T are unit and perpendicular, so their cross product is already a
	// unit +v candidate. Choose its sign to retain mirrored UV handedness.
	float bx = ny * tz - nz * ty;
	float by = nz * tx - nx * tz;
	float bz = nx * ty - ny * tx;
	if (bx * frame.bx + by * frame.by + bz * frame.bz < 0.0f) {
		bx = -bx;
		by = -by;
		bz = -bz;
	}

	// With T and B perpendicular to N, this sum approaches grazing as the
	// gradient grows but cannot cross to the back of the surface.
	float bumpedx = nx + dhx * tx + dhy * bx;
	float bumpedy = ny + dhx * ty + dhy * by;
	float bumpedz = nz + dhx * tz + dhy * bz;

	float lengthsquared = bumpedx * bumpedx + bumpedy * bumpedy + bumpedz * bumpedz;
	if (lengthsquared <= epsilon) {
		*outnx = nx;
		*outny = ny;
		*outnz = nz;
		return;
	}

	float inverselength = 1.0f / sqrt(lengthsquared);
	*outnx = bumpedx * inverselength;
	*outny = bumpedy * inverselength;
	*outnz = bumpedz * inverselength;
}

// L and N' are unit, so the term is N' · L.
float RETRO_BumpedLambert(float nx, float ny, float nz, float dhx, float dhy, const TangentFrame &frame, float lightx, float lighty, float lightz)
{
	RETRO_BumpNormal(nx, ny, nz, dhx, dhy, frame, &nx, &ny, &nz);
	return nx * lightx + ny * lighty + nz * lightz;
}

//
// Bump mapped shaded texture mapped polygon
// Texture coordinates are perspective-correct; the shade is affine, as in
// RETRO_DrawTexMapGouraudPolygon, and the normals the bump is tilted from
// retain the affine interpolation of the environment mappers. Handing every
// vertex the same shade and normal draws a flat shaded face, one of each per
// vertex a gouraud shaded one.
//
void RETRO_DrawTexMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *bumpmap, int bumpheight, unsigned char *shadetable, float lightx, float lighty, float lightz, const TangentFrame &frame, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight)
{
	if (texmap == NULL || bumpmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

	// The bump map is addressed by the texture's UVs, so a map with a size of its own is
	// stepped through at its own rate, and its gradient divided by the surface distance
	// one of its texels covers. Resampling the map changes its detail, not its depth
	float bumptexelu = (float)bumpmapwidth / texmapwidth;
	float bumptexelv = (float)bumpmapheight / texmapheight;

	// The Sobel weights sum to 4, and bumpheight is the height difference that
	// tilts to grazing
	float bumptiltu = bumptexelu / (4 * bumpheight);
	float bumptiltv = bumptexelv / (4 * bumpheight);

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dcdx = ((p1->c - p0->c) * (p2->y - p0->y) - (p2->c - p0->c) * (p1->y - p0->y)) / area;
		float dcdy = ((p1->x - p0->x) * (p2->c - p0->c) - (p2->x - p0->x) * (p1->c - p0->c)) / area;
		float dnxdx = ((p1->nx - p0->nx) * (p2->y - p0->y) - (p2->nx - p0->nx) * (p1->y - p0->y)) / area;
		float dnxdy = ((p1->x - p0->x) * (p2->nx - p0->nx) - (p2->x - p0->x) * (p1->nx - p0->nx)) / area;
		float dnydx = ((p1->ny - p0->ny) * (p2->y - p0->y) - (p2->ny - p0->ny) * (p1->y - p0->y)) / area;
		float dnydy = ((p1->x - p0->x) * (p2->ny - p0->ny) - (p2->x - p0->x) * (p1->ny - p0->ny)) / area;
		float dnzdx = ((p1->nz - p0->nz) * (p2->y - p0->y) - (p2->nz - p0->nz) * (p1->y - p0->y)) / area;
		float dnzdy = ((p1->x - p0->x) * (p2->nz - p0->nz) - (p2->x - p0->x) * (p1->nz - p0->nz)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float c = p0->c + dcdx * (px - p0->x) + dcdy * (py - p0->y);
			float nx = p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y);
			float ny = p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y);
			float nz = p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uq * inverseq;
					float v = vq * inverseq;
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
					float normallengthsquared = nx * nx + ny * ny + nz * nz;
					float inversenormallength = normallengthsquared > epsilon ? 1.0f / sqrt(normallengthsquared) : 0.0f;
					float unitnx = nx * inversenormallength;
					float unitny = ny * inversenormallength;
					float unitnz = nz * inversenormallength;
					// The shade moves by as much as the tilt changes the lighting
					// here, so a flat patch of the bump map is left shaded exactly
					// as it was drawn without one.
					float lambert = unitnx * lightx + unitny * lighty + unitnz * lightz;
					float bumpedlambert = RETRO_BumpedLambert(unitnx, unitny, unitnz, dhx, dhy, frame, lightx, lighty, lightz);
					float bumpshade = (RETRO_ShadeFromLambert(bumpedlambert) - RETRO_ShadeFromLambert(lambert)) * RETRO_SHADES;
					unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, RETRO_TEXTURE_COLORS);
					int shade = CLAMP128(c + bumpshade);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable[texel * RETRO_SHADES + shade];
					}
				}
				uq += duqdx;
				vq += dvqdx;
				c += dcdx;
				nx += dnxdx;
				ny += dnydx;
				nz += dnzdx;
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
void RETRO_DrawTexMapEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap, unsigned char *shadetable, unsigned char shade, bool lightingmap, int envmapintensity, int envmapwidth, int envmapheight, int texmapwidth, int texmapheight)
{
	if (texmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;
	bool envmapshading = envmap != NULL;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dnxdx = 0.0f, dnxdy = 0.0f;
		float dnydx = 0.0f, dnydy = 0.0f;
		float dnzdx = 0.0f, dnzdy = 0.0f;
		if (envmapshading) {
			dnxdx = ((p1->nx - p0->nx) * (p2->y - p0->y) - (p2->nx - p0->nx) * (p1->y - p0->y)) / area;
			dnxdy = ((p1->x - p0->x) * (p2->nx - p0->nx) - (p2->x - p0->x) * (p1->nx - p0->nx)) / area;
			dnydx = ((p1->ny - p0->ny) * (p2->y - p0->y) - (p2->ny - p0->ny) * (p1->y - p0->y)) / area;
			dnydy = ((p1->x - p0->x) * (p2->ny - p0->ny) - (p2->x - p0->x) * (p1->ny - p0->ny)) / area;
			dnzdx = ((p1->nz - p0->nz) * (p2->y - p0->y) - (p2->nz - p0->nz) * (p1->y - p0->y)) / area;
			dnzdy = ((p1->x - p0->x) * (p2->nz - p0->nz) - (p2->x - p0->x) * (p1->nz - p0->nz)) / area;
		}
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float nx = envmapshading ? p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y) : 0.0f;
			float ny = envmapshading ? p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y) : 0.0f;
			float nz = envmapshading ? p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y) : 0.0f;
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					unsigned int u = CLAMP(uq * inverseq, 0, texmapwidth);
					unsigned int v = CLAMP(vq * inverseq, 0, texmapheight);
					unsigned char texel = CLAMP(texmap[v * texmapwidth + u], 0, RETRO_TEXTURE_COLORS);
					unsigned char pixelshade = shade;
					if (envmapshading) {
						float e, w;
						RETRO_GetEnvMapCoordinates(nx, ny, nz, lightingmap, envmapwidth, envmapheight, envmapintensity, e, w);
						unsigned int envmapu = CLAMP(e, 0, envmapwidth);
						unsigned int envmapv = CLAMP(w, 0, envmapheight);
						pixelshade = CLAMP128(envmap[envmapv * envmapwidth + envmapu]);
					}
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable[texel * RETRO_SHADES + pixelshade];
					}
				}
				uq += duqdx;
				vq += dvqdx;
				if (envmapshading) {
					nx += dnxdx;
					ny += dnydx;
					nz += dnzdx;
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
void RETRO_DrawTexMapEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap, unsigned char *bumpmap, int bumpheight, unsigned char *shadetable, bool lightingmap, const TangentFrame &frame, int envmapintensity, int envmapwidth, int envmapheight, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight)
{
	if (texmap == NULL || envmap == NULL || bumpmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

	float bumptexelu = (float)bumpmapwidth / texmapwidth;
	float bumptexelv = (float)bumpmapheight / texmapheight;

	// The Sobel weights sum to 4, and bumpheight is the height difference that
	// tilts to grazing
	float bumptiltu = bumptexelu / (4 * bumpheight);
	float bumptiltv = bumptexelv / (4 * bumpheight);

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dnxdx = ((p1->nx - p0->nx) * (p2->y - p0->y) - (p2->nx - p0->nx) * (p1->y - p0->y)) / area;
		float dnxdy = ((p1->x - p0->x) * (p2->nx - p0->nx) - (p2->x - p0->x) * (p1->nx - p0->nx)) / area;
		float dnydx = ((p1->ny - p0->ny) * (p2->y - p0->y) - (p2->ny - p0->ny) * (p1->y - p0->y)) / area;
		float dnydy = ((p1->x - p0->x) * (p2->ny - p0->ny) - (p2->x - p0->x) * (p1->ny - p0->ny)) / area;
		float dnzdx = ((p1->nz - p0->nz) * (p2->y - p0->y) - (p2->nz - p0->nz) * (p1->y - p0->y)) / area;
		float dnzdy = ((p1->x - p0->x) * (p2->nz - p0->nz) - (p2->x - p0->x) * (p1->nz - p0->nz)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float nx = p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y);
			float ny = p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y);
			float nz = p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uq * inverseq;
					float v = vq * inverseq;
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
					float normallengthsquared = nx * nx + ny * ny + nz * nz;
					float inversenormallength = normallengthsquared > epsilon ? 1.0f / sqrt(normallengthsquared) : 0.0f;
					float bnx, bny, bnz;
					RETRO_BumpNormal(nx * inversenormallength, ny * inversenormallength, nz * inversenormallength,
									 gx * bumptiltu, gy * bumptiltv, frame, &bnx, &bny, &bnz);
					RETRO_GetEnvMapCoordinates(bnx, bny, bnz, lightingmap, envmapwidth, envmapheight, envmapintensity, e, w);
					unsigned int envmapu = CLAMP(e, 0, envmapwidth);
					unsigned int envmapv = CLAMP(w, 0, envmapheight);
					unsigned char texel = CLAMP(texmap[texmapv * texmapwidth + texmapu], 0, RETRO_TEXTURE_COLORS);
					unsigned char pixelshade = CLAMP128(envmap[envmapv * envmapwidth + envmapu]);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = shadetable[texel * RETRO_SHADES + pixelshade];
					}
				}
				uq += duqdx;
				vq += dvqdx;
				nx += dnxdx;
				ny += dnydx;
				nz += dnzdx;
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
void RETRO_DrawEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap, bool lightingmap, int envmapintensity, int envmapwidth, int envmapheight)
{
	if (envmap == NULL) return;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float dnxdx = ((p1->nx - p0->nx) * (p2->y - p0->y) - (p2->nx - p0->nx) * (p1->y - p0->y)) / area;
		float dnxdy = ((p1->x - p0->x) * (p2->nx - p0->nx) - (p2->x - p0->x) * (p1->nx - p0->nx)) / area;
		float dnydx = ((p1->ny - p0->ny) * (p2->y - p0->y) - (p2->ny - p0->ny) * (p1->y - p0->y)) / area;
		float dnydy = ((p1->x - p0->x) * (p2->ny - p0->ny) - (p2->x - p0->x) * (p1->ny - p0->ny)) / area;
		float dnzdx = ((p1->nz - p0->nz) * (p2->y - p0->y) - (p2->nz - p0->nz) * (p1->y - p0->y)) / area;
		float dnzdy = ((p1->x - p0->x) * (p2->nz - p0->nz) - (p2->x - p0->x) * (p1->nz - p0->nz)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float nx = p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y);
			float ny = p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y);
			float nz = p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				float e, w;
				RETRO_GetEnvMapCoordinates(nx, ny, nz, lightingmap, envmapwidth, envmapheight, envmapintensity, e, w);
				unsigned int envmapu = CLAMP(e, 0, envmapwidth);
				unsigned int envmapv = CLAMP(w, 0, envmapheight);
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q)) {
					RETRO.framebuffer[offset] = envmap[envmapv * envmapwidth + envmapu];
				}
				nx += dnxdx;
				ny += dnydx;
				nz += dnzdx;
				q += dqdx;
			}
		}
	}
}

//
// Bump-mapped environment polygon
// Lighting and reflection maps are read at the tilted unit normal N'.
//
void RETRO_DrawEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap, unsigned char *bumpmap, int bumpheight, bool lightingmap, const TangentFrame &frame, int envmapintensity, int envmapwidth, int envmapheight, int texmapwidth, int texmapheight, int bumpmapwidth, int bumpmapheight)
{
	if (envmap == NULL || bumpmap == NULL) return;

	const float epsilon = 1.0e-12f;

	float bumptexelu = (float)bumpmapwidth / texmapwidth;
	float bumptexelv = (float)bumpmapheight / texmapheight;

	// The Sobel weights sum to 4, and bumpheight is the height difference that
	// tilts to grazing
	float bumptiltu = bumptexelu / (4 * bumpheight);
	float bumptiltv = bumptexelv / (4 * bumpheight);

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float area = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;
		float dnxdx = ((p1->nx - p0->nx) * (p2->y - p0->y) - (p2->nx - p0->nx) * (p1->y - p0->y)) / area;
		float dnxdy = ((p1->x - p0->x) * (p2->nx - p0->nx) - (p2->x - p0->x) * (p1->nx - p0->nx)) / area;
		float dnydx = ((p1->ny - p0->ny) * (p2->y - p0->y) - (p2->ny - p0->ny) * (p1->y - p0->y)) / area;
		float dnydy = ((p1->x - p0->x) * (p2->ny - p0->ny) - (p2->x - p0->x) * (p1->ny - p0->ny)) / area;
		float dnzdx = ((p1->nz - p0->nz) * (p2->y - p0->y) - (p2->nz - p0->nz) * (p1->y - p0->y)) / area;
		float dnzdy = ((p1->x - p0->x) * (p2->nz - p0->nz) - (p2->x - p0->x) * (p1->nz - p0->nz)) / area;

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);
			float nx = p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y);
			float ny = p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y);
			float nz = p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseq = 1.0f / q;
					float u = uq * inverseq;
					float v = vq * inverseq;
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
					float normallengthsquared = nx * nx + ny * ny + nz * nz;
					float inversenormallength = normallengthsquared > epsilon ? 1.0f / sqrt(normallengthsquared) : 0.0f;
					float bnx, bny, bnz;
					RETRO_BumpNormal(nx * inversenormallength, ny * inversenormallength, nz * inversenormallength,
									 gx * bumptiltu, gy * bumptiltv, frame, &bnx, &bny, &bnz);
					RETRO_GetEnvMapCoordinates(bnx, bny, bnz, lightingmap, envmapwidth, envmapheight, envmapintensity, e, w);
					unsigned int envmapu = CLAMP(e, 0, envmapwidth);
					unsigned int envmapv = CLAMP(w, 0, envmapheight);
					int offset = y * RETRO_WIDTH + x;
					if (RETRO_DepthTest(offset, q)) {
						RETRO.framebuffer[offset] = envmap[envmapv * envmapwidth + envmapu];
					}
				}
				uq += duqdx;
				vq += dvqdx;
				q += dqdx;
				nx += dnxdx;
				ny += dnydx;
				nz += dnzdx;
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
void RETRO_DrawDepthSprite(float sx, float sy, float q, float size, float thickness, unsigned char *map, float *depthmap, int mapsize, unsigned char alpha = 0, unsigned char *buffer = RETRO.framebuffer)
{
	if (q <= 0.0f || size <= 0.0f) return;

	float half = size / 2;
	int xstart = MAX((int)ceil(sx - half - 0.5f), 0);
	int xend = MIN((int)ceil(sx + half - 0.5f), RETRO_WIDTH);
	int ystart = MAX((int)ceil(sy - half - 0.5f), 0);
	int yend = MIN((int)ceil(sy + half - 0.5f), RETRO_HEIGHT);
	float spritedepth = 1.0f / q;

	for (int y = ystart; y < yend; y++) {
		int v = CLAMP((int)(((y + 0.5f) - sy + half) * mapsize / size), 0, mapsize - 1);
		for (int x = xstart; x < xend; x++) {
			int u = CLAMP((int)(((x + 0.5f) - sx + half) * mapsize / size), 0, mapsize - 1);

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
