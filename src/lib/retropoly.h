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
	float e, w;				// Environment map UV coordinates
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
// Find the horizontal coverage of a triangle at pixel centers.
// The returned signed area is also used to calculate attribute gradients.
//
float RETRO_ScanTriangle(const PolygonPoint *p0, const PolygonPoint *p1, const PolygonPoint *p2, TriangleSpan *span)
{
	const float epsilon = 1.0e-12f;
	float area = (p1->x - p0->x) * (p2->y - p0->y) - (p1->y - p0->y) * (p2->x - p0->x);
	if (fabs(area) <= epsilon) return 0.0f;

	for (int y = 0; y < RETRO_HEIGHT; y++) {
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
		int ystart = ceil(a->y - 0.5f);
		int yend = ceil(b->y - 0.5f);
		float x = a->x + ((ystart + 0.5f) - a->y) * dxdy;

		for (int y = ystart; y < yend; y++, x += dxdy) {
			if (y < 0 || y >= RETRO_HEIGHT) continue;
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
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
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
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
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
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		float dcdx = ((p1->c - p0->c) * (p2->y - p0->y) - (p2->c - p0->c) * (p1->y - p0->y)) / area;
		float dcdy = ((p1->x - p0->x) * (p2->c - p0->c) - (p2->x - p0->x) * (p1->c - p0->c)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
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
	int cmax = light.c + light.cintensity;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		float dnxdx, dnxdy, dnydx, dnydy, dnzdx, dnzdy;
		dnxdx = ((p1->nx - p0->nx) * (p2->y - p0->y) - (p2->nx - p0->nx) * (p1->y - p0->y)) / area;
		dnxdy = ((p1->x - p0->x) * (p2->nx - p0->nx) - (p2->x - p0->x) * (p1->nx - p0->nx)) / area;
		dnydx = ((p1->ny - p0->ny) * (p2->y - p0->y) - (p2->ny - p0->ny) * (p1->y - p0->y)) / area;
		dnydy = ((p1->x - p0->x) * (p2->ny - p0->ny) - (p2->x - p0->x) * (p1->ny - p0->ny)) / area;
		dnzdx = ((p1->nz - p0->nz) * (p2->y - p0->y) - (p2->nz - p0->nz) * (p1->y - p0->y)) / area;
		dnzdy = ((p1->x - p0->x) * (p2->nz - p0->nz) - (p2->x - p0->x) * (p1->nz - p0->nz)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
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

				float paletteIntensity = asin(MIN(intensity, 1.0f)) / (M_PI / 2);
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
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
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
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
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

		for (int y = 0; y < RETRO_HEIGHT; y++) {
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
					unsigned char texel = CLAMP(texmap[v * 256 + u], 0, RETRO_SHADE_COLORS);
					int shade = CLAMP128(c);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = shadetable[texel * 128 + shade];
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
// Environment shaded texture mapped polygon
// Texture coordinates are perspective-correct. Environment coordinates are
// affine because they are derived from view-space normals rather than geometry.
//
void RETRO_DrawTexMapEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap = NULL, unsigned char *shadetable = NULL, unsigned char shade = 0)
{
	if (texmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dedx = ((p1->e - p0->e) * (p2->y - p0->y) - (p2->e - p0->e) * (p1->y - p0->y)) / area;
		float dedy = ((p1->x - p0->x) * (p2->e - p0->e) - (p2->x - p0->x) * (p1->e - p0->e)) / area;
		float dwdx = ((p1->w - p0->w) * (p2->y - p0->y) - (p2->w - p0->w) * (p1->y - p0->y)) / area;
		float dwdy = ((p1->x - p0->x) * (p2->w - p0->w) - (p2->x - p0->x) * (p1->w - p0->w)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float e = p0->e + dedx * (px - p0->x) + dedy * (py - p0->y);
			float w = p0->w + dwdx * (px - p0->x) + dwdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseQ = 1.0f / q;
					unsigned int u = CLAMP256(uq * inverseQ);
					unsigned int v = CLAMP256(vq * inverseQ);
					unsigned char texel = CLAMP(texmap[v * 256 + u], 0, RETRO_SHADE_COLORS);
					unsigned char pixelShade = shade;
					if (envmap != NULL) {
						unsigned int environmentU = CLAMP256(e);
						unsigned int environmentV = CLAMP256(w);
						pixelShade = CLAMP128(envmap[environmentV * 256 + environmentU]);
					}
					RETRO.framebuffer[y * RETRO_WIDTH + x] = shadetable[texel * 128 + pixelShade];
				}
				uq += duqdx;
				vq += dvqdx;
				e += dedx;
				w += dwdx;
				q += dqdx;
			}
		}
	}
}

//
// Bump and environment shaded texture mapped polygon
// Use perspective-correct coordinates for the texture and bump map, while
// environment coordinates remain affine.
//
void RETRO_DrawTexMapEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap = NULL, unsigned char *bumpmap = NULL, unsigned char *shadetable = NULL, unsigned char shade = 0)
{
	if (texmap == NULL || bumpmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dedx = ((p1->e - p0->e) * (p2->y - p0->y) - (p2->e - p0->e) * (p1->y - p0->y)) / area;
		float dedy = ((p1->x - p0->x) * (p2->e - p0->e) - (p2->x - p0->x) * (p1->e - p0->e)) / area;
		float dwdx = ((p1->w - p0->w) * (p2->y - p0->y) - (p2->w - p0->w) * (p1->y - p0->y)) / area;
		float dwdy = ((p1->x - p0->x) * (p2->w - p0->w) - (p2->x - p0->x) * (p1->w - p0->w)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float e = p0->e + dedx * (px - p0->x) + dedy * (py - p0->y);
			float w = p0->w + dwdx * (px - p0->x) + dwdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseQ = 1.0f / q;
					float u = uq * inverseQ;
					float v = vq * inverseQ;
					// A one-texel central difference gives a stable texture-space
					// height gradient. The sign makes bright texels protrude.
					int bu1 = CLAMP256(u + 1.0f) + CLAMP256(v) * 256;
					int bu2 = CLAMP256(u - 1.0f) + CLAMP256(v) * 256;
					int bv1 = CLAMP256(u) + CLAMP256(v + 1.0f) * 256;
					int bv2 = CLAMP256(u) + CLAMP256(v - 1.0f) * 256;
					int bu = bumpmap[bu2] - bumpmap[bu1] + e;
					int bv = bumpmap[bv2] - bumpmap[bv1] + w;
					// Outside the environment map, use the undisplaced lookup
					// instead of introducing black pixels.
					unsigned int environmentU = bu >= 0 && bu < 256 ? bu : CLAMP256(e);
					unsigned int environmentV = bv >= 0 && bv < 256 ? bv : CLAMP256(w);
					unsigned int textureU = CLAMP256(u);
					unsigned int textureV = CLAMP256(v);
					unsigned char texel = CLAMP(texmap[textureV * 256 + textureU], 0, RETRO_SHADE_COLORS);
					unsigned char pixelShade = shade;
					if (envmap != NULL) {
						pixelShade = CLAMP128(envmap[environmentV * 256 + environmentU]);
					}
					RETRO.framebuffer[y * RETRO_WIDTH + x] = shadetable[texel * 128 + pixelShade];
				}
				uq += duqdx;
				vq += dvqdx;
				e += dedx;
				w += dwdx;
				q += dqdx;
			}
		}
	}
}

