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

struct Point2D {
	int x, y;
};

struct Point2Df {
	float x, y;
};

struct Point3D {
	int x, y, z;
};

struct Point3Df {
	float x, y, z;
};

// C(step) = (step / steps) * C_loaded. Returns true when step >= steps.
bool RETRO_FadeIn(int steps, int step, RETRO_Palette *palette)
{
	step = CLAMP(step, 0, steps + 1);

	for (int i = 0; i < RETRO_COLORS; i++) {
		unsigned char r = (float)palette[i].r / steps * step;
		unsigned char g = (float)palette[i].g / steps * step;
		unsigned char b = (float)palette[i].b / steps * step;
		RETRO_SetColor(i, r, g, b);
	}

	return step >= steps;
}

// C(step) = ((steps - step) / steps) * C_loaded. Returns true when step >= steps.
bool RETRO_FadeOut(int steps, int step, RETRO_Palette *palette)
{
	step = CLAMP(step, 0, steps + 1);

	for (int i = 0; i < RETRO_COLORS; i++) {
		unsigned char r = (float)palette[i].r / steps * (steps - step);
		unsigned char g = (float)palette[i].g / steps * (steps - step);
		unsigned char b = (float)palette[i].b / steps * (steps - step);
		RETRO_SetColor(i, r, g, b);
	}

	return step >= steps;
}

void RETRO_DrawLine(int x1, int y1, int x2, int y2, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

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
	int px = x1;
	int py = y1;

	// Midpoint Bresenham: the error starts at half the major delta, so the minor
	// axis steps where the ideal line crosses a pixel centre.
	int steps = MAX(dx, dy);
	int error = steps;

	for (int i = 0; i <= steps; i++) {
		if (px >= 0 && px < width && py >= 0 && py < height) {
			buffer[py * width + px] = color;
		}
		// Doubled, so the half is exact for an odd delta
		if (dx >= dy) {
			px += sdx;
			error += 2 * dy;
			if (error >= 2 * steps) {
				error -= 2 * steps;
				py += sdy;
			}
		} else {
			py += sdy;
			error += 2 * dx;
			if (error >= 2 * steps) {
				error -= 2 * steps;
				px += sdx;
			}
		}
	}
}

void RETRO_DrawFireLine(int x1, int y1, int x2, int y2, unsigned char color, unsigned char intensity, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

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
	int px = x1;
	int py = y1;

	// Midpoint Bresenham: the error starts at half the major delta, so the minor
	// axis steps where the ideal line crosses a pixel centre.
	int steps = MAX(dx, dy);
	int error = steps;

	for (int i = 0; i <= steps; i++) {
		if (px >= 0 && px < width && py >= 0 && py < height) {
			buffer[py * width + px] = color + RANDOM(intensity);
		}
		// Doubled, so the half is exact for an odd delta
		if (dx >= dy) {
			px += sdx;
			error += 2 * dy;
			if (error >= 2 * steps) {
				error -= 2 * steps;
				py += sdy;
			}
		} else {
			py += sdy;
			error += 2 * dx;
			if (error >= 2 * steps) {
				error -= 2 * steps;
				px += sdx;
			}
		}
	}
}

// Inclusive on y1 and y2, so DrawVline(x, y1, y2) lights the same pixels as DrawLine(x, y1, x, y2).
void RETRO_DrawVline(int x, int y1, int y2, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

	if (y1 > y2) {
		SWAP(y1, y2);
	}

	for (int y = y1; y <= y2; y++) {
		if (x >= 0 && x < width && y >= 0 && y < height) {
			buffer[y * width + x] = color;
		}
	}
}

