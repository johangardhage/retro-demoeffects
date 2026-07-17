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
	float e, w;				// Environment map UV coordinates
	float nx, ny, nz;
};

struct LightSourcePoint {
	float nx, ny, nz, nn;	// Normal coordinates
	int c, cintensity;		// Min, max color
};

struct EdgeSpan {
	PolygonPoint p1;
	PolygonPoint p2;
};

void RETRO_ScanEdge(PolygonPoint p1, PolygonPoint p2, EdgeSpan *span)
{
	if (p2.y < p1.y) {
		SWAP(p1.x, p2.x);
		SWAP(p1.y, p2.y);
		SWAP(p1.c, p2.c);
		SWAP(p1.u, p2.u);
		SWAP(p1.v, p2.v);
		SWAP(p1.e, p2.e);
		SWAP(p1.w, p2.w);
		SWAP(p1.nx, p2.nx);
		SWAP(p1.ny, p2.ny);
		SWAP(p1.nz, p2.nz);
	}

	float ydiff = (p2.y - p1.y) != 0 ? p2.y - p1.y : 1;

	float dx = (p2.x - p1.x) / ydiff;
	float dc = (p2.c - p1.c) / ydiff;
	float du = (p2.u - p1.u) / ydiff;
	float dv = (p2.v - p1.v) / ydiff;
	float de = (p2.e - p1.e) / ydiff;
	float dw = (p2.w - p1.w) / ydiff;
	float dnx = (p2.nx - p1.nx) / ydiff;
	float dny = (p2.ny - p1.ny) / ydiff;
	float dnz = (p2.nz - p1.nz) / ydiff;

	float x = p1.x;
	float c = p1.c;
	float u = p1.u;
	float v = p1.v;
	float e = p1.e;
	float w = p1.w;
	float nx = p1.nx;
	float ny = p1.ny;
	float nz = p1.nz;

	for (int y = (int)p1.y; y < (int)p2.y; y++) {
		if (y >= 0 && y < RETRO_HEIGHT) {
			if (x < span[y].p1.x) {
				span[y].p1.x = x;
				span[y].p1.c = c;
				span[y].p1.u = u;
				span[y].p1.v = v;
				span[y].p1.e = e;
				span[y].p1.w = w;
				span[y].p1.nx = nx;
				span[y].p1.ny = ny;
				span[y].p1.nz = nz;
			}
			if (x > span[y].p2.x) {
				span[y].p2.x = x;
				span[y].p2.c = c;
				span[y].p2.u = u;
				span[y].p2.v = v;
				span[y].p2.e = e;
				span[y].p2.w = w;
				span[y].p2.nx = nx;
				span[y].p2.ny = ny;
				span[y].p2.nz = nz;
			}
		}
		x += dx;
		c += dc;
		u += du;
		v += dv;
		e += de;
		w += dw;
		nx += dnx;
		ny += dny;
		nz += dnz;
	}
}

//
// Flat shaded polygon
//
void RETRO_DrawFlatPolygon(PolygonPoint *vertices, int numvertices, unsigned char color)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
			if (x >= 0 && x < RETRO_WIDTH) {
				RETRO.framebuffer[y * RETRO_WIDTH + x] = color;
			}
		}
	}
}

//
// Glenz shaded polygon
//
void RETRO_DrawGlenzPolygon(PolygonPoint *vertices, int numvertices, unsigned char color)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
			if (x >= 0 && x < RETRO_WIDTH) {
				RETRO.framebuffer[y * RETRO_WIDTH + x] = MIN(RETRO.framebuffer[y * RETRO_WIDTH + x] + color, 255);
			}
		}
	}
}

//
// Gouraud shaded polygon
//
void RETRO_DrawGouraudPolygon(PolygonPoint *vertices, int numvertices)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? span[y].p2.x - span[y].p1.x : 1;
			float dc = (span[y].p2.c - span[y].p1.c) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					RETRO.framebuffer[y * RETRO_WIDTH + x] = span[y].p1.c;
				}
				span[y].p1.c += dc;
			}
		}
	}
}

