//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROPOLY_H_
#define _RETROPOLY_H_

#include "retrocolor.h"

struct PolygonPoint {
	float x, y;
	float c;
	float u, v;				// Texture UV coordinates
	float q;				// Reciprocal projection depth
	float nx, ny, nz;
};

struct LightSourcePoint {
	float nx, ny, nz, nn;	// Normal coordinates
	int c, cintensity;		// Min, max color
};

struct TriangleSpan {
	float x1, x2;
};

//
// Convert a unit view-space normal to environment-map coordinates.
// Both map types use normalized x/y coordinates. Lighting maps additionally
// keep the unlit hemisphere on the generated Phong map's dark rim.
//
void RETRO_GetEnvironmentCoordinates(float nx, float ny, float nz, bool lightingmap, int center, int intensity, float &u, float &v)
{
	const float epsilon = 1.0e-12f;

	if (lightingmap) {
		if (nz > 0.0f) {
			float radialLengthSquared = nx * nx + ny * ny;
			if (radialLengthSquared > epsilon) {
				float inverseRadialLength = 1.0f / sqrt(radialLengthSquared);
				nx *= inverseRadialLength;
				ny *= inverseRadialLength;
			} else {
				nx = 1.0f;
				ny = 0.0f;
			}
		} else {
			float normalLengthSquared = nx * nx + ny * ny + nz * nz;
			float inverseNormalLength = normalLengthSquared > epsilon ? 1.0f / sqrt(normalLengthSquared) : 0.0f;
			nx *= inverseNormalLength;
			ny *= inverseNormalLength;
		}
	}

	u = center + intensity * nx;
	v = center + intensity * ny;
}

//
// Find the horizontal coverage of a triangle at pixel centers.
// The returned signed area is also used to calculate attribute gradients.
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
		int edgeYStart = MAX((int)ceil(a->y - 0.5f), ystart);
		int edgeYEnd = MIN((int)ceil(b->y - 0.5f), yend);
		float x = a->x + ((edgeYStart + 0.5f) - a->y) * dxdy;

		for (int y = edgeYStart; y < edgeYEnd; y++, x += dxdy) {
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

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			for (int x = xstart; x < xend; x++) {
				RETRO.framebuffer[y * RETRO_WIDTH + x] = color;
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

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float c = p0->c + dcdx * (px - p0->x) + dcdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				RETRO.framebuffer[y * RETRO_WIDTH + x] = CLAMP256(c);
				c += dcdx;
			}
		}
	}
}

//
// Phong shaded polygon
// Interpolate vertex normals and evaluate diffuse lighting for every pixel.
//
void RETRO_DrawPhongPolygon(PolygonPoint *vertices, int numvertices, LightSourcePoint light)
{
	const float epsilon = 1.0e-12f;

	float inverseLightLength = light.nn > 0.0f ? 1.0f / light.nn : 0.0f;
	float lx = light.nx * inverseLightLength;
	float ly = light.ny * inverseLightLength;
	float lz = light.nz * inverseLightLength;
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

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float nx = p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y);
			float ny = p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y);
			float nz = p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				float normalLengthSquared = nx * nx + ny * ny + nz * nz;
				float intensity = 0.0f;

				// Interpolated normals must be normalized before lighting.
				if (normalLengthSquared > epsilon && inverseLightLength > 0.0f) {
					float inverseNormalLength = 1.0f / sqrt(normalLengthSquared);
					intensity = MAX((nx * lx + ny * ly + nz * lz) * inverseNormalLength, 0.0f);
				}

				float paletteIntensity = RETRO_ShadeFromLambert(intensity);
				int color = light.c + light.cintensity * paletteIntensity;
				RETRO.framebuffer[y * RETRO_WIDTH + x] = CLAMP(color, cmin, cmax);
				nx += dnxdx;
				ny += dnydx;
				nz += dnzdx;
			}
		}
	}
}

