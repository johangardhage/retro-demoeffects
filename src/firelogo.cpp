//
// Fire logo
//
// A rising heat field, one byte per pixel, the same idea as fire.cpp but
// with the logo burning as a second, standing source of fuel:
//
//   T' = max(0, mean(seven taps) − FIRE_DECAY)
//
// The taps are RETRO_BLUR_FLAME rather than the 8-tap RETRO_BLUR_FIRE:
// three on the row below, one two rows below and three at three rows
// below, with no self term and nothing on its own row. Reaching further
// down lifts heat faster, and taking no side taps on its own row keeps a
// column from smearing into its neighbours - which is what the letters
// need, since a laterally spread flame closes the gaps between them and
// the word turns into one slab.
//
// A logo texel seeds LOGO_HEAT of steady heat and as much again of
// flicker each step. Splitting it that way is what keeps the letters
// readable: seeding the whole amount at random, as a plain fire does,
// leaves them as dither with no shape of their own.
//
// The bottom rows still spark at 255, and the blit drops that fuel bed so
// only the risen flame and the logo-shaped heat are visible.
//
// The logo is set from the 16x16 font atlas instead of being loaded as a
// picture, so the wording is the two strings below. The lockup is the
// title at double size over a subtitle stretched to the same height but
// left at single width, which fits the longer word on screen at the
// weight of the shorter one. Both lines are tracked out, since the atlas
// glyphs fill their cell and would otherwise burn into each other.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define FIRE_HEIGHT 6 // rows of fuel along the bottom, cropped off the blit
#define FIRE_CHAOS 6 // a column is sparked with probability 1 / FIRE_CHAOS
#define FIRE_DECAY 4 // subtracted after the 7-tap average

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define IMAGE_WIDTH 944 // atlas pitch; 59 glyphs × 16

#define LOGO_TITLE "RETRO"
#define LOGO_SUBTITLE "DEMOEFFECTS"
#define LOGO_HEAT 56 // steady seed heat of a full-brightness texel, doubled on average by the flicker
#define LOGO_Y 78 // top row of the lockup, which centres it once the blit shifts it down
#define LOGO_GAP 6 // rows between the two lines
#define LOGO_TITLE_TRACKING 8 // columns left between glyphs, at that line's scale
#define LOGO_SUBTITLE_TRACKING 4

unsigned char FireBuffer[RETRO_HEIGHT * RETRO_WIDTH];
unsigned char LogoBuffer[RETRO_HEIGHT * RETRO_WIDTH];

//
// Set a string from the font atlas into the logo, centred, with every texel grown
// into an xscale by yscale block and tracking columns left between glyphs. A zero
// texel is transparent, and the rest scale from atlas brightness to seed heat.
//
void DrawText(const char *text, int y, int xscale, int yscale, int tracking)
{
	unsigned char *image = RETRO_ImageData();
	int length = strlen(text);
	int advance = FONT_WIDTH * xscale + tracking;
	int x = (RETRO_WIDTH - (length * advance - tracking)) / 2;

	for (int i = 0; i < length; i++) {
		unsigned char *glyph = image + ((text[i] - 32) * FONT_WIDTH);

		for (int gy = 0; gy < FONT_HEIGHT; gy++) {
			for (int gx = 0; gx < FONT_WIDTH; gx++) {
				unsigned char texel = glyph[IMAGE_WIDTH * gy + gx];
				if (texel == 0) {
					continue;
				}
				for (int sy = 0; sy < yscale; sy++) {
					for (int sx = 0; sx < xscale; sx++) {
						LogoBuffer[(y + gy * yscale + sy) * RETRO_WIDTH + (x + i * advance + gx * xscale + sx)] = texel * LOGO_HEAT / 255;
					}
				}
			}
		}
	}
}

//
// Advance the flame in fixed steps. It rises one blur pass at a time through a buffer
// that is never cleared, and the logo is reseeded once per step, so the step rate sets
// both speeds.
//
void DEMO_Update(double deltatime)
{
	// Seed logo. Half of what a texel puts in is the same every step and half is drawn
	// fresh, so a letter holds its shape while its heat still boils.
	for (int offset = 0; offset < RETRO_HEIGHT * RETRO_WIDTH; offset++) {
		if (LogoBuffer[offset] > 0) {
			FireBuffer[offset] = LogoBuffer[offset] + RANDOM(LogoBuffer[offset]);
		}
	}

	// Seed bed
	for (int x = 0; x < RETRO_WIDTH; x++) {
		if (RANDOM(FIRE_CHAOS) == 0) {
			for (int y = RETRO_HEIGHT - FIRE_HEIGHT; y < RETRO_HEIGHT; y++) {
				FireBuffer[y * RETRO_WIDTH + x] = 255;
			}
		}
	}

	// Rise
	RETRO_Blur(RETRO_BLUR_FLAME, FIRE_DECAY, RETRO_BLUR_WRAP, FireBuffer);
}

//
// Draw the field, dropping the fuel bed the way fire.cpp does
//
void DEMO_Render(double deltatime)
{
	RETRO_Blit(FireBuffer, (RETRO_HEIGHT - FIRE_HEIGHT) * RETRO_WIDTH, RETRO_FrameBuffer() + (FIRE_HEIGHT * RETRO_WIDTH));
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/font_16x16.pcx");

	// Init logo
	DrawText(LOGO_TITLE, LOGO_Y, 2, 2, LOGO_TITLE_TRACKING);
	DrawText(LOGO_SUBTITLE, LOGO_Y + FONT_HEIGHT * 2 + LOGO_GAP, 1, 2, LOGO_SUBTITLE_TRACKING);

	// Init palette
	RETRO_CreateGradientPalette(0, 24, RETRO_BLACK, RETRO_DARKBLUE);
	RETRO_CreateGradientPalette(24, 48, RETRO_DARKBLUE, RETRO_RED);
	RETRO_CreateGradientPalette(48, 64, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(64, 128, RETRO_YELLOW, RETRO_WHITE);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_WHITE, RETRO_WHITE);
}
