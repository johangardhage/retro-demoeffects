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
	int nx, ny, nz;			// Normal coordinates
	float nn;				// Length
	int c, cintensity;		// Min, max color
} LightSourcePoint;

typedef struct
{
	int x1, x2;
	long c1, c2;
	long nx1, nx2;
	long ny1, ny2;
	long nz1, nz2;
	long u1, u2;
	long v1, v2;
} EdgeSpan;

//
// Flat shaded polygon
//

void RETRO_DrawEdgeFlat(int y, EdgeSpan span, unsigned char color)
{
	if (span.x2 < span.x1) {
		SWAP(span.x1, span.x2);
	}

	int xdiff = span.x2 - span.x1;
	if (xdiff > 0) {
		unsigned char *buffer = RETRO_GetFrameBuffer();

		for (int x = span.x1; x < span.x2; x++) {
			buffer[y * RETRO_WIDTH + x] = color;
		}
	}
}

void RETRO_ScanEdgeFlat(PolygonPoint p1, PolygonPoint p2, EdgeSpan *span)
{
	if (p2.y == p1.y) {
		return;
	}

	if (p2.y < p1.y) {
		SWAP(p1.y, p2.y);
		SWAP(p1.x, p2.x);
	}

	int ydiff = p2.y - p1.y;
	int xdiff = p2.x - p1.x;
	long x = p1.x << 8;
	long xstep = (xdiff << 8) / ydiff;

	p1.y++;
	x += xstep;

	for (int y = p1.y; y <= p2.y; y++) {
		if (span[y].x1 == INT_MIN) {
			span[y].x1 = x >> 8;
		} else {
			span[y].x2 = x >> 8;
		}
		x += xstep;
	}
}

void RETRO_DrawFlatPolygon(PolygonPoint *vertices, int numvertex, unsigned char color)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].x1 = INT_MIN;
		span[y].x2 = INT_MIN;
	}

	for (int i = 0; i < numvertex; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertex;
		RETRO_ScanEdgeFlat(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].x1 != INT_MIN && span[y].x2 != INT_MIN) {
			RETRO_DrawEdgeFlat(y, span[y], color);
		}
	}
}

//
// Glenz polygon
//

void RETRO_DrawEdgeGlenz(int y, EdgeSpan span, unsigned char color)
{
	if (span.x2 < span.x1) {
		SWAP(span.x1, span.x2);
	}

	int xdiff = span.x2 - span.x1;
	if (xdiff > 0) {
		unsigned char *buffer = RETRO_GetFrameBuffer();

		for (int x = span.x1; x < span.x2; x++) {
			buffer[y * RETRO_WIDTH + x] += color;
		}
	}
}

void RETRO_ScanEdgeGlenz(PolygonPoint p1, PolygonPoint p2, EdgeSpan *span)
{
	if (p2.y == p1.y) {
		return;
	}

	if (p2.y < p1.y) {
		SWAP(p1.y, p2.y);
		SWAP(p1.x, p2.x);
	}

	int ydiff = p2.y - p1.y;
	int xdiff = p2.x - p1.x;
	long x = p1.x << 8;
	long xstep = (xdiff << 8) / ydiff;

	p1.y++;
	x += xstep;

	for (int y = p1.y; y <= p2.y; y++) {
		if (span[y].x1 == INT_MIN) {
			span[y].x1 = x >> 8;
		} else {
			span[y].x2 = x >> 8;
		}
		x += xstep;
	}
}

void RETRO_DrawGlenzPolygon(PolygonPoint *vertices, int numvertex, unsigned char color)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].x1 = INT_MIN;
		span[y].x2 = INT_MIN;
	}

	for (int i = 0; i < numvertex; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertex;
		RETRO_ScanEdgeGlenz(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].x1 != INT_MIN && span[y].x2 != INT_MIN) {
			RETRO_DrawEdgeGlenz(y, span[y], color);
		}
	}
}

//
// Gouraud shading polygon
//