// Filled axis-aligned rectangle with inclusive endpoints, clipped to the buffer.
void RETRO_DrawRectangle(int x1, int y1, int x2, int y2, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

	if (x1 > x2) SWAP(x1, x2);
	if (y1 > y2) SWAP(y1, y2);
	x1 = MAX(x1, 0);
	y1 = MAX(y1, 0);
	x2 = MIN(x2, width - 1);
	y2 = MIN(y2, height - 1);
	if (x1 > x2 || y1 > y2) return;

	for (int y = y1; y <= y2; y++) memset(buffer + y * width + x1, color, x2 - x1 + 1);
}

//
// Filled axis-aligned ellipse, (x − cx)² / ra² + (y − cy)² / rb² ≤ 1.
// Each scanline is the span between the two roots in x, inclusive, clipped
// to the buffer. ra or rb below 1 is empty.
//
void RETRO_DrawEllipse(float cx, float cy, float ra, float rb, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

	if (ra < 1.0f || rb < 1.0f) {
		return;
	}

	int y0 = MAX((int)ceil(cy - rb), 0);
	int y1 = MIN((int)floor(cy + rb), height - 1);

	for (int y = y0; y <= y1; y++) {
		float fy = (y + 0.5f - cy) / rb;
		float inner = 1.0f - fy * fy;
		if (inner <= 0.0f) {
			continue;
		}
		float xoff = ra * sqrt(inner);
		int x0 = MAX((int)ceil(cx - xoff), 0);
		int x1 = MIN((int)floor(cx + xoff), width - 1);
		if (x0 > x1) {
			continue;
		}
		memset(buffer + y * width + x0, color, x1 - x0 + 1);
	}
}

void RETRO_DrawSprite(int x, int y, float xsize, float ysize, int imagewidth, int imageheight, unsigned char* image, unsigned char alpha, int color = -1, unsigned char *buffer = RETRO.framebuffer)
{
	float xstart = x - xsize / 2;
	float ystart = y - ysize / 2;
	float xdelta = imagewidth / xsize;
	float ydelta = imageheight / ysize;

	for (int xx = 0; xx < xsize; xx++) {
		for (int yy = 0; yy < ysize; yy++) {
			int xpos = xx + xstart;
			int ypos = yy + ystart;
			int xsrc = xx * xdelta;
			int ysrc = yy * ydelta;
			if (image[ysrc * imagewidth + xsrc] != alpha) {
				if (xpos >= 0 && xpos < RETRO_WIDTH && ypos >= 0 && ypos < RETRO_HEIGHT) {
					if (color == -1) {
						buffer[ypos * RETRO_WIDTH + xpos] = image[ysrc * imagewidth + xsrc];
					} else {
						buffer[ypos * RETRO_WIDTH + xpos] = color;
					}
				}
			}
		}
	}
}