//
// Texture mapped polygon
// Interpolate u/z, v/z and 1/z to remove affine texture distortion.
//
void RETRO_DrawTexMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap)
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
					float inverseQ = 1.0f / q;
					unsigned int u = CLAMP256(uq * inverseQ);
					unsigned int v = CLAMP256(vq * inverseQ);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = texmap[v * 256 + u];
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
void RETRO_DrawTexMapGouraudPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *shadetable = NULL)
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
					float inverseQ = 1.0f / q;
					unsigned int u = CLAMP256(uq * inverseQ);
					unsigned int v = CLAMP256(vq * inverseQ);
					unsigned char texel = CLAMP(texmap[v * 256 + u], 0, RETRO_TEXTURE_COLORS);
					int shade = CLAMP128(c);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = shadetable[texel * RETRO_SHADES + shade];
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
// The lighting a bump takes, as the lambert term of the perturbed normal.
//
// A bump map is a height field, and its gradient tilts the surface normal away
// from the climb. The tilt is applied to the normal's x and y, and its z
// follows from keeping the normal on the unit sphere:
//
//   N' = (nx - dh/du, ny - dh/dv, +-sqrt(1 - nx'^2 - ny'^2))
//
// with the sign of z kept from the normal the surface came in with, since the
// tilt turns a normal but never moves it to the hemisphere behind the surface.
// A gradient steep enough to carry it past the horizon leaves it with no z at
// all, grazing the surface, which is as far as a tilt can go
//
// This is the tilt the environment bump mappers apply. There the gradient
// displaces a lookup, and since an environment map is indexed by the normal's
// x and y with the z of that normal baked into what the map holds, displacing
// the lookup tilts the normal in exactly this way. Both paths therefore read a
// bump map at the same depth
//
// Both the perturbed normal and the light are unit, so the lambert term of the
// bump is their dot product.
//
float RETRO_BumpedLambert(float nx, float ny, float nz, float dhx, float dhy, float lightx, float lighty, float lightz)
{
	nx += dhx;
	ny += dhy;

	float radiusSquared = nx * nx + ny * ny;
	if (radiusSquared > 1.0f) {
		float inverseRadius = 1.0f / sqrt(radiusSquared);
		nx *= inverseRadius;
		ny *= inverseRadius;
		nz = 0.0f;
	} else {
		nz = copysign(sqrt(1.0f - radiusSquared), nz);
	}

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
void RETRO_DrawTexMapGouraudBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *bumpmap, int bumpheight, unsigned char *shadetable, float lightx, float lighty, float lightz)
{
	if (texmap == NULL || bumpmap == NULL || shadetable == NULL) return;

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
					float inverseQ = 1.0f / q;
					float u = uq * inverseQ;
					float v = vq * inverseQ;
					unsigned int textureU = CLAMP256(u);
					unsigned int textureV = CLAMP256(v);
					// A one-texel central difference gives a stable texture-space
					// height gradient. The sign makes bright texels protrude.
					int bu1 = CLAMP256(u + 1.0f) + textureV * 256;
					int bu2 = CLAMP256(u - 1.0f) + textureV * 256;
					int bv1 = textureU + CLAMP256(v + 1.0f) * 256;
					int bv2 = textureU + CLAMP256(v - 1.0f) * 256;
					float dhx = (float)(bumpmap[bu2] - bumpmap[bu1]) / bumpheight;
					float dhy = (float)(bumpmap[bv2] - bumpmap[bv1]) / bumpheight;
					// Interpolated normals must be normalized before lighting.
					float normalLengthSquared = nx * nx + ny * ny + nz * nz;
					float inverseNormalLength = normalLengthSquared > epsilon ? 1.0f / sqrt(normalLengthSquared) : 0.0f;
					float unitnx = nx * inverseNormalLength;
					float unitny = ny * inverseNormalLength;
					float unitnz = nz * inverseNormalLength;
					// The shade moves by as much as the tilt changes the lighting
					// here, so a flat patch of the bump map is left shaded exactly
					// as it was drawn without one.
					float lambert = unitnx * lightx + unitny * lighty + unitnz * lightz;
					float bumpedlambert = RETRO_BumpedLambert(unitnx, unitny, unitnz, dhx, dhy, lightx, lighty, lightz);
					float bumpshade = (RETRO_ShadeFromLambert(bumpedlambert) - RETRO_ShadeFromLambert(lambert)) * RETRO_SHADES;
					unsigned char texel = CLAMP(texmap[textureV * 256 + textureU], 0, RETRO_TEXTURE_COLORS);
					int shade = CLAMP128(c + bumpshade);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = shadetable[texel * RETRO_SHADES + shade];
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
void RETRO_DrawTexMapEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap, unsigned char *shadetable, unsigned char shade, bool lightingmap, int environmentCenter, int environmentIntensity)
{
	if (texmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;
	bool environmentShading = envmap != NULL;

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
		if (environmentShading) {
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
			float nx = environmentShading ? p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y) : 0.0f;
			float ny = environmentShading ? p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y) : 0.0f;
			float nz = environmentShading ? p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y) : 0.0f;
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseQ = 1.0f / q;
					unsigned int u = CLAMP256(uq * inverseQ);
					unsigned int v = CLAMP256(vq * inverseQ);
					unsigned char texel = CLAMP(texmap[v * 256 + u], 0, RETRO_TEXTURE_COLORS);
					unsigned char pixelShade = shade;
					if (environmentShading) {
						float e, w;
						RETRO_GetEnvironmentCoordinates(nx, ny, nz, lightingmap, environmentCenter, environmentIntensity, e, w);
						unsigned int environmentU = CLAMP256(e);
						unsigned int environmentV = CLAMP256(w);
						pixelShade = CLAMP128(envmap[environmentV * 256 + environmentU]);
					}
					RETRO.framebuffer[y * RETRO_WIDTH + x] = shadetable[texel * RETRO_SHADES + pixelShade];
				}
				uq += duqdx;
				vq += dvqdx;
				if (environmentShading) {
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
// The texture-space height gradient displaces the selected lighting or
// reflection lookup.
//
void RETRO_DrawTexMapEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap, unsigned char *bumpmap, int bumpheight, unsigned char *shadetable, bool lightingmap, int environmentCenter, int environmentIntensity)
{
	if (texmap == NULL || envmap == NULL || bumpmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

	// A height difference of RETRO_BUMP_HEIGHT tilts a normal all the way to
	// grazing, and grazing is the rim of the map, environmentIntensity away from
	// its center. That is what turns a height gradient into a lookup displacement
	float bumpScale = (float)environmentIntensity / bumpheight;

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
					float inverseQ = 1.0f / q;
					float u = uq * inverseQ;
					float v = vq * inverseQ;
					float e, w;
					RETRO_GetEnvironmentCoordinates(nx, ny, nz, lightingmap, environmentCenter, environmentIntensity, e, w);
					unsigned int textureU = CLAMP256(u);
					unsigned int textureV = CLAMP256(v);
					// A one-texel central difference gives a stable texture-space
					// height gradient. The sign makes bright texels protrude.
					int bu1 = CLAMP256(u + 1.0f) + textureV * 256;
					int bu2 = CLAMP256(u - 1.0f) + textureV * 256;
					int bv1 = textureU + CLAMP256(v + 1.0f) * 256;
					int bv2 = textureU + CLAMP256(v - 1.0f) * 256;
					int bu = (bumpmap[bu2] - bumpmap[bu1]) * bumpScale + e;
					int bv = (bumpmap[bv2] - bumpmap[bv1]) * bumpScale + w;
					// Outside the environment map, use the undisplaced lookup
					// instead of introducing black pixels.
					unsigned int environmentU = bu >= 0 && bu < 256 ? bu : CLAMP256(e);
					unsigned int environmentV = bv >= 0 && bv < 256 ? bv : CLAMP256(w);
					unsigned char texel = CLAMP(texmap[textureV * 256 + textureU], 0, RETRO_TEXTURE_COLORS);
					unsigned char pixelShade = CLAMP128(envmap[environmentV * 256 + environmentU]);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = shadetable[texel * RETRO_SHADES + pixelShade];
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
void RETRO_DrawEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap, bool lightingmap, int environmentCenter, int environmentIntensity, int envmapWidth = 256, int envmapHeight = 256)
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

		for (int y = ystart; y < yend; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float nx = p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y);
			float ny = p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y);
			float nz = p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				float e, w;
				RETRO_GetEnvironmentCoordinates(nx, ny, nz, lightingmap, environmentCenter, environmentIntensity, e, w);
				unsigned int environmentU = CLAMP(e, 0, envmapWidth - 1);
				unsigned int environmentV = CLAMP(w, 0, envmapHeight - 1);
				RETRO.framebuffer[y * RETRO_WIDTH + x] = envmap[environmentV * envmapWidth + environmentU];
				nx += dnxdx;
				ny += dnydx;
				nz += dnzdx;
			}
		}
	}
}

