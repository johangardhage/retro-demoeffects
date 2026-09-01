//
// Crossfade
//
// Two still pictures, smoothly blended in 24-bit RGB space using a 256-color
// optimal palette shared between both pictures, indexed via a 3D inverse color LUT.
// Hardware DAC palette fade-in for the monkey picture at startup.
//
//   C(t) = (1 − t) A[i] + t B[i]
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retroshadetable.h"

#define TIME_FADEIN 1.5 // seconds to fade in the monkey from black at startup
#define CROSSFADE_SPEED 1.2 // radians of φ per second; a round trip is 2π / this

RETRO_Image *PictureA;
RETRO_Image *PictureB;
unsigned char ColorLUT[32][32][32];

void DEMO_Render(double deltatime)
{
	static double elapsed = 0;
	static double phase = 0;
	elapsed += deltatime;

	if (elapsed < TIME_FADEIN) {
		// Hardware DAC palette fade-in: pure linear dimming without color shifting
		RETRO_FadeIn(1000, (elapsed / TIME_FADEIN) * 1000, PictureA->palette);
		RETRO_Blit(PictureA->data);
		return;
	}

	// Ensure full palette brightness once fade-in completes
	static bool palette_restored = false;
	if (!palette_restored) {
		RETRO_SetPalette(PictureA->palette);
		palette_restored = true;
	}

	// Crossfade transition
	phase = fmod(phase + deltatime * CROSSFADE_SPEED, 2 * M_PI);
	float t = 0.5f - 0.5f * cos(phase);
	float s = 1.0f - t;

	unsigned char *a = PictureA->data;
	unsigned char *b = PictureB->data;
	RETRO_Palette *pa = PictureA->palette;
	RETRO_Palette *pb = PictureB->palette;
	unsigned char *buffer = RETRO_FrameBuffer();

	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		RETRO_Palette ca = pa[a[i]];
		RETRO_Palette cb = pb[b[i]];
		int r = s * ca.r + t * cb.r;
		int g = s * ca.g + t * cb.g;
		int bch = s * ca.b + t * cb.b;
		buffer[i] = ColorLUT[r >> 3][g >> 3][bch >> 3];
	}
}

void DEMO_Initialize(void)
{
	PictureA = RETRO_LoadImage("assets/monkey_320x240_quantizized.pcx", true);
	PictureB = RETRO_LoadImage("assets/flowers_320x240_quantizized.pcx");

	if (PictureA->width != RETRO_WIDTH || PictureA->height != RETRO_HEIGHT ||
		PictureB->width != RETRO_WIDTH || PictureB->height != RETRO_HEIGHT) {
		RETRO_RageQuit("Crossfade pictures must be 320x240\n");
	}

	// Build 3D inverse color lookup table to map blended RGB to closest palette entry
	for (int r = 0; r < 32; r++) {
		for (int g = 0; g < 32; g++) {
			for (int b = 0; b < 32; b++) {
				RETRO_Palette target = {
					(unsigned char)(r * 255 / 31),
					(unsigned char)(g * 255 / 31),
					(unsigned char)(b * 255 / 31)
				};
				ColorLUT[r][g][b] = RETRO_ClosestPaletteColor(target, PictureA->palette, RETRO_COLORS);
			}
		}
	}

	// Start with black hardware palette for initial fade-in
	RETRO_Palette black_palette[RETRO_COLORS] = { {0, 0, 0} };
	RETRO_SetPalette(black_palette);
}
