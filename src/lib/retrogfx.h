//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROGFX_H_
#define _RETROGFX_H_

#include "retro.h"

enum {
	RETRO_BLUR_CLAMP,
	RETRO_BLUR_WRAP,
	RETRO_BLUR_OVERFLOW,
	RETRO_BLUR_3,
	RETRO_BLUR_4,
	RETRO_BLUR_7,
	RETRO_BLUR_8
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

bool RETRO_FadeIn(int steps, int step, RETRO_Palette *palette)
{
	if (step >= steps) return true;

	for (int i = 0; i < RETRO_COLORS; i++) {
		unsigned char r = (float)palette[i].r / steps * step;
		unsigned char g = (float)palette[i].g / steps * step;
		unsigned char b = (float)palette[i].b / steps * step;
		RETRO_SetColor(i, r, g, b);
	}

	return false;
}

bool RETRO_FadeOut(int steps, int step, RETRO_Palette *palette)
{
	if (step >= steps) return true;

	for (int i = 0; i < RETRO_COLORS; i++) {
		unsigned char r = (float)palette[i].r / steps * (steps - step);
		unsigned char g = (float)palette[i].g / steps * (steps - step);
		unsigned char b = (float)palette[i].b / steps * (steps - step);
		RETRO_SetColor(i, r, g, b);
	}

	return false;
}

void RETRO_DrawLine(int x1, int y1, int x2, int y2, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

	int x = 0;
	int y = 0;
	int dx = x2 - x1;
	int dy = y2 - y1;
	int sdx = (dx < 0) ? -1 : 1;
	int sdy = (dy < 0) ? -1 : 1;
	int px = x1;
	int py = y1;

	dx = sdx * dx + 1;
	dy = sdy * dy + 1;

	if (dx >= dy) {
		for (x = 0; x < dx; x++) {
			if (px >= 0 && px < width && py >= 0 && py < height) {
				buffer[py * width + px] = color;
			}
			y += dy;
			if (y >= dx) {
				y -= dx;
				py += sdy;
			}
			px += sdx;
		}
	} else {
		for (y = 0; y < dy; y++) {
			if (px >= 0 && px < width && py >= 0 && py < height) {
				buffer[py * width + px] = color;
			}
			x += dx;
			if (x >= dy) {
				x -= dy;
				px += sdx;
			}
			py += sdy;
		}
	}
}

void RETRO_DrawFireLine(int x1, int y1, int x2, int y2, unsigned char color, unsigned char intensity, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

	int x = 0;
	int y = 0;
	int dx = x2 - x1;
	int dy = y2 - y1;
	int sdx = (dx < 0) ? -1 : 1;
	int sdy = (dy < 0) ? -1 : 1;
	int px = x1;
	int py = y1;

	dx = sdx * dx + 1;
	dy = sdy * dy + 1;

	if (dx >= dy) {
		for (x = 0; x < dx; x++) {
			if (px >= 0 && px < width && py >= 0 && py < height) {
				buffer[py * width + px] = color + RANDOM(intensity);
			}
			y += dy;
			if (y >= dx) {
				y -= dx;
				py += sdy;
			}
			px += sdx;
		}
	} else {
		for (y = 0; y < dy; y++) {
			if (px >= 0 && px < width && py >= 0 && py < height) {
				buffer[py * width + px] = color + RANDOM(intensity);
			}
			x += dx;
			if (x >= dy) {
				x -= dy;
				px += sdx;
			}
			py += sdy;
		}
	}
}

void RETRO_DrawVline(int x, int y1, int y2, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

	for (int y = y1; y < y2; y++) {
		if (x >= 0 && x < width && y >= 0 && y < height) {
			buffer[y * width + x] = color;
		}
	}
}

void RETRO_Blur(int blur, int decay = 0, int mode = RETRO_BLUR_CLAMP, unsigned char *buffer = NULL)
{
	buffer = buffer ? buffer : RETRO.framebuffer;

	typedef int pattern_ptr[2];
	static int pattern3[][2] = {{0, -1}, {0, 0}, {0, 1}};
	static int pattern4[][2] = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}};
	static int pattern7[][2] = {{0, 1}, {0, 1}, {0, 1}, {0, 2}, {-1, 3}, {0, 3}, {1, 3}};
	static int pattern8[][2] = {{-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}, {-1, 2}, {0, 2}, {1, 2}};

	int pixels;
	pattern_ptr *pattern;
	if (blur == RETRO_BLUR_3) {
		pixels = 3;
		pattern = pattern3;
	} else if (blur == RETRO_BLUR_4) {
		pixels = 4;
		pattern = pattern4;
	} else if (blur == RETRO_BLUR_7) {
		pixels = 7;
		pattern = pattern7;
	} else if (blur == RETRO_BLUR_8) {
		pixels = 8;
		pattern = pattern8;
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int color = 0;
			for (int i = 0; i < pixels; i++) {
				if (mode == RETRO_BLUR_WRAP) {
					color += buffer[RETRO.yoffset[WRAPHEIGHT(y + pattern[i][1])] + WRAPWIDTH(x + pattern[i][0])];
				} else if (mode == RETRO_BLUR_CLAMP) {
					color += buffer[RETRO.yoffset[CLAMPHEIGHT(y + pattern[i][1])] + CLAMPWIDTH(x + pattern[i][0])];
				} else if (mode == RETRO_BLUR_OVERFLOW) {
					int x2 = x + pattern[i][0];
					int y2 = y + pattern[i][1];
					if (y2 >= 0 && y2 < RETRO_HEIGHT && x2 >= 0 && x2 < RETRO_WIDTH) {
						color += buffer[RETRO.yoffset[y2] + x2];
					}
				}
			}

			color /= pixels;

			if (color > decay) {
				color -= decay;
			}

			buffer[RETRO.yoffset[y] + x] = (unsigned char)color;
		}
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

