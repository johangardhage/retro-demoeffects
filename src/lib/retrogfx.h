//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROGFX_H_
#define _RETROGFX_H_

#include "retro.h"

enum RETRO_BLUR_PATTERN {
	RETRO_BLUR_VERTICAL,	// 3 taps in a vertical line, softens into slight vertical streaks
	RETRO_BLUR_DIFFUSE,	// 4 neighbors without center, fading halo diffusion
	RETRO_BLUR_FLAME,	// 7 weighted taps below, tall narrow rising flames
	RETRO_BLUR_FIRE,	// 8 taps beside and below, classic rising fire
	RETRO_BLUR_SMOOTH,	// Plus-with-center, isotropic softening
	RETRO_BLUR_RING		// All 8 neighbors without center, symmetric melt
};

enum RETRO_BLUR_MODE {
	RETRO_BLUR_CLAMP,
	RETRO_BLUR_WRAP,
	RETRO_BLUR_OVERFLOW
};

// The pixels a drawer may write, half-open [x0, x1) by [y0, y1). Full screen
// unless a caller passes a tighter one so different regions can have their
// own drawer.
struct RETRO_Rectangle {
	int x0 = 0;
	int x1 = RETRO_WIDTH;
	int y0 = 0;
	int y1 = RETRO_HEIGHT;
};

// A Bresenham line. intensity, when given, adds a random 0 to intensity - 1 to
// every pixel's color, so the line flickers upward from color the way a
// burning wireframe draws its edges. The caller keeps color + intensity within
// RETRO_COLORS: past it the sum wraps to the bottom of the palette.
inline void RETRO_DrawLine(int x1, int y1, int x2, int y2, unsigned char color, RETRO_Rectangle clip = {}, unsigned char intensity = 0, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	int clipx1 = MIN(clip.x1, bufferwidth);
	int clipy1 = MIN(clip.y1, bufferheight);

	// Draw from whichever end comes first, so a segment and its reverse run the
	// identical loop and light the same pixels.
	if (y1 > y2 || (y1 == y2 && x1 > x2)) {
		SWAP(x1, x2);
		SWAP(y1, y2);
	}

	int dx = x2 > x1 ? x2 - x1 : x1 - x2;
	int dy = y2 > y1 ? y2 - y1 : y1 - y2;
	int sdx = x2 > x1 ? 1 : -1;
	int sdy = y2 > y1 ? 1 : -1;
	int x = x1;
	int y = y1;

	// Midpoint Bresenham: the error starts at half the major delta, so the minor
	// axis steps where the ideal line crosses a pixel center.
	int steps = MAX(dx, dy);
	int error = steps;

	for (int i = 0; i <= steps; i++) {
		if (x >= clip.x0 && x < clipx1 && y >= clip.y0 && y < clipy1) {
			buffer[y * bufferwidth + x] = intensity ? color + RANDOM(intensity) : color;
		}
		// Doubled, so the half is exact for an odd delta
		if (dx >= dy) {
			x += sdx;
			error += 2 * dy;
			if (error >= 2 * steps) {
				error -= 2 * steps;
				y += sdy;
			}
		} else {
			y += sdy;
			error += 2 * dx;
			if (error >= 2 * steps) {
				error -= 2 * steps;
				x += sdx;
			}
		}
	}
}

// Fill a row from x1 to x2, clipped to [0, width). The ends are pixel
// positions, each truncated to the column it falls in, and x2's column is
// exclusive: a span that crosses no column boundary draws nothing, however
// close it comes.
inline void RETRO_DrawSpan(unsigned char *row, float x1, float x2, unsigned char color, int width = RETRO_WIDTH)
{
	int left = x1 < 0 ? 0 : (int)x1;
	int right = x2 > width ? width : (int)x2;

	if (right > left) {
		memset(row + left, color, right - left);
	}
}

