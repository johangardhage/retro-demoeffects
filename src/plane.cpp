//
// Plane
//
// A mode-7 floor: the horizontal plane y = −16, spanned by U = (256, 0, 0)
// and V = (0, 0, 256), seen from the origin with focal length D = 320.
// Screen point (sx, sy) is the ray (sx − W/2, sy − H/2, D). Cramer's
// rule for the intersection bp + s U + t V = λ ray gives
//
//   s = a / c,   t = b / c
//
// with (a, b, c) linear in (sx, sy). Along a scanline c is constant, so
// (s, t) is a DDA in 24-bit fixed point (× 2²⁴); the texel is bits 16–23
// (s · 256). bp = (φ, −16, φ) walks the texture along the diagonal; φ
// lives on 256, one texture period. Rows above y = 140, near the horizon
// at y = H/2, are left undrawn.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define PLANE_DISTANCE 320
#define PLANE_PERIOD 256 // one texture width; U, V and φ share it
#define PLANE_FIXED (1 << 24) // 24-bit fixed; texel in bits 16–23
#define PLANE_EPSILON 65536.0f // closest c gets to the horizon before 1/c is capped

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * 20, PLANE_PERIOD);

	Point3Df bp = { (float)phase, -16, (float)phase };
	Point3Df up = { (float)PLANE_PERIOD, 0, 0 };
	Point3Df vp = { 0, 0, (float)PLANE_PERIOD };

	unsigned char *image = RETRO_ImageData();

	// Draw a perspective correct textured plane
	float cx = up.y * vp.z - vp.y * up.z;
	float cy = vp.x * up.z - up.x * vp.z;
	float cz = (up.x * vp.y - vp.x * up.y) * PLANE_DISTANCE;
	float ax = vp.y * bp.z - bp.y * vp.z;
	float ay = bp.x * vp.z - vp.x * bp.z;
	float az = (vp.x * bp.y - bp.x * vp.y) * PLANE_DISTANCE;
	float bx = bp.y * up.z - up.y * bp.z;
	float by = up.x * bp.z - bp.x * up.z;
	float bz = (bp.x * up.y - up.x * bp.y) * PLANE_DISTANCE;

	// Only render the lower part of the plane
	for (int y = 140; y < RETRO_HEIGHT; y++) {
		// Compute the (U,V) coordinates and the interpolation
		float a = az + ay * (y - (RETRO_HEIGHT / 2)) + ax * -(RETRO_WIDTH / 2);
		float b = bz + by * (y - (RETRO_HEIGHT / 2)) + bx * -(RETRO_WIDTH / 2);
		float c = cz + cy * (y - (RETRO_HEIGHT / 2)) + cx * -(RETRO_WIDTH / 2);

		// Near the horizon c goes to zero. The guard keeps its sign.
		float ic = fabs(c) > PLANE_EPSILON ? 1 / c : copysignf(1 / PLANE_EPSILON, c);

		// Wrapped into one texture period before it is fixed-pointed, or a row
		// near the horizon overflows int
		int u = (int)(fmod(a * ic, (double)PLANE_PERIOD) * PLANE_FIXED);
		int v = (int)(fmod(b * ic, (double)PLANE_PERIOD) * PLANE_FIXED);
		int du = (int)(ax * ic * PLANE_FIXED);
		int dv = (int)(bx * ic * PLANE_FIXED);

		for (int x = 0; x < RETRO_WIDTH; x++) {
			unsigned char color = image[((v >> 8) & 0xff00) + ((u >> 16) & 0xff)];
			RETRO_PutPixel(x, y, color);

			u += du;
			v += dv;
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());
}