RETRO_Palette RETRO_Default8bitPalette[256] = {
  0,   0,   0,
  0,   0, 170,
  0, 170,   0,
  0, 170, 170,
170,   0,   0,
170,   0, 170,
170,  85,   0,
170, 170, 170,
 85,  85,  85,
 85,  85, 255,
 85, 255,  85,
 85, 255, 255,
255,  85,  85,
255,  85, 255,
255, 255,  85,
255, 255, 255,
  0,   0,   0,
 20,  20,  20,
 32,  32,  32,
 44,  44,  44,
 56,  56,  56,
 69,  69,  69,
 81,  81,  81,
 97,  97,  97,
113, 113, 113,
130, 130, 130,
146, 146, 146,
162, 162, 162,
182, 182, 182,
203, 203, 203,
227, 227, 227,
255, 255, 255,
  0,   0, 255,
 65,   0, 255,
125,   0, 255,
190,   0, 255,
255,   0, 255,
255,   0, 190,
255,   0, 125,
255,   0,  65,
255,   0,   0,
255,  65,   0,
255, 125,   0,
255, 190,   0,
255, 255,   0,
190, 255,   0,
125, 255,   0,
 65, 255,   0,
  0, 255,   0,
  0, 255,  65,
  0, 255, 125,
  0, 255, 190,
  0, 255, 255,
  0, 190, 255,
  0, 125, 255,
  0,  65, 255,
125, 125, 255,
158, 125, 255,
190, 125, 255,
223, 125, 255,
255, 125, 255,
255, 125, 223,
255, 125, 190,
255, 125, 158,
255, 125, 125,
255, 158, 125,
255, 190, 125,
255, 223, 125,
255, 255, 125,
223, 255, 125,
190, 255, 125,
158, 255, 125,
125, 255, 125,
125, 255, 158,
125, 255, 190,
125, 255, 223,
125, 255, 255,
125, 223, 255,
125, 190, 255,
125, 158, 255,
182, 182, 255,
199, 182, 255,
219, 182, 255,
235, 182, 255,
255, 182, 255,
255, 182, 235,
255, 182, 219,
255, 182, 199,
255, 182, 182,
255, 199, 182,
255, 219, 182,
255, 235, 182,
255, 255, 182,
235, 255, 182,
219, 255, 182,
199, 255, 182,
182, 255, 182,
182, 255, 199,
182, 255, 219,
182, 255, 235,
182, 255, 255,
182, 235, 255,
182, 219, 255,
182, 199, 255,
  0,   0, 113,
 28,   0, 113,
 56,   0, 113,
 85,   0, 113,
113,   0, 113,
113,   0,  85,
113,   0,  56,
113,   0,  28,
113,   0,   0,
113,  28,   0,
113,  56,   0,
113,  85,   0,
113, 113,   0,
 85, 113,   0,
 56, 113,   0,
 28, 113,   0,
  0, 113,   0,
  0, 113,  28,
  0, 113,  56,
  0, 113,  85,
  0, 113, 113,
  0,  85, 113,
  0,  56, 113,
  0,  28, 113,
 56,  56, 113,
 69,  56, 113,
 85,  56, 113,
 97,  56, 113,
113,  56, 113,
113,  56,  97,
113,  56,  85,
113,  56,  69,
113,  56,  56,
113,  69,  56,
113,  85,  56,
113,  97,  56,
113, 113,  56,
 97, 113,  56,
 85, 113,  56,
 69, 113,  56,
 56, 113,  56,
 56, 113,  69,
 56, 113,  85,
 56, 113,  97,
 56, 113, 113,
 56,  97, 113,
 56,  85, 113,
 56,  69, 113,
 81,  81, 113,
 89,  81, 113,
 97,  81, 113,
105,  81, 113,
113,  81, 113,
113,  81, 105,
113,  81,  97,
113,  81,  89,
113,  81,  81,
113,  89,  81,
113,  97,  81,
113, 105,  81,
113, 113,  81,
105, 113,  81,
 97, 113,  81,
 89, 113,  81,
 81, 113,  81,
 81, 113,  89,
 81, 113,  97,
 81, 113, 105,
 81, 113, 113,
 81, 105, 113,
 81,  97, 113,
 81,  89, 113,
  0,   0,  65,
 16,   0,  65,
 32,   0,  65,
 48,   0,  65,
 65,   0,  65,
 65,   0,  48,
 65,   0,  32,
 65,   0,  16,
 65,   0,   0,
 65,  16,   0,
 65,  32,   0,
 65,  48,   0,
 65,  65,   0,
 48,  65,   0,
 32,  65,   0,
 16,  65,   0,
  0,  65,   0,
  0,  65,  16,
  0,  65,  32,
  0,  65,  48,
  0,  65,  65,
  0,  48,  65,
  0,  32,  65,
  0,  16,  65,
 32,  32,  65,
 40,  32,  65,
 48,  32,  65,
 56,  32,  65,
 65,  32,  65,
 65,  32,  56,
 65,  32,  48,
 65,  32,  40,
 65,  32,  32,
 65,  40,  32,
 65,  48,  32,
 65,  56,  32,
 65,  65,  32,
 56,  65,  32,
 48,  65,  32,
 40,  65,  32,
 32,  65,  32,
 32,  65,  40,
 32,  65,  48,
 32,  65,  56,
 32,  65,  65,
 32,  56,  65,
 32,  48,  65,
 32,  40,  65,
 44,  44,  65,
 48,  44,  65,
 52,  44,  65,
 60,  44,  65,
 65,  44,  65,
 65,  44,  60,
 65,  44,  52,
 65,  44,  48,
 65,  44,  44,
 65,  48,  44,
 65,  52,  44,
 65,  60,  44,
 65,  65,  44,
 60,  65,  44,
 52,  65,  44,
 48,  65,  44,
 44,  65,  44,
 44,  65,  48,
 44,  65,  52,
 44,  65,  60,
 44,  65,  65,
 44,  60,  65,
 44,  52,  65,
 44,  48,  65,
  0,   0,   0,
  0,   0,   0,
  0,   0,   0,
  0,   0,   0,
  0,   0,   0,
  0,   0,   0,
  0,   0,   0,
  0,   0,   0
};

