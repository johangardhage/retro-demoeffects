//
// Plane 3
//
// The same floor as Plane 4, using the usual ray-plane hit instead of
// Cramer's rule. The camera sits at the origin looking toward +Z with
// focal length D = PLANE_DISTANCE. Screen point (sx, sy) is the ray
//
//   r = (sx − W/2, sy − H/2, D)
//
// The floor is the horizontal plane y = PLANE_Y. Every point on the ray
// is λ r, so it hits the floor when λ r_y = PLANE_Y:
//
//   λ = PLANE_Y / r_y
//   hit = λ r
//
// Unrotated, U = (P, 0, 0) and V = (0, 0, P) with P = PLANE_PERIOD, so
// s = 1 or t = 1 is one texture width. Yaw about Y by θ = ang is
//
//   Ry(θ) (x, y, z) = (x cos θ + z sin θ, y, −x sin θ + z cos θ)
//
// which sends
//
//   U = (P cos θ,  0, −P sin θ)
//   V = (P sin θ,  0,  P cos θ)
//
// They stay orthogonal and length P, so the hit's (s, t) on that frame is
//
//   s = (hit − bp) · U / P²
//   t = (hit − bp) · V / P²
//
// Walk (xd, yd) is along those axes, not along world XZ — wrapping them
// on P is then one texture period at any yaw:
//
//   bp = (xd / P) U + (yd / P) V + (0, PLANE_Y, 0)
//      = (xd cos θ + yd sin θ, PLANE_Y, −xd sin θ + yd cos θ)
//
// On a scanline r_y is constant, so λ is constant and hit_z is constant.
// hit_x steps by λ per pixel of sx, and
//
//   Δs = λ U_x / P²,   Δt = λ V_x / P²
//
// The texel is (s P, t P), wrapped. This is the same intersection as
// Plane 4; Cramer's a/c and b/c are these dots. Rows above PLANE_FIRST_ROW
// are left undrawn (r_y is small, λ blows up). xd, yd live on P; ang
// lives on 360°. The texture is a generated P×P wrapping checker of
// FIELD_TILE-texel squares. Color 0 is the sky (the cleared upper half).
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define PLANE_DISTANCE 320
#define PLANE_PERIOD 256 // one texture width; U, V, xd and yd share it
#define PLANE_Y -16.0f // world y of the floor
#define PLANE_YAW_SPEED 24.0 // degrees per second
#define PLANE_WALK_SPEED 80.0 // texels per second
#define PLANE_FIRST_ROW 140 // skip rows near the horizon at H/2
#define FIELD_TILE 64

unsigned char Field[PLANE_PERIOD * PLANE_PERIOD];

void DEMO_Render(double time, double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();

	// Yaw the texture axes about Y, and walk along those axes. Wrapping
	// (xd, yd) on P is one texture period, so the floor does not jump.
	float ang = fmod(time * PLANE_YAW_SPEED, 360.0);
	float cosa = cos(ang * DEG2RAD);
	float sina = sin(ang * DEG2RAD);

	float xd = fmod(time * PLANE_WALK_SPEED, PLANE_PERIOD);
	float yd = fmod(time * PLANE_WALK_SPEED, PLANE_PERIOD);

	// Rotate U = (P, 0, 0) and V = (0, 0, P) by Ry(ang). bp is the
	// world-space origin of that frame: (xd / P) U + (yd / P) V, sitting
	// on y = PLANE_Y.
	Point3Df up = { PLANE_PERIOD * cosa, 0, -PLANE_PERIOD * sina };
	Point3Df vp = { PLANE_PERIOD * sina, 0, PLANE_PERIOD * cosa };
	Point3Df bp = { xd * cosa + yd * sina, PLANE_Y, -xd * sina + yd * cosa };

	const float invp2 = 1.0f / (PLANE_PERIOD * PLANE_PERIOD);

	// Draw the floor. Each scanline is one r_y, so λ = PLANE_Y / r_y is
	// constant and the hit only walks in x. Start at the left edge
	// (sx = −W/2) and step (s, t) by λ (U_x, V_x) / P², one pixel of sx.
	for (int y = PLANE_FIRST_ROW; y < RETRO_HEIGHT; y++) {
		float ry = y - (RETRO_HEIGHT / 2);
		float lambda = PLANE_Y / ry;
		float sx0 = -(RETRO_WIDTH / 2);
		float hitx = lambda * sx0;
		float hitz = lambda * PLANE_DISTANCE;
		float dx = hitx - bp.x;
		float dz = hitz - bp.z;

		// s, t at the left edge: (hit − bp) projected onto U and V
		float s = (dx * up.x + dz * up.z) * invp2;
		float t = (dx * vp.x + dz * vp.z) * invp2;
		float ds = lambda * up.x * invp2;
		float dt = lambda * vp.x * invp2;

		unsigned char *row = dest + y * RETRO_WIDTH;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// s = 1 is one U, i.e. one texture width, so the texel is (s P, t P)
			int tx = WRAP(s * PLANE_PERIOD, PLANE_PERIOD);
			int ty = WRAP(t * PLANE_PERIOD, PLANE_PERIOD);
			row[x] = Field[ty * PLANE_PERIOD + tx];
			s += ds;
			t += dt;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
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
