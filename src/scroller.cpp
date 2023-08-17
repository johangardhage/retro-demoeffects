//
// scroller.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define IMAGE_WIDTH 944

#define SCROLL_TEXT "                    HORIZONTAL SCROLLER..."

unsigned char scroll_bitmap[FONT_WIDTH * FONT_HEIGHT * strlen(SCROLL_TEXT)];

void DEMO_Render(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 400;
	int frame = frame_counter;

	static int scroll_bitmap_width = FONT_WIDTH * strlen(SCROLL_TEXT);

	// Draw scroller
	for (int i = 0; i < FONT_HEIGHT; i++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			char color = scroll_bitmap[i * scroll_bitmap_width + (x + frame) % scroll_bitmap_width];
			if (color != 0) {
				RETRO_PutPixel(x, i + 112, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/font_16x16.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());

	// Init scroll bitmap
	unsigned char *image = RETRO_ImageData();

	int scroll_length = strlen(SCROLL_TEXT);
	for (int i = 0; i < scroll_length; i++) {
		unsigned char *src = image + ((SCROLL_TEXT[i] - 32) * FONT_WIDTH);
		unsigned char *dst = scroll_bitmap + (i * FONT_WIDTH);

		for (int y = 0; y < FONT_HEIGHT; y++) {
			for (int x = 0; x < FONT_WIDTH; x++) {
				dst[FONT_WIDTH * scroll_length * y + x] = src[IMAGE_WIDTH * y + x];
			}
		}
	}
}