RETRO_Palette RETRO_Default6bitPalette[256] = {
 0,  0,  0,
 0,  0, 42,
 0, 42,  0,
 0, 42, 42,
42,  0,  0,
42,  0, 42,
42, 21,  0,
42, 42, 42,
21, 21, 21,
21, 21, 63,
21, 63, 21,
21, 63, 63,
63, 21, 21,
63, 21, 63,
63, 63, 21,
63, 63, 63,
 0,  0,  0,
 5,  5,  5,
 8,  8,  8,
11, 11, 11,
14, 14, 14,
17, 17, 17,
20, 20, 20,
24, 24, 24,
28, 28, 28,
32, 32, 32,
36, 36, 36,
40, 40, 40,
45, 45, 45,
50, 50, 50,
56, 56, 56,
63, 63, 63,
 0,  0, 63,
16,  0, 63,
31,  0, 63,
47,  0, 63,
63,  0, 63,
63,  0, 47,
63,  0, 31,
63,  0, 16,
63,  0,  0,
63, 16,  0,
63, 31,  0,
63, 47,  0,
63, 63,  0,
47, 63,  0,
31, 63,  0,
16, 63,  0,
 0, 63,  0,
 0, 63, 16,
 0, 63, 31,
 0, 63, 47,
 0, 63, 63,
 0, 47, 63,
 0, 31, 63,
 0, 16, 63,
31, 31, 63,
39, 31, 63,
47, 31, 63,
55, 31, 63,
63, 31, 63,
63, 31, 55,
63, 31, 47,
63, 31, 39,
63, 31, 31,
63, 39, 31,
63, 47, 31,
63, 55, 31,
63, 63, 31,
55, 63, 31,
47, 63, 31,
39, 63, 31,
31, 63, 31,
31, 63, 39,
31, 63, 47,
31, 63, 55,
31, 63, 63,
31, 55, 63,
31, 47, 63,
31, 39, 63,
45, 45, 63,
49, 45, 63,
54, 45, 63,
58, 45, 63,
63, 45, 63,
63, 45, 58,
63, 45, 54,
63, 45, 49,
63, 45, 45,
63, 49, 45,
63, 54, 45,
63, 58, 45,
63, 63, 45,
58, 63, 45,
54, 63, 45,
49, 63, 45,
45, 63, 45,
45, 63, 49,
45, 63, 54,
45, 63, 58,
45, 63, 63,
45, 58, 63,
45, 54, 63,
45, 49, 63,
 0,  0, 28,
 7,  0, 28,
14,  0, 28,
21,  0, 28,
28,  0, 28,
28,  0, 21,
28,  0, 14,
28,  0,  7,
28,  0,  0,
28,  7,  0,
28, 14,  0,
28, 21,  0,
28, 28,  0,
21, 28,  0,
14, 28,  0,
 7, 28,  0,
 0, 28,  0,
 0, 28,  7,
 0, 28, 14,
 0, 28, 21,
 0, 28, 28,
 0, 21, 28,
 0, 14, 28,
 0,  7, 28,
14, 14, 28,
17, 14, 28,
21, 14, 28,
24, 14, 28,
28, 14, 28,
28, 14, 24,
28, 14, 21,
28, 14, 17,
28, 14, 14,
28, 17, 14,
28, 21, 14,
28, 24, 14,
28, 28, 14,
24, 28, 14,
21, 28, 14,
17, 28, 14,
14, 28, 14,
14, 28, 17,
14, 28, 21,
14, 28, 24,
14, 28, 28,
14, 24, 28,
14, 21, 28,
14, 17, 28,
20, 20, 28,
22, 20, 28,
24, 20, 28,
26, 20, 28,
28, 20, 28,
28, 20, 26,
28, 20, 24,
28, 20, 22,
28, 20, 20,
28, 22, 20,
28, 24, 20,
28, 26, 20,
28, 28, 20,
26, 28, 20,
24, 28, 20,
22, 28, 20,
20, 28, 20,
20, 28, 22,
20, 28, 24,
20, 28, 26,
20, 28, 28,
20, 26, 28,
20, 24, 28,
20, 22, 28,
 0,  0, 16,
 4,  0, 16,
 8,  0, 16,
12,  0, 16,
16,  0, 16,
16,  0, 12,
16,  0,  8,
16,  0,  4,
16,  0,  0,
16,  4,  0,
16,  8,  0,
16, 12,  0,
16, 16,  0,
12, 16,  0,
 8, 16,  0,
 4, 16,  0,
 0, 16,  0,
 0, 16,  4,
 0, 16,  8,
 0, 16, 12,
 0, 16, 16,
 0, 12, 16,
 0,  8, 16,
 0,  4, 16,
 8,  8, 16,
10,  8, 16,
12,  8, 16,
14,  8, 16,
16,  8, 16,
16,  8, 14,
16,  8, 12,
16,  8, 10,
16,  8,  8,
16, 10,  8,
16, 12,  8,
16, 14,  8,
16, 16,  8,
14, 16,  8,
12, 16,  8,
10, 16,  8,
 8, 16,  8,
 8, 16, 10,
 8, 16, 12,
 8, 16, 14,
 8, 16, 16,
 8, 14, 16,
 8, 12, 16,
 8, 10, 16,
11, 11, 16,
12, 11, 16,
13, 11, 16,
15, 11, 16,
16, 11, 16,
16, 11, 15,
16, 11, 13,
16, 11, 12,
16, 11, 11,
16, 12, 11,
16, 13, 11,
16, 15, 11,
16, 16, 11,
15, 16, 11,
13, 16, 11,
12, 16, 11,
11, 16, 11,
11, 16, 12,
11, 16, 13,
11, 16, 15,
11, 16, 16,
11, 15, 16,
11, 13, 16,
11, 12, 16,
 0,  0,  0,
 0,  0,  0,
 0,  0,  0,
 0,  0,  0,
 0,  0,  0,
 0,  0,  0,
 0,  0,  0,
 0,  0,  0
};

#endif
