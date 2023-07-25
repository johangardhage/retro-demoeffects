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

void RETRO_DrawLine(int x1, int y1, int x2, int y2, unsigned char color, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO_GetSurfaceBuffer();

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

void RETRO_DrawLine2(int x1, int y1, int x2, int y2, unsigned char *colors, unsigned char *buffer = NULL, int width = RETRO_WIDTH, int height = RETRO_HEIGHT)
{
	buffer = buffer ? buffer : RETRO_GetSurfaceBuffer();

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
				buffer[py * width + px] = colors[x];
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
				buffer[py * width + px] = colors[x];
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
	buffer = buffer ? buffer : RETRO_GetSurfaceBuffer();

	for (int y = y1; y < y2; y++) {
		if (x >= 0 && x < width && y >= 0 && y < height) {
			buffer[y * width + x] = color;
		}
	}
}

void RETRO_Blur(int blur, int decay = 0, int mode = RETRO_BLUR_CLAMP, unsigned char *buffer = NULL)
{
	buffer = buffer ? buffer : RETRO_GetSurfaceBuffer();

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
					color += buffer[RETRO_lib.yoffsettable[WRAPHEIGHT(y + pattern[i][1])] + WRAPWIDTH(x + pattern[i][0])];
				} else if (mode == RETRO_BLUR_CLAMP) {
					color += buffer[RETRO_lib.yoffsettable[CLAMPHEIGHT(y + pattern[i][1])] + CLAMPWIDTH(x + pattern[i][0])];
				} else if (mode == RETRO_BLUR_OVERFLOW) {
					int x2 = x + pattern[i][0];
					int y2 = y + pattern[i][1];
					if (y2 >= 0 && y2 < RETRO_HEIGHT && x2 >= 0 && x2 < RETRO_WIDTH) {
						color += buffer[RETRO_lib.yoffsettable[y2] + x2];
					}
				}
			}

			color /= pixels;

			if (color > decay) {
				color -= decay;
			}

			buffer[RETRO_lib.yoffsettable[y] + x] = (unsigned char)color;
		}
	}
}

#endif
