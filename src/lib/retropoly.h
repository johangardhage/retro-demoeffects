//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROPOLY_H_
#define _RETROPOLY_H_

#include "retro.h"

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
				RETRO.framebuffer[y * RETRO_WIDTH + x] += color;
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
			float dnx = (span[y].p2.nx - span[y].p1.nx) / xdiff;
			float dny = (span[y].p2.ny - span[y].p1.ny) / xdiff;
			float dnz = (span[y].p2.nz - span[y].p1.nz) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					float angle = (span[y].p1.nx * light.nx + span[y].p1.ny * light.ny + span[y].p1.nz * light.nz) /
								  (sqrt(span[y].p1.nx * span[y].p1.nx + span[y].p1.ny * span[y].p1.ny + span[y].p1.nz * span[y].p1.nz) * light.nn);
					RETRO.framebuffer[y * RETRO_WIDTH + x] = CLAMP(light.cintensity * angle, light.c, light.cintensity);
				}
				span[y].p1.nx += dnx;
				span[y].p1.ny += dny;
				span[y].p1.nz += dnz;
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
					unsigned char texel = CLAMP128(texmap[v * 256 + u]);
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
					unsigned char texel = CLAMP128(texmap[v * 256 + u]);
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

	// Calculate ydiff
	int ymin = RETRO_HEIGHT;
	int ymax = 0;
	for (int i = 0; i < numvertices; i++) {
		ymin = vertices[i].y < ymin ? vertices[i].y : ymin;
		ymax = vertices[i].y > ymax ? vertices[i].y : ymax;
	}
	float ydiff = (ymax - ymin) != 0 ? ymax - ymin : 1;

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? span[y].p2.x - span[y].p1.x : 1;
			float dudx = (span[y].p2.u - span[y].p1.u) / xdiff;
			float dvdx = (span[y].p2.v - span[y].p1.v) / xdiff;
			float dudy = (span[y].p2.u - span[y].p1.u) / ydiff;
			float dvdy = (span[y].p2.v - span[y].p1.v) / ydiff;
			float de = (span[y].p2.e - span[y].p1.e) / xdiff;
			float dw = (span[y].p2.w - span[y].p1.w) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					int bu1 = CLAMP256((span[y].p1.u + dudx)) + CLAMP256((span[y].p1.v + dvdx)) * 256;
					int bu2 = CLAMP256((span[y].p1.u - dudx)) + CLAMP256((span[y].p1.v - dvdx)) * 256;
					int bv1 = CLAMP256((span[y].p1.u + dudy)) + CLAMP256((span[y].p1.v + dvdy)) * 256;
					int bv2 = CLAMP256((span[y].p1.u - dudy)) + CLAMP256((span[y].p1.v - dvdy)) * 256;

					int bu = CLAMP256(bumpmap[bu1] - bumpmap[bu2] + span[y].p1.e);
					int bv = CLAMP256(bumpmap[bv1] - bumpmap[bv2] + span[y].p1.w);

					if (bu >= 0 && bu < 256 && bv >= 0 && bv < 256) {
						unsigned int u = CLAMP256(span[y].p1.u);
						unsigned int v = CLAMP256(span[y].p1.v);
						unsigned char texel = CLAMP128(texmap[v * 256 + u]);
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

	// Calculate ydiff
	int ymin = RETRO_HEIGHT;
	int ymax = 0;
	for (int i = 0; i < numvertices; i++) {
		ymin = vertices[i].y < ymin ? vertices[i].y : ymin;
		ymax = vertices[i].y > ymax ? vertices[i].y : ymax;
	}
	float ydiff = (ymax - ymin) != 0 ? ymax - ymin : 1;

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].p1.x <= span[y].p2.x) {
			float xdiff = (span[y].p2.x - span[y].p1.x) != 0 ? (span[y].p2.x - span[y].p1.x) : 1;
			float dudx = (span[y].p2.u - span[y].p1.u) / xdiff;
			float dvdx = (span[y].p2.v - span[y].p1.v) / xdiff;
			float dudy = (span[y].p2.u - span[y].p1.u) / ydiff;
			float dvdy = (span[y].p2.v - span[y].p1.v) / ydiff;
			float de = (span[y].p2.e - span[y].p1.e) / xdiff;
			float dw = (span[y].p2.w - span[y].p1.w) / xdiff;

			for (int x = (int)span[y].p1.x; x < (int)span[y].p2.x; x++) {
				if (x >= 0 && x < RETRO_WIDTH) {
					int bu1 = CLAMP256((span[y].p1.u + dudx)) + CLAMP256((span[y].p1.v + dvdx)) * 256;
					int bu2 = CLAMP256((span[y].p1.u - dudx)) + CLAMP256((span[y].p1.v - dvdx)) * 256;
					int bv1 = CLAMP256((span[y].p1.u + dudy)) + CLAMP256((span[y].p1.v + dvdy)) * 256;
					int bv2 = CLAMP256((span[y].p1.u - dudy)) + CLAMP256((span[y].p1.v - dvdy)) * 256;

					int bu = CLAMP256(bumpmap[bu1] - bumpmap[bu2] + span[y].p1.e);
					int bv = CLAMP256(bumpmap[bv1] - bumpmap[bv2] + span[y].p1.w);

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