//
// In-place box filter. Each pixel is replaced by
//
//   T' = max(0, mean(T at the pattern offsets) - decay)
//
// FIRE's eight taps sit beside and below the pixel, so scanning top to bottom
// lifts heat upward. DIFFUSE is the four-neighbour cross. The pass is
// Gauss-Seidel along the scan (already-written neighbours are reused).
//
// Replace every pixel with the mean of a pattern of neighbours, less decay
//
// Every tap reads the field as it was before the pass, not as the pass has
// left it: a Jacobi update, so the result has no direction. No pattern
// reaches more than one row above, so two row buffers hold everything the
// pass has overwritten; the rows below are still untouched.
//
// RETRO_BLUR_DIFFUSE is the exception and stays in place. Four edge
// neighbours with no self term have symbol (cos kx + cos ky) / 2, which is
// -1 at the checkerboard: that mode is undamped and inverts every step, so
// reading the previous state would let it stand forever as dither. The
// in-place sweep's already-written left and upper taps couple the two
// sublattices and kill it. Every other pattern here damps the checkerboard
// on its own (RING to 0, FIRE to 1/4, SMOOTH to 3/5).
//
void RETRO_Blur(RETRO_BLUR_PATTERN blur, int decay = 0, RETRO_BLUR_MODE mode = RETRO_BLUR_CLAMP, unsigned char *buffer = NULL)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

	typedef int pattern_ptr[2];
	static int patternvertical[][2] = {{0, -1}, {0, 0}, {0, 1}};
	static int patterndiffuse[][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
	static int patternflame[][2] = {{0, 1}, {0, 1}, {0, 1}, {0, 2}, {-1, 3}, {0, 3}, {1, 3}};
	static int patternfire[][2] = {{-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}, {-1, 2}, {0, 2}, {1, 2}};
	static int patternsmooth[][2] = {{0, 0}, {0, -1}, {-1, 0}, {1, 0}, {0, 1}};
	static int patternring[][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

	int pixels;
	pattern_ptr *pattern;
	switch (blur) {
	case RETRO_BLUR_VERTICAL:
		pixels = 3;
		pattern = patternvertical;
		break;
	case RETRO_BLUR_FLAME:
		pixels = 7;
		pattern = patternflame;
		break;
	case RETRO_BLUR_FIRE:
		pixels = 8;
		pattern = patternfire;
		break;
	case RETRO_BLUR_SMOOTH:
		pixels = 5;
		pattern = patternsmooth;
		break;
	case RETRO_BLUR_RING:
		pixels = 8;
		pattern = patternring;
		break;
	case RETRO_BLUR_DIFFUSE:
	default:
		pixels = 4;
		pattern = patterndiffuse;
		break;
	}

	// Pattern extents, used to skip edge handling for interior pixels
	int xmin = 0, xmax = 0, ymin = 0, ymax = 0;
	for (int i = 0; i < pixels; i++) {
		xmin = MIN(xmin, pattern[i][0]);
		xmax = MAX(xmax, pattern[i][0]);
		ymin = MIN(ymin, pattern[i][1]);
		ymax = MAX(ymax, pattern[i][1]);
	}

	// What the pass has already written over
	static unsigned char rowabove[RETRO_WIDTH];
	static unsigned char rowcurrent[RETRO_WIDTH];
	bool previousstate = blur != RETRO_BLUR_DIFFUSE;

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		memcpy(rowcurrent, buffer + RETRO.yoffset[y], RETRO_WIDTH);

		bool yinside = (y + ymin >= 0 && y + ymax < RETRO_HEIGHT);
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int color = 0;
			if (yinside && x + xmin >= 0 && x + xmax < RETRO_WIDTH) {
				for (int i = 0; i < pixels; i++) {
					int x2 = x + pattern[i][0];
					int y2 = y + pattern[i][1];
					color += !previousstate ? buffer[RETRO.yoffset[y2] + x2]
						: (y2 == y ? rowcurrent[x2] : (y2 == y - 1 ? rowabove[x2] : buffer[RETRO.yoffset[y2] + x2]));
				}
			} else {
				for (int i = 0; i < pixels; i++) {
					int x2 = x + pattern[i][0];
					int y2 = y + pattern[i][1];
					if (mode == RETRO_BLUR_WRAP) {
						x2 = WRAPWIDTH(x2);
						y2 = WRAPHEIGHT(y2);
					} else if (mode == RETRO_BLUR_CLAMP) {
						x2 = CLAMPWIDTH(x2);
						y2 = CLAMPHEIGHT(y2);
					} else if (y2 < 0 || y2 >= RETRO_HEIGHT || x2 < 0 || x2 >= RETRO_WIDTH) {
						continue; // RETRO_BLUR_OVERFLOW contributes nothing off the edge
					}
					color += !previousstate ? buffer[RETRO.yoffset[y2] + x2]
						: (y2 == y ? rowcurrent[x2] : (y2 == y - 1 ? rowabove[x2] : buffer[RETRO.yoffset[y2] + x2]));
				}
			}

			color /= pixels;
			color = MAX(color - decay, 0);

			buffer[RETRO.yoffset[y] + x] = (unsigned char)color;
		}

		memcpy(rowabove, rowcurrent, RETRO_WIDTH);
	}
}

#endif