//
// Phong shaded polygon
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
		float area = (p1->x - p0->x) * (p2->y - p0->y) - (p1->y - p0->y) * (p2->x - p0->x);
		if (fabs(area) <= epsilon) continue;

		EdgeSpan span[RETRO_HEIGHT];
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			span[y].p1.x = RETRO_WIDTH;
			span[y].p2.x = 0;
		}

		PolygonPoint *edgevertices[] = { p0, p1, p2, p0 };
		for (int edge = 0; edge < 3; edge++) {
			PolygonPoint a = *edgevertices[edge];
			PolygonPoint b = *edgevertices[edge + 1];
			if (b.y < a.y) SWAP(a, b);

			float ydiff = b.y - a.y;
			if (ydiff == 0.0f) continue;

			float dxdy = (b.x - a.x) / ydiff;
			int ystart = ceil(a.y - 0.5f);
			int yend = ceil(b.y - 0.5f);
			float x = a.x + ((ystart + 0.5f) - a.y) * dxdy;

			for (int y = ystart; y < yend; y++, x += dxdy) {
				if (y < 0 || y >= RETRO_HEIGHT) continue;
				span[y].p1.x = MIN(span[y].p1.x, x);
				span[y].p2.x = MAX(span[y].p2.x, x);
			}
		}

		float dnxdx, dnxdy, dnydx, dnydy, dnzdx, dnzdy;
		dnxdx = ((p1->nx - p0->nx) * (p2->y - p0->y) - (p2->nx - p0->nx) * (p1->y - p0->y)) / area;
		dnxdy = ((p1->x - p0->x) * (p2->nx - p0->nx) - (p2->x - p0->x) * (p1->nx - p0->nx)) / area;
		dnydx = ((p1->ny - p0->ny) * (p2->y - p0->y) - (p2->ny - p0->ny) * (p1->y - p0->y)) / area;
		dnydy = ((p1->x - p0->x) * (p2->ny - p0->ny) - (p2->x - p0->x) * (p1->ny - p0->ny)) / area;
		dnzdx = ((p1->nz - p0->nz) * (p2->y - p0->y) - (p2->nz - p0->nz) * (p1->y - p0->y)) / area;
		dnzdy = ((p1->x - p0->x) * (p2->nz - p0->nz) - (p2->x - p0->x) * (p1->nz - p0->nz)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float nx = p0->nx + dnxdx * (px - p0->x) + dnxdy * (py - p0->y);
			float ny = p0->ny + dnydx * (px - p0->x) + dnydy * (py - p0->y);
			float nz = p0->nz + dnzdx * (px - p0->x) + dnzdy * (py - p0->y);

			for (int x = xstart; x < xend; x++) {
				float normalLengthSquared = nx * nx + ny * ny + nz * nz;
				float intensity = 0.0f;

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
//
void RETRO_DrawTexMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? span[y].p2.x - span[y].p1.x : 1;
			float du = (span[y].p2.u - span[y].p1.u) / xdiff;
			float dv = (span[y].p2.v - span[y].p1.v) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					unsigned int u = CLAMP256(span[y].p1.u);
					unsigned int v = CLAMP256(span[y].p1.v);
					unsigned char texel = CLAMP256(texmap[v * 256 + u]);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = texel;
				}
				span[y].p1.u += du;
				span[y].p1.v += dv;
			}
		}
	}
}

//
// Texture mapped polygon
//
void RETRO_DrawTexMapGouraudPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *shadetable = NULL)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? span[y].p2.x - span[y].p1.x : 1;
			float du = (span[y].p2.u - span[y].p1.u) / xdiff;
			float dv = (span[y].p2.v - span[y].p1.v) / xdiff;
			float dc = (span[y].p2.c - span[y].p1.c) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					unsigned int u = CLAMP256(span[y].p1.u);
					unsigned int v = CLAMP256(span[y].p1.v);
					unsigned char texel = CLAMP(texmap[v * 256 + u], 0, RETRO_SHADE_COLORS);
					int shade = span[y].p1.c;
					RETRO.framebuffer[y * RETRO_WIDTH + x] = CLAMP256(shadetable[texel * 128 + shade]);
				}
				span[y].p1.u += du;
				span[y].p1.v += dv;
				span[y].p1.c += dc;
			}
		}
	}
}