// Filled axis-aligned rectangle with inclusive endpoints, clipped to clip.
inline void RETRO_DrawRectangle(int x1, int y1, int x2, int y2, unsigned char color, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	int clipx1 = MIN(clip.x1, bufferwidth);
	int clipy1 = MIN(clip.y1, bufferheight);

	if (x1 > x2) SWAP(x1, x2);
	if (y1 > y2) SWAP(y1, y2);

	int ymin = MAX(y1, clip.y0);
	int ymax = MIN(y2, clipy1 - 1);
	int xmin = MAX(x1, clip.x0);
	int xmax = MIN(x2, clipx1 - 1);
	if (xmin > xmax || ymin > ymax) {
		return;
	}

	for (int y = ymin; y <= ymax; y++) {
		memset(buffer + y * bufferwidth + xmin, color, xmax - xmin + 1);
	}
}

// Inclusive on y1 and y2, so DrawVline(x, y1, y2) lights the same pixels as DrawLine(x, y1, x, y2).
inline void RETRO_DrawVline(int x, int y1, int y2, unsigned char color, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	RETRO_DrawRectangle(x, y1, x, y2, color, clip, buffer, bufferwidth, bufferheight);
}

// Inclusive on x1 and x2, so DrawHline(x1, x2, y) lights the same pixels as DrawLine(x1, y, x2, y).
inline void RETRO_DrawHline(int x1, int x2, int y, unsigned char color, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	RETRO_DrawRectangle(x1, y, x2, y, color, clip, buffer, bufferwidth, bufferheight);
}

//
// Filled axis-aligned ellipse, (x − cx)² / ra² + (y − cy)² / rb² ≤ 1: every
// pixel whose center (x + 1/2, y + 1/2) lies inside, clipped to clip. ra or
// rb below 1 is empty.
//
inline void RETRO_DrawEllipse(float cx, float cy, float ra, float rb, unsigned char color, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	int clipx1 = MIN(clip.x1, bufferwidth);
	int clipy1 = MIN(clip.y1, bufferheight);

	if (ra < 1.0f || rb < 1.0f) {
		return;
	}

	int ymin = MAX((int)ceil(cy - rb - 0.5f), clip.y0);
	int ymax = MIN((int)floor(cy + rb - 0.5f), clipy1 - 1);

	for (int y = ymin; y <= ymax; y++) {
		float fy = (y + 0.5f - cy) / rb;
		float inner = 1.0f - fy * fy;
		if (inner <= 0.0f) {
			continue;
		}
		float xoff = ra * sqrt(inner);
		int xmin = MAX((int)ceil(cx - xoff - 0.5f), clip.x0);
		int xmax = MIN((int)floor(cx + xoff - 0.5f), clipx1 - 1);
		if (xmin > xmax) {
			continue;
		}
		memset(buffer + y * bufferwidth + xmin, color, xmax - xmin + 1);
	}
}

// Blits image (imagewidth x imageheight) scaled to xsize x ysize, centered on
// (xc, yc). Pixels equal to alpha are skipped; color overrides the image's
// own index when not -1.
inline void RETRO_DrawSprite(int xc, int yc, float xsize, float ysize, int imagewidth, int imageheight, unsigned char *image, unsigned char alpha, int color = -1, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	int clipx1 = MIN(clip.x1, bufferwidth);
	int clipy1 = MIN(clip.y1, bufferheight);

	float xstart = xc - xsize / 2;
	float ystart = yc - ysize / 2;
	float xdelta = imagewidth / xsize;
	float ydelta = imageheight / ysize;

	// xpos/ypos floor x + xstart/y + ystart, so a sprite hanging off the left
	// or top keeps one source column or row per screen one rather than folding
	// two onto the edge. Where that crosses clip.x0/y0 can still land a pixel
	// either side of the estimate, so the scanned range is widened by one on
	// each side and the per-pixel check below does the exact filtering.
	int ymin = MAX(0, (int)floor(clip.y0 - ystart) - 1);
	int ymax = MIN((int)ysize, (int)ceil(clipy1 - ystart) + 1);
	int xmin = MAX(0, (int)floor(clip.x0 - xstart) - 1);
	int xmax = MIN((int)xsize, (int)ceil(clipx1 - xstart) + 1);

	for (int y = ymin; y < ymax; y++) {
		int ypos = (int)floorf(y + ystart);
		int ysrc = y * ydelta;
		for (int x = xmin; x < xmax; x++) {
			int xpos = (int)floorf(x + xstart);
			int xsrc = x * xdelta;
			if (image[ysrc * imagewidth + xsrc] != alpha && xpos >= clip.x0 && xpos < clipx1 && ypos >= clip.y0 && ypos < clipy1) {
				if (color == -1) {
					buffer[ypos * bufferwidth + xpos] = image[ysrc * imagewidth + xsrc];
				} else {
					buffer[ypos * bufferwidth + xpos] = color;
				}
			}
		}
	}
}

