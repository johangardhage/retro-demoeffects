//
// Plane
//
// A mode-7 floor. The camera sits a height PLANE_EYE above the plane.
// Row y is counted up from the bottom of the screen, so the pixel is
// written at HEIGHT − y. Screen point (x, y) looks at the plane at
//
//   u = PLANE_EYE · (W/2) / P · (x − W/2) / (y − H/2)
//   v = PLANE_EYE · (H/2) / (y − H/2)
//
// with P = PLANE_PERIOD. u and v are already texels: the (W/2) / P in u
// maps a half-screen of pixels onto one texture period at unit depth.
// v is the forward texel and is constant on a scanline; u is linear in x.
// The horizon is y = H/2, where the denominator is zero and the row is
// left undrawn. The loop runs y = 1 … PLANE_VIEW − 1, so it never hits it.
//
// Yaw about the vertical by θ = ang is a 2D rotation of that (u, v), then
// the camera walks (xd, yd) in texture space:
//
//   unew = xd + u cos θ − v sin θ
//   vnew = yd + u sin θ + v cos θ
//
// xd and yd are not wrapped; WRAP at the sample is one texture period.
// ang lives on 360°. The texture is a generated P×P wrapping checker of
// FIELD_TILE-texel squares. Color 0 is the sky (the cleared upper half).
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define PLANE_VIEW (RETRO_HEIGHT / 2)
#define PLANE_EYE 100 // camera height in world units
#define PLANE_YAW_SPEED 24.0 // degrees per second
#define PLANE_WALK_SPEED 80.0 // texels per second
#define PLANE_PERIOD 256 // one texture width; u, v, xd and yd share it
#define FIELD_TILE 64

unsigned char Field[PLANE_PERIOD * PLANE_PERIOD];

void DEMO_Render(double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();

	// Yaw the sampled (u, v) in the plane, and walk (xd, yd) in texture
	// space. WRAP at the sample is one texture period.
	static float ang = 0;
	ang = fmod(ang + deltatime * PLANE_YAW_SPEED, 360.0);
	float cosa = cos(ang * DEG2RAD);
	float sina = sin(ang * DEG2RAD);

	static float xd, yd;
	xd += deltatime * PLANE_WALK_SPEED;
	yd += deltatime * PLANE_WALK_SPEED;

	// Perspective scales. v is the forward texel at this depth and does
	// not depend on x. u stretches about the screen centre with a factor
	// (W/2) / P so a half-width of pixels is one texture period at unit
	// depth. midx, midy are the vanishing point (the horizon is midy).
	float xscale = PLANE_EYE * (RETRO_WIDTH / 2.0f) / PLANE_PERIOD;
	float yscale = PLANE_EYE * (RETRO_HEIGHT / 2.0f);
	float midx = RETRO_WIDTH / 2.0f;
	float midy = RETRO_HEIGHT / 2.0f;

	// Draw the floor. y is distance up from the bottom; the pixel sits at
	// HEIGHT − y. Each pixel divides by (y − midy) and then yaws.
	for (int y = 1; y < PLANE_VIEW; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float u = (xscale * (x - midx)) / (y - midy);
			float v = yscale / (y - midy);

			float unew = xd + u * cosa - v * sina;
			float vnew = yd + u * sina + v * cosa;

			dest[(RETRO_HEIGHT - y) * RETRO_WIDTH + x] = Field[WRAP(vnew, PLANE_PERIOD) * PLANE_PERIOD + WRAP(unew, PLANE_PERIOD)];
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Color 0 fills the uncleared sky. 1 and 2 are the checker.
	RETRO_SetColor(0, RETRO_SPACECADET);
	RETRO_SetColor(1, RETRO_HUNTERGREEN);
	RETRO_SetColor(2, RETRO_MOSSGREEN);

	// Init field
	for (int y = 0; y < PLANE_PERIOD; y++) {
		for (int x = 0; x < PLANE_PERIOD; x++) {
			int checker = ((x / FIELD_TILE) ^ (y / FIELD_TILE)) & 1;
			Field[y * PLANE_PERIOD + x] = checker ? 1 : 2;
		}
	}
}
