//
// 3D diagonal dot scroller
//
// The font (see FONT below) is packed into one horizontal strip at startup
// and sampled as a strip of points,
//
//   worldx = column · DOT_SPACING − phase
//   z      = AMP · sin(column · DOT_SPACING · 2π / WAVELENGTH + depthphase)
//   scale  = CAMERA / (CAMERA + z)
//   x      = (worldx − WIDTH/2) · scale + WIDTH/2
//   y      = HEIGHT/2 + (glyphy + worldx · DIAGONAL_RISE) · scale
//
// glyphy is the texel's row, centred on the strip. The strip travels from
// the lower right to the upper left while the sine bends it in depth.
// Perspective makes the near parts larger and the far parts smaller; every
// visible font texel is still drawn as one pixel only. phase lives on
// displaywidth = stripwidth · DOT_SPACING, in screen pixels per second. A
// zero texel is transparent.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define DOT_SPACING 3
#define SCROLL_SPEED 100 // screen pixels per second
#define DIAGONAL_RISE 0.25 // pixels upward for each pixel travelled left

#define DEPTH_AMPLITUDE 110
#define DEPTH_WAVELENGTH 220
#define DEPTH_SPEED 2.0 // radians per second
#define CAMERA_DISTANCE 360

static const char *const ScrollText[] = { "       RETRO DEMOEFFECTS..." };

RETRO_Image *ScrollImage;

void DEMO_Render(double time, double deltatime)
{
	int displaywidth = ScrollImage->width * DOT_SPACING;

	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, displaywidth);
	double depthphase = fmod(time * DEPTH_SPEED, 2 * M_PI);

	for (int sy = 0; sy < ScrollImage->height; sy++) {
		for (int sx = 0; sx < ScrollImage->width; sx++) {
			unsigned char color = ScrollImage->data[sy * ScrollImage->width + sx];
			if (color == 0) {
				continue;
			}

			// Scroll the point strip and wrap it after its padded tail.
			double worldx = sx * DOT_SPACING - phase;
			if (worldx < 0) {
				worldx += displaywidth;
			}
			if (worldx > RETRO_WIDTH + DEPTH_AMPLITUDE) {
				continue;
			}

			// Bend the strip toward and away from the camera. The depth wave
			// belongs to the text, so its shape travels with the letters.
			double z = DEPTH_AMPLITUDE * sin(sx * DOT_SPACING * 2 * M_PI /
				DEPTH_WAVELENGTH + depthphase);
			double scale = CAMERA_DISTANCE / (CAMERA_DISTANCE + z);

			double x = (worldx - RETRO_WIDTH / 2.0) * scale + RETRO_WIDTH / 2.0;
			double glyphy = (sy - (ScrollImage->height - 1) / 2.0) * DOT_SPACING;
			double pathy = (worldx - RETRO_WIDTH / 2.0) * DIAGONAL_RISE;
			double y = RETRO_HEIGHT / 2.0 + (glyphy + pathy) * scale;

			int px = lround(x);
			int py = lround(y);
			if (px >= 0 && px < RETRO_WIDTH && py >= 0 && py < RETRO_HEIGHT) {
				RETRO_PutPixel(px, py, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	RETRO_SetPalette(ScrollImage->palette);
}
