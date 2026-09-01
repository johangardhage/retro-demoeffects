//
// Plane demo
//
// The dual-plane variant of Plane: a mode-7 floor and a mode-7 ceiling,
// split at the horizon y = H/2. Depth z is the distance from that
// horizon — down the screen is the land, up is the sky. At depth z the
// half-width of the frustum is
//
//   w(z) = 2z + PLANE_NEAR
//
// so a column steps u by (W/2) / w(z). The forward texel is
//
//   land:  v = PLANE_LAND_V / (z + 1) + phase
//   sky:   v = PLANE_SKY_V / z + phase / PLANE_SKY_SCROLL
//
// The +1 on the land and PLANE_NEAR at the horizon keep 1/z finite, so z
// may start at 0 on the land and the horizon has no 1-pixel gap. Sky
// starts at z = 1. u on the sky is divided by PLANE_SKY_U, so the clouds
// are coarser. Both maps wrap on PLANE_MAP. The left half of the screen
// is column (W/2 − 1 − x) so column 0 is drawn and the centre column is
// not written twice.
//
// Land, sky and the ball sprite are one 128×384 atlas (assets/planedemo.pcx),
// stacked as land, sky, sprite, pitch PLANE_MAP. Palette 0 is black
// (sprite key and the reflection tint). There is no yaw. phase lives
// on the wall clock. A chain of sprites rides a Lissajous above the
// horizon, with a squashed copy as a reflection on the land.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define PLANE_VIEW (RETRO_HEIGHT / 2)
#define PLANE_SCROLL_SPEED 180.0 // texels per second
#define PLANE_MAP 128
#define PLANE_NEAR 10 // frustum half-width at the horizon, so 1/z stays finite
#define PLANE_LAND_V 7000 // land v = this / (z + 1)
#define PLANE_SKY_V 2048 // sky v = this / z
#define PLANE_SKY_U 4 // sky u is this many times coarser than the land
#define PLANE_SKY_SCROLL 32 // sky scroll is this many times slower than the land
#define ATLAS_SKY PLANE_MAP // row offset of the sky tile
#define ATLAS_SPRITE (2 * PLANE_MAP) // row offset of the sprite tile
#define SPRITE_ALPHA 0

void DEMO_Render(double time, double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();
	unsigned char *atlas = RETRO_ImageData();
	unsigned char *land = atlas;
	unsigned char *sky = atlas + ATLAS_SKY * PLANE_MAP;
	unsigned char *sprite = atlas + ATLAS_SPRITE * PLANE_MAP;

	// Calculate phase
	float phase = time * PLANE_SCROLL_SPEED;

	int midx = RETRO_WIDTH / 2;

	// Draw the ceiling. z is distance up from the horizon. u starts at
	// the centre and steps by (W/2) / w(z); the sky is sampled at
	// u / PLANE_SKY_U.
	for (int z = 1; z <= PLANE_VIEW; z++) {
		float u = 0;
		float halfw = 2 * z + PLANE_NEAR;
		float du = midx / halfw;

		for (int x = 0; x < midx; x++) {
			int xsrc = WRAP(u / PLANE_SKY_U, PLANE_MAP);
			int ysrc = WRAP(PLANE_SKY_V / z + (phase / PLANE_SKY_SCROLL), PLANE_MAP);
			dest[(PLANE_VIEW - z) * RETRO_WIDTH + (midx + x)] = sky[ysrc * PLANE_MAP + (PLANE_MAP - 1 - xsrc)];
			dest[(PLANE_VIEW - z) * RETRO_WIDTH + (midx - 1 - x)] = sky[ysrc * PLANE_MAP + xsrc];
			u += du;
		}
	}

	// Draw the floor. z is distance down from the horizon and may be 0.
	for (int z = 0; z < PLANE_VIEW; z++) {
		float u = 0;
		float halfw = 2 * z + PLANE_NEAR;
		float du = midx / halfw;

		for (int x = 0; x < midx; x++) {
			int xsrc = WRAP(u, PLANE_MAP);
			int ysrc = WRAP(PLANE_LAND_V / (z + 1) + phase, PLANE_MAP);
			dest[(PLANE_VIEW + z) * RETRO_WIDTH + (midx + x)] = land[ysrc * PLANE_MAP + (PLANE_MAP - 1 - xsrc)];
			dest[(PLANE_VIEW + z) * RETRO_WIDTH + (midx - 1 - x)] = land[ysrc * PLANE_MAP + xsrc];
			u += du;
		}
	}

	// Sprites. A Lissajous chain above the horizon, and a squashed copy
	// on the land as a reflection (tint 0).
	for (int b = 0; b < RETRO_HEIGHT; b += 10) {
		int x = midx + 50 * sin((2 * b + phase) * 0.01) + 25 * cos((2 * b + phase) * 0.01);
		int y = PLANE_VIEW - 20 + 20 * cos((b + phase) * 0.01) + 15 * sin((4 * b + phase) * 0.01);

		RETRO_DrawSprite(x, y, 15, 15, PLANE_MAP, PLANE_MAP, sprite, SPRITE_ALPHA);
		RETRO_DrawSprite(x, y / 4 + PLANE_VIEW + 15, 12, 5, PLANE_MAP, PLANE_MAP, sprite, SPRITE_ALPHA, 0);
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/planedemo.pcx", true);
}