//
// Texture mapped polygon
//
void RETRO_DrawTexMapEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap = NULL, unsigned char *shadetable = NULL, unsigned char shade = 0)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? span[y].p2.x - span[y].p1.x : 1;
			float du = (span[y].p2.u - span[y].p1.u) / xdiff;
			float dv = (span[y].p2.v - span[y].p1.v) / xdiff;
			float de = (span[y].p2.e - span[y].p1.e) / xdiff;
			float dw = (span[y].p2.w - span[y].p1.w) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					unsigned int u = CLAMP256(span[y].p1.u);
					unsigned int v = CLAMP256(span[y].p1.v);
					unsigned char texel = CLAMP(texmap[v * 256 + u], 0, RETRO_SHADE_COLORS);
					if (envmap != NULL) {
						unsigned int e = CLAMP256(span[y].p1.e);
						unsigned int w = CLAMP256(span[y].p1.w);
						shade = CLAMP128(envmap[w * 256 + e]);
					}
					RETRO.framebuffer[y * RETRO_WIDTH + x] = CLAMP256(shadetable[texel * 128 + shade]);
				}
				span[y].p1.u += du;
				span[y].p1.v += dv;
				span[y].p1.e += de;
				span[y].p1.w += dw;
			}
		}
	}
}

//
// Texture mapped polygon
//
void RETRO_DrawTexMapEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap = NULL, unsigned char *bumpmap = NULL, unsigned char *shadetable = NULL, unsigned char shade = 0)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	// Calculate constant gradients dudx, dvdx, dudy, dvdy across the polygon face
	float dudx = 0, dvdx = 0, dudy = 0, dvdy = 0;
	if (numvertices >= 3) {
		float x0 = vertices[0].x, y0 = vertices[0].y;
		float x1 = vertices[1].x, y1 = vertices[1].y;
		float x2 = vertices[2].x, y2 = vertices[2].y;
		float u0 = vertices[0].u, v0 = vertices[0].v;
		float u1 = vertices[1].u, v1 = vertices[1].v;
		float u2 = vertices[2].u, v2 = vertices[2].v;

		float D = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
		if (D != 0) {
			dudx = ((u1 - u0) * (y2 - y0) - (u2 - u0) * (y1 - y0)) / D;
			dudy = ((u2 - u0) * (x1 - x0) - (u1 - u0) * (x2 - x0)) / D;
			dvdx = ((v1 - v0) * (y2 - y0) - (v2 - v0) * (y1 - y0)) / D;
			dvdy = ((v2 - v0) * (x1 - x0) - (v1 - v0) * (x2 - x0)) / D;
		}
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? span[y].p2.x - span[y].p1.x : 1;
			float de = (span[y].p2.e - span[y].p1.e) / xdiff;
			float dw = (span[y].p2.w - span[y].p1.w) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					int bu1 = CLAMP256((span[y].p1.u + dudx)) + CLAMP256((span[y].p1.v + dvdx)) * 256;
					int bu2 = CLAMP256((span[y].p1.u - dudx)) + CLAMP256((span[y].p1.v - dvdx)) * 256;
					int bv1 = CLAMP256((span[y].p1.u + dudy)) + CLAMP256((span[y].p1.v + dvdy)) * 256;
					int bv2 = CLAMP256((span[y].p1.u - dudy)) + CLAMP256((span[y].p1.v - dvdy)) * 256;

					int bu = bumpmap[bu1] - bumpmap[bu2] + span[y].p1.e;
					int bv = bumpmap[bv1] - bumpmap[bv2] + span[y].p1.w;

					if (bu >= 0 && bu < 256 && bv >= 0 && bv < 256) {
						unsigned int u = CLAMP256(span[y].p1.u);
						unsigned int v = CLAMP256(span[y].p1.v);
						unsigned char texel = CLAMP(texmap[v * 256 + u], 0, RETRO_SHADE_COLORS);
						if (envmap != NULL) {
							shade = CLAMP128(envmap[bv * 256 + bu]);
						}
						RETRO.framebuffer[y * RETRO_WIDTH + x] = CLAMP256(shadetable[texel * 128 + shade]);
					} else {
						RETRO.framebuffer[y * RETRO_WIDTH + x] = 0;
					}
				}
				span[y].p1.u += dudx;
				span[y].p1.v += dvdx;
				span[y].p1.e += de;
				span[y].p1.w += dw;
			}
		}
	}
}

