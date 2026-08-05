//
// Wormhole
//
// A precomputed map from screen pixel to a 15×15 texture. Ring d and spoke s
// land at
//
//   (x, y) = (xd cos θ, yd sin θ) + (W/2, H/4 - z)
//   xd = W d / DIVS,   yd = H d / DIVS
//   z  = Z0 + ZLOG * log(2 d / DIVS)
//
// The ellipse grows linearly with d (real radii, not integer pixels). The
// log in z packs far rings toward the throat, which is why d = 0 is
// skipped (log 0). The texture index is (s/8, d/7) wrapped to 15. Each
// frame the tiny texture is scrolled; φ lives on 15. The map is only a
// lookup.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define WORM_XDIR -1
#define WORM_YDIR 1
#define WORM_SPOKES 2400
#define WORM_DIVS 2400
#define WORM_Z0 -10 // constant offset of the throat
#define WORM_ZLOG 11 // how hard the log packs far rings
#define WORM_SPEED 100 // texture texels a second
#define TEXTURE_WIDTH 15
#define TEXTURE_HEIGHT 15

unsigned char WormHole[RETRO_WIDTH * RETRO_HEIGHT];

void DEMO_Render(double deltatime)
{
	unsigned char *image = RETRO_ImageData();

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * WORM_SPEED, TEXTURE_WIDTH);
	int xphase = (int)phase * WORM_XDIR;
	int yphase = (int)phase * WORM_YDIR;

	unsigned char newimage[TEXTURE_WIDTH * TEXTURE_HEIGHT];

	// Create new image
	for (int y = 0; y < TEXTURE_HEIGHT; y++) {
		for (int x = 0; x < TEXTURE_WIDTH; x++) {
			newimage[TEXTURE_WIDTH * y + x] = image[TEXTURE_WIDTH * WRAP(y + yphase, TEXTURE_HEIGHT) + WRAP(x + xphase, TEXTURE_WIDTH)];
		}
	}

	// Draw wormhole
	unsigned char *buffer = RETRO_FrameBuffer();
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		buffer[i] = newimage[WormHole[i]];
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/wormhole_15x15.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());

	// Init tables
	float CosTable[WORM_SPOKES];
	float SinTable[WORM_SPOKES];
	for (int i = 0; i < WORM_SPOKES; i++) {
		CosTable[i] = cos(2 * M_PI * i / WORM_SPOKES);
		SinTable[i] = sin(2 * M_PI * i / WORM_SPOKES);
	}

	// Init wormhole (skip d = 0, where log(0) is undefined)
	for (int d = 1; d < WORM_DIVS; d++) {
		float xd = (float)RETRO_WIDTH * d / WORM_DIVS;
		float yd = (float)RETRO_HEIGHT * d / WORM_DIVS;
		float zd = WORM_Z0 + log(2.0 * d / WORM_DIVS) * WORM_ZLOG;

		for (int s = 0; s < WORM_SPOKES; s++) {
			int x = xd * CosTable[s] + RETRO_WIDTH / 2.0;
			int y = yd * SinTable[s] + RETRO_HEIGHT / 2.0 - RETRO_HEIGHT / 4.0 - zd;

			if ((x >= 0) && (x < RETRO_WIDTH) && (y >= 0) && (y < RETRO_HEIGHT)) {
				unsigned char color = WRAP(s / 8, TEXTURE_WIDTH) + (TEXTURE_WIDTH * WRAP(d / 7, TEXTURE_WIDTH));
				WormHole[y * RETRO_WIDTH + x] = color;
			}
		}
	}
}
