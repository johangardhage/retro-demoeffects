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
	const float epsilon = 1.0e-12f;

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

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
			for (int x = xstart; x < xend; x++) {
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
	const float epsilon = 1.0e-12f;

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

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
			for (int x = xstart; x < xend; x++) {
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
	const float epsilon = 1.0e-12f;

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

		float dcdx = ((p1->c - p0->c) * (p2->y - p0->y) - (p2->c - p0->c) * (p1->y - p0->y)) / area;
		float dcdy = ((p1->x - p0->x) * (p2->c - p0->c) - (p2->x - p0->x) * (p1->c - p0->c)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
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
	if (texmap == NULL) return;

	const float epsilon = 1.0e-12f;

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

		float u0 = p0->u * p0->q, u1 = p1->u * p1->q, u2 = p2->u * p2->q;
		float v0 = p0->v * p0->q, v1 = p1->v * p1->q, v2 = p2->v * p2->q;
		float duqdx = ((u1 - u0) * (p2->y - p0->y) - (u2 - u0) * (p1->y - p0->y)) / area;
		float duqdy = ((p1->x - p0->x) * (u2 - u0) - (p2->x - p0->x) * (u1 - u0)) / area;
		float dvqdx = ((v1 - v0) * (p2->y - p0->y) - (v2 - v0) * (p1->y - p0->y)) / area;
		float dvqdy = ((p1->x - p0->x) * (v2 - v0) - (p2->x - p0->x) * (v1 - v0)) / area;
		float dqdx = ((p1->q - p0->q) * (p2->y - p0->y) - (p2->q - p0->q) * (p1->y - p0->y)) / area;
		float dqdy = ((p1->x - p0->x) * (p2->q - p0->q) - (p2->x - p0->x) * (p1->q - p0->q)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
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
// Texture mapped polygon
//
void RETRO_DrawTexMapGouraudPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *shadetable = NULL)
{
	if (texmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

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
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
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
// Texture mapped polygon
//
void RETRO_DrawTexMapEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap = NULL, unsigned char *shadetable = NULL, unsigned char shade = 0)
{
	if (texmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

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
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
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
// Texture mapped polygon
//
void RETRO_DrawTexMapEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *texmap, unsigned char *envmap = NULL, unsigned char *bumpmap = NULL, unsigned char *shadetable = NULL, unsigned char shade = 0)
{
	if (texmap == NULL || bumpmap == NULL || shadetable == NULL) return;

	const float epsilon = 1.0e-12f;

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
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
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
					int bu1 = CLAMP256(u + 1.0f) + CLAMP256(v) * 256;
					int bu2 = CLAMP256(u - 1.0f) + CLAMP256(v) * 256;
					int bv1 = CLAMP256(u) + CLAMP256(v + 1.0f) * 256;
					int bv2 = CLAMP256(u) + CLAMP256(v - 1.0f) * 256;
					int bu = bumpmap[bu2] - bumpmap[bu1] + e;
					int bv = bumpmap[bv2] - bumpmap[bv1] + w;
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
// Texture mapped polygon
//
void RETRO_DrawEnvMapPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap)
{
	if (envmap == NULL) return;

	const float epsilon = 1.0e-12f;

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

		float dedx = ((p1->e - p0->e) * (p2->y - p0->y) - (p2->e - p0->e) * (p1->y - p0->y)) / area;
		float dedy = ((p1->x - p0->x) * (p2->e - p0->e) - (p2->x - p0->x) * (p1->e - p0->e)) / area;
		float dwdx = ((p1->w - p0->w) * (p2->y - p0->y) - (p2->w - p0->w) * (p1->y - p0->y)) / area;
		float dwdy = ((p1->x - p0->x) * (p2->w - p0->w) - (p2->x - p0->x) * (p1->w - p0->w)) / area;

		for (int y = 0; y < RETRO_HEIGHT; y++) {
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
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
// Texture mapped polygon
//
void RETRO_DrawEnvMapBumpPolygon(PolygonPoint *vertices, int numvertices, unsigned char *envmap, unsigned char *bumpmap)
{
	if (envmap == NULL || bumpmap == NULL) return;

	const float epsilon = 1.0e-12f;

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
			if (span[y].p1.x > span[y].p2.x) continue;
			int xstart = MAX((int)ceil(span[y].p1.x - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].p2.x - 0.5f), RETRO_WIDTH);
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