//
// Bump-mapped environment polygon
// The bump gradient displaces the selected lighting or reflection lookup.
//
void RETRO_DrawEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap, unsigned char *bumpmap, int bumpheight, bool lightingmap, int environmentCenter, int environmentIntensity)
{
	if (envmap == NULL || bumpmap == NULL) return;

	const float epsilon = 1.0e-12f;

	// A height difference of RETRO_BUMP_HEIGHT tilts a normal all the way to
	// grazing, and grazing is the rim of the map, environmentIntensity away from
	// its center. That is what turns a height gradient into a lookup displacement
	float bumpScale = (float)environmentIntensity / bumpheight;

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
					float inverseQ = 1.0f / q;
					float u = uq * inverseQ;
					float v = vq * inverseQ;
					float e, w;
					RETRO_GetEnvironmentCoordinates(nx, ny, nz, lightingmap, environmentCenter, environmentIntensity, e, w);
					unsigned int textureU = CLAMP256(u);
					unsigned int textureV = CLAMP256(v);

					// Sample the height gradient one texel away in each axis.
					int bu1 = CLAMP256(u + 1.0f) + textureV * 256;
					int bu2 = CLAMP256(u - 1.0f) + textureV * 256;
					int bv1 = textureU + CLAMP256(v + 1.0f) * 256;
					int bv2 = textureU + CLAMP256(v - 1.0f) * 256;
					int bu = (bumpmap[bu2] - bumpmap[bu1]) * bumpScale + e;
					int bv = (bumpmap[bv2] - bumpmap[bv1]) * bumpScale + w;
					unsigned int environmentU = bu >= 0 && bu < 256 ? bu : CLAMP256(e);
					unsigned int environmentV = bv >= 0 && bv < 256 ? bv : CLAMP256(w);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = envmap[environmentV * 256 + environmentU];
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

#endif
