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

// C(step) = (step / steps) * C_loaded. Returns true when step >= steps.
inline bool RETRO_FadeIn(int steps, int step, RETRO_Palette *palette)
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
inline bool RETRO_FadeOut(int steps, int step, RETRO_Palette *palette)
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

inline void RETRO_DrawLine(int x1, int y1, int x2, int y2, unsigned char color, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
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
	// axis steps where the ideal line crosses a pixel centre.
	int steps = MAX(dx, dy);
	int error = steps;

	for (int i = 0; i <= steps; i++) {
		if (x >= clip.x0 && x < clipx1 && y >= clip.y0 && y < clipy1) {
			buffer[y * bufferwidth + x] = color;
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

inline void RETRO_DrawFireLine(int x1, int y1, int x2, int y2, unsigned char color, unsigned char intensity, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
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
	// axis steps where the ideal line crosses a pixel centre.
	int steps = MAX(dx, dy);
	int error = steps;

	for (int i = 0; i <= steps; i++) {
		if (x >= clip.x0 && x < clipx1 && y >= clip.y0 && y < clipy1) {
			buffer[y * bufferwidth + x] = color + RANDOM(intensity);
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

// Inclusive on y1 and y2, so DrawVline(x, y1, y2) lights the same pixels as DrawLine(x, y1, x, y2).
inline void RETRO_DrawVline(int x, int y1, int y2, unsigned char color, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	int clipx1 = MIN(clip.x1, bufferwidth);
	int clipy1 = MIN(clip.y1, bufferheight);

	if (y1 > y2) SWAP(y1, y2);

	int ymin = MAX(y1, clip.y0);
	int ymax = MIN(y2, clipy1 - 1);
	if (x < clip.x0 || x >= clipx1 || ymin > ymax) {
		return;
	}

	for (int y = ymin; y <= ymax; y++) {
		buffer[y * bufferwidth + x] = color;
	}
}

// Inclusive on x1 and x2, so DrawHline(x1, x2, y) lights the same pixels as DrawLine(x1, y, x2, y).
inline void RETRO_DrawHline(int x1, int x2, int y, unsigned char color, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	int clipx1 = MIN(clip.x1, bufferwidth);
	int clipy1 = MIN(clip.y1, bufferheight);

	if (x1 > x2) SWAP(x1, x2);

	int xmin = MAX(x1, clip.x0);
	int xmax = MIN(x2, clipx1 - 1);
	if (y < clip.y0 || y >= clipy1 || xmin > xmax) {
		return;
	}

	memset(buffer + y * bufferwidth + xmin, color, xmax - xmin + 1);
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

//
// Filled axis-aligned ellipse, (x − cx)² / ra² + (y − cy)² / rb² ≤ 1.
// Each scanline is the span between the two roots in x, inclusive, clipped
// to clip. ra or rb below 1 is empty.
//
inline void RETRO_DrawEllipse(float cx, float cy, float ra, float rb, unsigned char color, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	int clipx1 = MIN(clip.x1, bufferwidth);
	int clipy1 = MIN(clip.y1, bufferheight);

	if (ra < 1.0f || rb < 1.0f) {
		return;
	}

	int ymin = MAX((int)ceil(cy - rb), clip.y0);
	int ymax = MIN((int)floor(cy + rb), clipy1 - 1);

	for (int y = ymin; y <= ymax; y++) {
		float fy = (y + 0.5f - cy) / rb;
		float inner = 1.0f - fy * fy;
		if (inner <= 0.0f) {
			continue;
		}
		float xoff = ra * sqrt(inner);
		int xmin = MAX((int)ceil(cx - xoff), clip.x0);
		int xmax = MIN((int)floor(cx + xoff), clipx1 - 1);
		if (xmin > xmax) {
			continue;
		}
		memset(buffer + y * bufferwidth + xmin, color, xmax - xmin + 1);
	}
}

// Blits image (imagewidth x imageheight) scaled to xsize x ysize, centered on
// (xc, yc). Pixels equal to alpha are skipped; color overrides the image's
// own index when not -1.
inline void RETRO_DrawSprite(int xc, int yc, float xsize, float ysize, int imagewidth, int imageheight, unsigned char* image, unsigned char alpha, int color = -1, RETRO_Rectangle clip = {}, unsigned char *buffer = RETRO.framebuffer, int bufferwidth = RETRO_WIDTH, int bufferheight = RETRO_HEIGHT)
{
	int clipx1 = MIN(clip.x1, bufferwidth);
	int clipy1 = MIN(clip.y1, bufferheight);

	float xstart = xc - xsize / 2;
	float ystart = yc - ysize / 2;
	float xdelta = imagewidth / xsize;
	float ydelta = imageheight / ysize;

	// xpos/ypos truncate x + xstart/y + ystart toward zero, so the x/y where
	// that crosses clip.x0/y0 can land a pixel early or late; widen the
	// scanned range by one on each side and let the per-pixel check below do
	// the exact filtering.
	int ymin = MAX(0, (int)floor(clip.y0 - ystart) - 1);
	int ymax = MIN((int)ysize, (int)ceil(clipy1 - ystart) + 1);
	int xmin = MAX(0, (int)floor(clip.x0 - xstart) - 1);
	int xmax = MIN((int)xsize, (int)ceil(clipx1 - xstart) + 1);

	for (int y = ymin; y < ymax; y++) {
		int ypos = y + ystart;
		int ysrc = y * ydelta;
		for (int x = xmin; x < xmax; x++) {
			int xpos = x + xstart;
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
inline void RETRO_Blur(RETRO_BLUR_PATTERN blur, int decay = 0, RETRO_BLUR_MODE mode = RETRO_BLUR_CLAMP, unsigned char *buffer = RETRO.framebuffer)
{
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