void RETRO_DrawEdgeGouraud(int y, EdgeSpan span)
{
	if (span.x2 < span.x1) {
		SWAP(span.x1, span.x2);
		SWAP(span.c1, span.c2);
	}

	long xdiff = span.x2 - span.x1;
	if (xdiff > 0) {
		long cdiff = span.c2 - span.c1;
		long cstep = cdiff / xdiff;

		unsigned char *buffer = RETRO_GetFrameBuffer();

		for (long x = span.x1; x < span.x2; x++) {
			buffer[y * RETRO_WIDTH + x] = span.c1 >> 8;
			span.c1 += cstep;
		}
	}
}

void RETRO_ScanEdgeGouraud(PolygonPoint p1, PolygonPoint p2, EdgeSpan *span)
{
	if (p2.y == p1.y) {
		return;
	}

	if (p2.y < p1.y) {
		SWAP(p1.y, p2.y);
		SWAP(p1.x, p2.x);
		SWAP(p1.c, p2.c);
	}

	int ydiff = p2.y - p1.y;
	int xdiff = p2.x - p1.x;
	int cdiff = p2.c - p1.c;
	long x = p1.x << 8;
	long c = p1.c << 8;
	long xstep = (xdiff << 8) / ydiff;
	long cstep = (cdiff << 8) / ydiff;

	p1.y++;
	x += xstep;
	c += cstep;

	for (int y = p1.y; y <= p2.y; y++) {
		if (span[y].x1 == INT_MIN) {
			span[y].x1 = x >> 8;
			span[y].c1 = c;
		} else {
			span[y].x2 = x >> 8;
			span[y].c2 = c;
		}
		x += xstep;
		c += cstep;
	}
}

void RETRO_DrawGouraudPolygon(PolygonPoint *vertices, int numvertices)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].x1 = INT_MIN;
		span[y].x2 = INT_MIN;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdgeGouraud(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].x1 != INT_MIN && span[y].x2 != INT_MIN) {
			RETRO_DrawEdgeGouraud(y, span[y]);
		}
	}
}

//
// Phong shading polygon
//

void RETRO_DrawEdgePhong(int y, EdgeSpan span, LightSourcePoint light)
{
	if (span.x2 < span.x1) {
		SWAP(span.x1, span.x2);
		SWAP(span.nx1, span.nx2);
		SWAP(span.ny1, span.ny2);
		SWAP(span.nz1, span.nz2);
	}

	int xdiff = span.x2 - span.x1;
	if (xdiff > 0) {
		long nxdiff = span.nx2 - span.nx1;
		long nydiff = span.ny2 - span.ny1;
		long nzdiff = span.nz2 - span.nz1;
		long nxstep = nxdiff / xdiff;
		long nystep = nydiff / xdiff;
		long nzstep = nzdiff / xdiff;

		unsigned char *buffer = RETRO_GetFrameBuffer();

		for (long x = span.x1; x < span.x2; x++) {
			long angle = ((span.nx1 * light.nx + span.ny1 * light.ny + span.nz1 * light.nz) << 8) / (sqrt(span.nx1 * span.nx1 + span.ny1 * span.ny1 + span.nz1 * span.nz1) * light.nn);
			buffer[y * RETRO_WIDTH + x] = CLAMP(light.cintensity * angle >> 8, light.c, light.cintensity);
			span.nx1 += nxstep;
			span.ny1 += nystep;
			span.nz1 += nzstep;
		}
	}
}

void RETRO_ScanEdgePhong(PolygonPoint p1, PolygonPoint p2, EdgeSpan *span)
{
	if (p2.y == p1.y) {
		return;
	}

	if (p2.y < p1.y) {
		SWAP(p1.y, p2.y);
		SWAP(p1.x, p2.x);
		SWAP(p1.nx, p2.nx);
		SWAP(p1.ny, p2.ny);
		SWAP(p1.nz, p2.nz);
	}

	int ydiff = p2.y - p1.y;
	int xdiff = p2.x - p1.x;
	int nxdiff = p2.nx - p1.nx;
	int nydiff = p2.ny - p1.ny;
	int nzdiff = p2.nz - p1.nz;
	long xstep = (xdiff << 8) / ydiff;
	long nxstep = (nxdiff << 8) / ydiff;
	long nystep = (nydiff << 8) / ydiff;
	long nzstep = (nzdiff << 8) / ydiff;
	long x = p1.x << 8;
	long nx = p1.nx << 8;
	long ny = p1.ny << 8;
	long nz = p1.nz << 8;

	p1.y++;
	x += xstep;
	nx += nxstep;
	ny += nystep;
	nz += nzstep;

	for (int y = p1.y; y <= p2.y; y++) {
		if (span[y].x1 == INT_MIN) {
			span[y].x1 = x >> 8;
			span[y].nx1 = nx;
			span[y].ny1 = ny;
			span[y].nz1 = nz;
		} else {
			span[y].x2 = x >> 8;
			span[y].nx2 = nx;
			span[y].ny2 = ny;
			span[y].nz2 = nz;
		}
		x += xstep;
		nx += nxstep;
		ny += nystep;
		nz += nzstep;
	}
}