//
// Texture mapped polygon
//
void RETRO_DrawEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? span[y].p2.x - span[y].p1.x : 1;
			float de = (span[y].p2.e - span[y].p1.e) / xdiff;
			float dw = (span[y].p2.w - span[y].p1.w) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					unsigned int e = CLAMP256(span[y].p1.e);
					unsigned int w = CLAMP256(span[y].p1.w);
					unsigned char texel = CLAMP256(envmap[w * 256 + e]);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = texel;
				}
				span[y].p1.e += de;
				span[y].p1.w += dw;
			}
		}
	}
}

//
// Texture mapped polygon
//
void RETRO_DrawEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap, unsigned char *bumpmap)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].p1.x = RETRO_WIDTH;
		span[y].p2.x = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	// Calculate constant gradients dudx, dvdx, dudy, dvdy across the polygon face
	float dudx = 0, dvdx = 0, dudy = 0, dvdy = 0;
	if (numvertices >= 3) {
		float x0 = vertices[0].x, y0 = vertices[0].y;
		float x1 = vertices[1].x, y1 = vertices[1].y;
		float x2 = vertices[2].x, y2 = vertices[2].y;
		float u0 = vertices[0].u, v0 = vertices[0].v;
		float u1 = vertices[1].u, v1 = vertices[1].v;
		float u2 = vertices[2].u, v2 = vertices[2].v;

		float D = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
		if (D != 0) {
			dudx = ((u1 - u0) * (y2 - y0) - (u2 - u0) * (y1 - y0)) / D;
			dudy = ((u2 - u0) * (x1 - x0) - (u1 - u0) * (x2 - x0)) / D;
			dvdx = ((v1 - v0) * (y2 - y0) - (v2 - v0) * (y1 - y0)) / D;
			dvdy = ((v2 - v0) * (x1 - x0) - (v1 - v0) * (x2 - x0)) / D;
		}
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? (span[y].p2.x - span[y].p1.x) : 1;
			float de = (span[y].p2.e - span[y].p1.e) / xdiff;
			float dw = (span[y].p2.w - span[y].p1.w) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					int bu1 = CLAMP256((span[y].p1.u + dudx)) + CLAMP256((span[y].p1.v + dvdx)) * 256;
					int bu2 = CLAMP256((span[y].p1.u - dudx)) + CLAMP256((span[y].p1.v - dvdx)) * 256;
					int bv1 = CLAMP256((span[y].p1.u + dudy)) + CLAMP256((span[y].p1.v + dvdy)) * 256;
					int bv2 = CLAMP256((span[y].p1.u - dudy)) + CLAMP256((span[y].p1.v - dvdy)) * 256;

					int bu = bumpmap[bu1] - bumpmap[bu2] + span[y].p1.e;
					int bv = bumpmap[bv1] - bumpmap[bv2] + span[y].p1.w;

					if (bu >= 0 && bu < 256 && bv >= 0 && bv < 256) {
						unsigned char texel = CLAMP256(envmap[bv * 256 + bu]);
						RETRO.framebuffer[y * RETRO_WIDTH + x] = CLAMP256(texel);
					} else {
						RETRO.framebuffer[y * RETRO_WIDTH + x] = 0;
					}
				}
				span[y].p1.u += dudx;
				span[y].p1.v += dvdx;
				span[y].p1.e += de;
				span[y].p1.w += dw;
			}
		}
	}
}

#endif
