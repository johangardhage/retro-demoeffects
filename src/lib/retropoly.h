//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROPOLY_H_
#define _RETROPOLY_H_

#include "retro.h"

typedef struct
{
	int x, y;
	int u, v;
	int nx, ny, nz;
	int c;
} PolygonPoint;

typedef struct
{
	float nx, ny, nz, nn;	// Normal coordinates
	int c, cintensity;		// Min, max color
} LightSourcePoint;

typedef struct
{
	float x1, x2;
	float c1, c2;
	float nx1, nx2;
	float ny1, ny2;
	float nz1, nz2;
	float u1, u2;
	float v1, v2;
} EdgeSpan;

void RETRO_ScanEdge(PolygonPoint p1, PolygonPoint p2, EdgeSpan *span)
{
	if (p2.y < p1.y) {
		SWAP(p1.x, p2.x);
		SWAP(p1.y, p2.y);
		SWAP(p1.c, p2.c);
		SWAP(p1.u, p2.u);
		SWAP(p1.v, p2.v);
		SWAP(p1.nx, p2.nx);
		SWAP(p1.ny, p2.ny);
		SWAP(p1.nz, p2.nz);
	}

	float ydiff = (p2.y - p1.y) != 0 ? p2.y - p1.y : 1;

	float mx = (p2.x - p1.x) / ydiff;
	float mc = (p2.c - p1.c) / ydiff;
	float mu = (p2.u - p1.u) / ydiff;
	float mv = (p2.v - p1.v) / ydiff;
	float mnx = (p2.nx - p1.nx) / ydiff;
	float mny = (p2.ny - p1.ny) / ydiff;
	float mnz = (p2.nz - p1.nz) / ydiff;

	float x = p1.x;
	float c = p1.c;
	float u = p1.u;
	float v = p1.v;
	float nx = p1.nx;
	float ny = p1.ny;
	float nz = p1.nz;

	for (int y = p1.y; y < p2.y; y++) {
		if (y >= 0 && y < RETRO_HEIGHT) {
			if (x < span[y].x1) {
				span[y].x1 = x;
				span[y].c1 = c;
				span[y].u1 = u;
				span[y].v1 = v;
				span[y].nx1 = nx;
				span[y].ny1 = ny;
				span[y].nz1 = nz;
			}
			if (x > span[y].x2) {
				span[y].x2 = x;
				span[y].c2 = c;
				span[y].u2 = u;
				span[y].v2 = v;
				span[y].nx2 = nx;
				span[y].ny2 = ny;
				span[y].nz2 = nz;
			}
		}
		x += mx;
		c += mc;
		u += mu;
		v += mv;
		nx += mnx;
		ny += mny;
		nz += mnz;
	}
}

//
// Flat shaded polygon
//
void RETRO_DrawFlatPolygon(PolygonPoint *vertices, int numvertices, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO_GetFrameBuffer();

	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < height; y++) {
		span[y].x1 = width;
		span[y].x2 = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < height; y++) {
		for (int x = (int)span[y].x1; x < (int)span[y].x2; x++) {
			if (x >= 0 && x < width) {
				buffer[y * width + x] = color;
			}
		}
	}
}

//
// Glenz shaded polygon
//
void RETRO_DrawGlenzPolygon(PolygonPoint *vertices, int numvertices, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO_GetFrameBuffer();

	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < height; y++) {
		span[y].x1 = width;
		span[y].x2 = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < height; y++) {
		for (int x = (int)span[y].x1; x < (int)span[y].x2; x++) {
			if (x >= 0 && x < width) {
				buffer[y * width + x] += color;
			}
		}
	}
}

//
// Gouraud shaded polygon
//
void RETRO_DrawGouraudPolygon(PolygonPoint *vertices, int numvertices, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO_GetFrameBuffer();

	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < height; y++) {
		span[y].x1 = width;
		span[y].x2 = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < height; y++) {
		if (span[y].x1 <= span[y].x2) {
			float xdiff = (span[y].x2 - span[y].x1) != 0 ? span[y].x2 - span[y].x1 : 1;
			float mc = (span[y].c2 - span[y].c1) / xdiff;

			for (int x = (int)span[y].x1; x < (int)span[y].x2; x++) {
				if (x >= 0 && x < width) {
					buffer[y * width + x] = span[y].c1;
				}
				span[y].c1 += mc;
			}
		}
	}
}

//
// Phong shaded polygon
//
void RETRO_DrawPhongPolygon(PolygonPoint *vertices, int numvertices, LightSourcePoint light, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO_GetFrameBuffer();

	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < height; y++) {
		span[y].x1 = width;
		span[y].x2 = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < height; y++) {
		if (span[y].x1 <= span[y].x2) {
			float xdiff = (span[y].x2 - span[y].x1) != 0 ? span[y].x2 - span[y].x1 : 1;
			float mnx = (span[y].nx2 - span[y].nx1) / xdiff;
			float mny = (span[y].ny2 - span[y].ny1) / xdiff;
			float mnz = (span[y].nz2 - span[y].nz1) / xdiff;

			for (int x = (int)span[y].x1; x < (int)span[y].x2; x++) {
				if (x >= 0 && x < width) {
					float angle = (span[y].nx1 * light.nx + span[y].ny1 * light.ny + span[y].nz1 * light.nz) / (sqrt(span[y].nx1 * span[y].nx1 + span[y].ny1 * span[y].ny1 + span[y].nz1 * span[y].nz1) * light.nn);
					buffer[y * width + x] = CLAMP(light.cintensity * angle, light.c, light.cintensity);
				}
				span[y].nx1 += mnx;
				span[y].ny1 += mny;
				span[y].nz1 += mnz;
			}
		}
	}
}

//
// Texture mapped polygon
//
void RETRO_DrawTexturePolygon(PolygonPoint *vertices, int numvertices, unsigned char *image, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO_GetFrameBuffer();

	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < height; y++) {
		span[y].x1 = width;
		span[y].x2 = 0;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdge(*p1, *p2, span);
	}

	for (int y = 0; y < height; y++) {
		if (span[y].x1 <= span[y].x2) {
			float xdiff = (span[y].x2 - span[y].x1) != 0 ? span[y].x2 - span[y].x1 : 1;
			float mu = (span[y].u2 - span[y].u1) / xdiff;
			float mv = (span[y].v2 - span[y].v1) / xdiff;

			for (int x = span[y].x1; x < span[y].x2; x++) {
				if (x >= 0 && x < width) {
					int u = CLAMP128(span[y].u1);
					int v = CLAMP128(span[y].v1);
					buffer[y * width + x] = image[v * 128 + u];
				}
				span[y].u1 += mu;
				span[y].v1 += mv;
			}
		}
	}
}

#endif
