//
// Plane 2
//
// Plane with a sky and distance fog. The camera sits a height PLANE_EYE
// above the plane. Row y is counted up from the bottom of the screen, so
// the pixel is written at HEIGHT − y. Screen point (x, y) looks at the
// plane at
//
//   u = PLANE_EYE · (W/2) / P · (x − W/2) / (y − H/2)
//   v = PLANE_EYE · (H/2) / (y − H/2)
//
// with P = PLANE_PERIOD. u and v are already texels. v is constant on a
// scanline and u is linear in x, so after yaw the rotated (u, v) advance
// by a constant step. Yaw about the vertical by θ = ang is
//
//   unew = xd + u cos θ − v sin θ
//   vnew = yd + u sin θ + v cos θ
//
// The horizon is y = H/2, where the denominator is zero and the row is
// left undrawn. The loop runs y = 1 … PLANE_VIEW − 1, so it never hits it.
//
// Fog is a shade on the palette index, not an RGB blend. Each texel stores
// a ramp bit and a 0–31 shade. Far rows scale that shade by
//
//   1 − (y / PLANE_VIEW)²
//
// so a hunter-green tile fades through hunter green, not through a shared
// grey. Two palettes both start at the same haze: 0–31 hunter green,
// 32–63 moss green. Palette 64–127 is zenith to that haze, used for the
// sky; the sky parameter is t² so the haze hugs the horizon. xd and yd
// are not wrapped; WRAP at the sample is one texture period. ang lives
// on 360°. The texture is a generated P×P wrapping checker of
// FIELD_TILE-texel squares.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define PLANE_VIEW (RETRO_HEIGHT / 2)
#define PLANE_EYE 100 // camera height in world units
#define PLANE_YAW_SPEED 24.0 // degrees per second
#define PLANE_WALK_SPEED 80.0 // texels per second
#define PLANE_PERIOD 256 // one texture width; u, v, xd and yd share it
#define FIELD_TILE 64

unsigned char Field[PLANE_PERIOD * PLANE_PERIOD];

void DEMO_Render(double time, double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();

	// Yaw the sampled (u, v) in the plane, and walk (xd, yd) in texture
	// space. WRAP at the sample is one texture period.
	float ang = fmod(time * PLANE_YAW_SPEED, 360.0);
	float cosa = cos(ang * DEG2RAD);
	float sina = sin(ang * DEG2RAD);

	float xd = time * PLANE_WALK_SPEED;
	float yd = time * PLANE_WALK_SPEED;

	// Perspective scales. v is the forward texel at this depth and does
	// not depend on x. u stretches about the screen centre with a factor
	// (W/2) / P so a half-width of pixels is one texture period at unit
	// depth. midx, midy are the vanishing point (the horizon is midy).
	float xscale = PLANE_EYE * (RETRO_WIDTH / 2.0f) / PLANE_PERIOD;
	float yscale = PLANE_EYE * (RETRO_HEIGHT / 2.0f);
	float midx = RETRO_WIDTH / 2.0f;
	float midy = RETRO_HEIGHT / 2.0f;

	// Sky. t = 0 at the top of the screen (zenith, index 64), t = 1 at
	// the horizon (haze, index 127). t² keeps most of the sky zenith-blue
	// and packs the haze into the last rows above the floor.
	for (int y = 0; y < PLANE_VIEW; y++) {
		float t = (float)y / (PLANE_VIEW - 1);
		unsigned char color = 64 + (unsigned char)((t * t) * 63);
		memset(dest + y * RETRO_WIDTH, color, RETRO_WIDTH);
	}

	// Draw the floor. y is distance up from the bottom; the pixel sits at
	// HEIGHT − y. v and 1/(y − midy) are constant on the row, so start at
	// the left edge (x = 0) and step the yawed (u, v) by xscale / (y − midy)
	// rotated, which is one pixel of x.
	for (int y = 1; y < PLANE_VIEW; y++) {
		float ic = 1.0f / (y - midy);
		float u = xscale * -midx * ic;
		float v = yscale * ic;
		float unew = xd + u * cosa - v * sina;
		float vnew = yd + u * sina + v * cosa;
		float du = xscale * ic * cosa;
		float dv = xscale * ic * sina;
		// Near rows (small y) keep their shade; far rows walk the ramp
		// toward haze (index 0 or 32, the same RGB).
		float shade = 1.0f - ((float)y / PLANE_VIEW) * ((float)y / PLANE_VIEW);

		unsigned char *row = dest + (RETRO_HEIGHT - y) * RETRO_WIDTH;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char texel = Field[WRAP(vnew, PLANE_PERIOD) * PLANE_PERIOD + WRAP(unew, PLANE_PERIOD)];
			row[x] = (texel & 32) + (unsigned char)((texel & 31) * shade);
			unew += du;
			vnew += dv;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Two floor ramps, both starting at haze so fog stays
	// in-hue: 0–31 hunter green, 32–63 moss green. 64–127 is the sky,
	// zenith to the same haze.
	RETRO_CreateGradientPalette(0, 32, RETRO_HAZE, RETRO_HUNTERGREEN);
	RETRO_SetColor(31, RETRO_HUNTERGREEN);
	RETRO_CreateGradientPalette(32, 64, RETRO_HAZE, RETRO_MOSSGREEN);
	RETRO_SetColor(32, RETRO_HAZE);
	RETRO_SetColor(63, RETRO_MOSSGREEN);

	RETRO_CreateGradientPalette(64, 96, RETRO_SPACECADET, RETRO_GLAUCOUS);
	RETRO_CreateGradientPalette(96, 127, RETRO_GLAUCOUS, RETRO_HAZE);
	RETRO_SetColor(127, RETRO_HAZE);
	RETRO_SetColor(0, RETRO_HAZE);

	// Init field. Bit 5 selects the ramp; 27–30 is the unfogged shade,
	// so near tiles sit at the saturated end of the ramp.
	for (int y = 0; y < PLANE_PERIOD; y++) {
		for (int x = 0; x < PLANE_PERIOD; x++) {
			int tx = x / FIELD_TILE;
			int ty = y / FIELD_TILE;
			int checker = (tx ^ ty) & 1;
			int vary = (tx * 13 + ty * 7) & 3;
			Field[y * PLANE_PERIOD + x] = (unsigned char)((checker ? 32 : 0) + 27 + vary);
		}
	}
}