//
// Replace every pixel with the mean of a pattern of neighbors, less decay
//
//   T' = max(0, mean(T at the pattern offsets) - decay)
//
// FIRE's eight taps sit beside and below the pixel, so heat rises. DIFFUSE is
// the four-neighbor cross. A tap listed more than once is weighted that many
// times.
//
// The field is first copied into a border as wide as any pattern reaches,
// filled the way mode says the edge behaves: CLAMP repeats the edge pixel,
// WRAP the opposite side, and OVERFLOW leaves it black. The blur then reads
// only the copy and never tests an edge.
//
// Every tap reads the field as it was before the pass: a Jacobi update, so
// the result has no direction. RETRO_BLUR_DIFFUSE is the exception: each
// result is also written back into the copy, so its left and upper taps read
// this pass. Four edge neighbors with no self term have symbol
// (cos kx + cos ky) / 2, which is -1 at the checkerboard: that mode is
// undamped and inverts every step, so reading the previous state would let
// it stand forever as dither. The taps already written couple the two
// sublattices and kill it. Every other pattern here damps the checkerboard on
// its own (RING to 0, FIRE to 1/4, SMOOTH to 3/5).
//
inline void RETRO_Blur(RETRO_BLUR_PATTERN blur, int decay = 0, RETRO_BLUR_MODE mode = RETRO_BLUR_CLAMP, unsigned char *buffer = RETRO.framebuffer)
{
	struct BlurPattern {
		int pixels;
		int offset[8][2];
	};
	// In RETRO_BLUR_PATTERN order
	static const BlurPattern patterns[] = {
		{3, {{0, -1}, {0, 0}, {0, 1}}},
		{4, {{0, -1}, {-1, 0}, {1, 0}, {0, 1}}},
		{7, {{0, 1}, {0, 1}, {0, 1}, {0, 2}, {-1, 3}, {0, 3}, {1, 3}}},
		{8, {{-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}, {-1, 2}, {0, 2}, {1, 2}}},
		{5, {{0, 0}, {0, -1}, {-1, 0}, {1, 0}, {0, 1}}},
		{8, {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}}}
	};
	const BlurPattern &pattern = patterns[blur];

	// Copy the field with its border, padded[y + 1][x + 1] holding pixel (x, y)
	static unsigned char padded[RETRO_HEIGHT + 4][RETRO_WIDTH + 2];
	for (int y = -1; y < RETRO_HEIGHT + 3; y++) {
		unsigned char *row = padded[y + 1];
		int y2 = mode == RETRO_BLUR_WRAP ? WRAPHEIGHT(y) : CLAMPHEIGHT(y);
		if (mode == RETRO_BLUR_OVERFLOW && y != y2) {
			memset(row, 0, RETRO_WIDTH + 2);
			continue;
		}
		memcpy(row + 1, buffer + RETRO.yoffset[y2], RETRO_WIDTH);
		row[0] = mode == RETRO_BLUR_WRAP ? row[RETRO_WIDTH] : (mode == RETRO_BLUR_CLAMP ? row[1] : 0);
		row[RETRO_WIDTH + 1] = mode == RETRO_BLUR_WRAP ? row[1] : (mode == RETRO_BLUR_CLAMP ? row[RETRO_WIDTH] : 0);
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int color = 0;
			for (int i = 0; i < pattern.pixels; i++) {
				color += padded[y + 1 + pattern.offset[i][1]][x + 1 + pattern.offset[i][0]];
			}
			color = MAX(color / pattern.pixels - decay, 0);

			buffer[RETRO.yoffset[y] + x] = color;
			if (blur == RETRO_BLUR_DIFFUSE) {
				padded[y + 1][x + 1] = color;
			}
		}
	}
}

#endif