//
// Environment mapped polygon
// Environment coordinates are interpolated affinely in screen space.
//
void RETRO_DrawEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap)
{
	if (envmap == NULL) return;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		float dedx = ((p1->e - p0->e) * (p2->y - p0->y) - (p2->e - p0->e) * (p1->y - p0->y)) / area;
		float dedy = ((p1->x - p0->x) * (p2->e - p0->e) - (p2->x - p0->x) * (p1->e - p0->e)) / area;
		float dwdx = ((p1->w - p0->w) * (p2->y - p0->y) - (p2->w - p0->w) * (p1->y - p0->y)) / area;
		float dwdy = ((p1->x - p0->x) * (p2->w - p0->w) - (p2->x - p0->x) * (p1->w - p0->w)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float e = p0->e + dedx * (px - p0->x) + dedy * (py - p0->y);
			float w = p0->w + dwdx * (px - p0->x) + dwdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				unsigned int environmentU = CLAMP256(e);
				unsigned int environmentV = CLAMP256(w);
				RETRO.framebuffer[y * RETRO_WIDTH + x] = envmap[environmentV * 256 + environmentU];
				e += dedx;
				w += dwdx;
			}
		}
	}
}

//
// Bump-mapped environment polygon
// The bump gradient displaces an affine environment-map lookup.
//
void RETRO_DrawEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap, unsigned char *bumpmap)
{
	if (envmap == NULL || bumpmap == NULL) return;

	const float epsilon = 1.0e-12f;

	for (int triangle = 1; triangle < numvertices - 1; triangle++) {
		PolygonPoint *p0 = &vertices[0];
		PolygonPoint *p1 = &vertices[triangle];
		PolygonPoint *p2 = &vertices[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		float area = RETRO_ScanTriangle(p0, p1, p2, span);
		if (area == 0.0f) continue;

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;
		float dedx = ((p1->e - p0->e) * (p2->y - p0->y) - (p2->e - p0->e) * (p1->y - p0->y)) / area;
		float dedy = ((p1->x - p0->x) * (p2->e - p0->e) - (p2->x - p0->x) * (p1->e - p0->e)) / area;
		float dwdx = ((p1->w - p0->w) * (p2->y - p0->y) - (p2->w - p0->w) * (p1->y - p0->y)) / area;
		float dwdy = ((p1->x - p0->x) * (p2->w - p0->w) - (p2->x - p0->x) * (p1->w - p0->w)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].x1 > span[y].x2) continue;
			int xstart = MAX((int)ceil(span[y].x1 - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].x2 - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float uq = u0 + duqdx * (px - p0->x) + duqdy * (py - p0->y);
			float vq = v0 + dvqdx * (px - p0->x) + dvqdy * (py - p0->y);
			float q = p0->q + dqdx * (px - p0->x) + dqdy * (py - p0->y);
			float e = p0->e + dedx * (px - p0->x) + dedy * (py - p0->y);
			float w = p0->w + dwdx * (px - p0->x) + dwdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				if (fabs(q) > epsilon) {
					float inverseQ = 1.0f / q;
					float u = uq * inverseQ;
					float v = vq * inverseQ;

					// Sample the height gradient one texel away in each axis.
					int bu1 = CLAMP256(u + 1.0f) + CLAMP256(v) * 256;
					int bu2 = CLAMP256(u - 1.0f) + CLAMP256(v) * 256;
					int bv1 = CLAMP256(u) + CLAMP256(v + 1.0f) * 256;
					int bv2 = CLAMP256(u) + CLAMP256(v - 1.0f) * 256;
					int bu = bumpmap[bu2] - bumpmap[bu1] + e;
					int bv = bumpmap[bv2] - bumpmap[bv1] + w;
					unsigned int environmentU = bu >= 0 && bu < 256 ? bu : CLAMP256(e);
					unsigned int environmentV = bv >= 0 && bv < 256 ? bv : CLAMP256(w);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = envmap[environmentV * 256 + environmentU];
				}
				uq += duqdx;
				vq += dvqdx;
				q += dqdx;
				e += dedx;
				w += dwdx;
			}
		}
	}
}

#endif