void RETRO_DrawPhongPolygon(PolygonPoint *vertices, int numvertices, LightSourcePoint light)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].x1 = INT_MIN;
		span[y].x2 = INT_MIN;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdgePhong(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].x1 != INT_MIN && span[y].x2 != INT_MIN) {
			RETRO_DrawEdgePhong(y, span[y], light);
		}
	}
}

//
// Texture mapping polygon
//

void RETRO_DrawEdgeTexture(int y, EdgeSpan span, unsigned char *image)
{
	if (span.x2 < span.x1) {
		SWAP(span.x1, span.x2);
		SWAP(span.u1, span.u2);
		SWAP(span.v1, span.v2);
	}

	int xdiff = span.x2 - span.x1;
	if (xdiff > 0) {
		long udiff = span.u2 - span.u1;
		long vdiff = span.v2 - span.v1;
		long ustep = udiff / xdiff;
		long vstep = vdiff / xdiff;

		unsigned char *buffer = RETRO_GetFrameBuffer();

		for (int x = span.x1; x <= span.x2; x++) {
			buffer[y * RETRO_WIDTH + x] = image[(span.v1 >> 8) * 128 + (span.u1 >> 8)];
			span.u1 += ustep;
			span.v1 += vstep;
		}
	}
}

void RETRO_ScanEdgeTexture(PolygonPoint p1, PolygonPoint p2, EdgeSpan *span)
{
	if (p2.y == p1.y) {
		return;
	}

	if (p2.y < p1.y) {
		SWAP(p1.y, p2.y);
		SWAP(p1.x, p2.x);
		SWAP(p1.u, p2.u);
		SWAP(p1.v, p2.v);
	}

	int ydiff = p2.y - p1.y;
	int xdiff = p2.x - p1.x;
	int udiff = p2.u - p1.u;
	int vdiff = p2.v - p1.v;
	long x = p1.x << 8;
	long u = p1.u << 8;
	long v = p1.v << 8;
	long xstep = (xdiff << 8) / ydiff;
	long ustep = (udiff << 8) / ydiff;
	long vstep = (vdiff << 8) / ydiff;

	p1.y++;
	x += xstep;
	u += ustep;
	v += vstep;

	for (int y = p1.y; y <= p2.y; y++) {
		if (span[y].x1 == INT_MIN) {
			span[y].x1 = x >> 8;
			span[y].u1 = u;
			span[y].v1 = v;
		} else {
			span[y].x2 = x >> 8;
			span[y].u2 = u;
			span[y].v2 = v;
		}
		x += xstep;
		u += ustep;
		v += vstep;
	}
}

void RETRO_DrawTexturePolygon(PolygonPoint *vertices, int numvertices, unsigned char *image)
{
	EdgeSpan span[RETRO_HEIGHT];

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		span[y].x1 = INT_MIN;
		span[y].x2 = INT_MIN;
	}

	for (int i = 0; i < numvertices; i++) {
		PolygonPoint *p1 = vertices + i;
		PolygonPoint *p2 = vertices + (i + 1) % numvertices;
		RETRO_ScanEdgeTexture(*p1, *p2, span);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (span[y].x1 != INT_MIN && span[y].x2 != INT_MIN) {
			RETRO_DrawEdgeTexture(y, span[y], image);
		}
	}
}

#endif
