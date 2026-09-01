//
// Plane 4
//
// The same floor as Plane 3, using Cramer's rule instead of the ray-plane
// equation. The camera sits at the origin looking toward +Z with
// focal length D = PLANE_DISTANCE. Screen point (sx, sy) is the ray
//
//   r = (sx − W/2, sy − H/2, D)
//
// The floor is the horizontal plane y = PLANE_Y, parametrized as
//
//   p = bp + s U + t V
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
// Walk (xd, yd) is along those axes, not along world XZ — wrapping them
// on P is then one texture period at any yaw:
//
//   bp = (xd / P) U + (yd / P) V + (0, PLANE_Y, 0)
//      = (xd cos θ + yd sin θ, PLANE_Y, −xd sin θ + yd cos θ)
//
// The ray hits the plane when bp + s U + t V = λ r, or
//
//   s U + t V − λ r = −bp
//
// Cramer's rule, as scalar triple products with r = (sx, sy, D):
//
//   c = (U × V) · r
//   a = (V × bp) · r
//   b = (bp × U) · r
//   s = a / c,   t = b / c
//
// Expanding the triples in (sx, sy) gives the linear forms the inner
// loop uses, with D folded into the constant terms (az, bz, cz):
//
//   a = az + ay sy + ax sx
//   b = bz + by sy + bx sx
//   c = cz + cy sy + cx sx
//
// U and V have no Y component, so U × V is vertical, cx = cz = 0, and c
// is constant on a scanline. Then (s, t) advances by (ax, bx) / c per
// pixel. The texel is (s P, t P), wrapped. c → 0 at the horizon sy = 0;
// PLANE_EPSILON caps 1/c and keeps its sign. Rows above PLANE_FIRST_ROW
// are left undrawn. xd, yd live on P; ang lives on 360°.
// The texture is a generated P×P wrapping checker of FIELD_TILE-texel
// squares. Color 0 is the sky (the cleared upper half).
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
#define PLANE_EPSILON 65536.0f // closest |c| gets before 1/c is capped
#define FIELD_TILE 64

unsigned char Field[PLANE_PERIOD * PLANE_PERIOD];

void DEMO_Render(double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();

	// Yaw the texture axes about Y, and walk along those axes. Wrapping
	// (xd, yd) on P is one texture period, so the floor does not jump.
	static float ang = 0;
	ang = fmod(ang + deltatime * PLANE_YAW_SPEED, 360.0);
	float cosa = cos(ang * DEG2RAD);
	float sina = sin(ang * DEG2RAD);

	static float xd, yd;
	xd = fmod(xd + deltatime * PLANE_WALK_SPEED, PLANE_PERIOD);
	yd = fmod(yd + deltatime * PLANE_WALK_SPEED, PLANE_PERIOD);

	// Rotate U = (P, 0, 0) and V = (0, 0, P) by Ry(ang). bp is the
	// world-space origin of that frame: (xd / P) U + (yd / P) V, sitting
	// on y = PLANE_Y.
	Point3Df up = { PLANE_PERIOD * cosa, 0, -PLANE_PERIOD * sina };
	Point3Df vp = { PLANE_PERIOD * sina, 0, PLANE_PERIOD * cosa };
	Point3Df bp = { xd * cosa + yd * sina, PLANE_Y, -xd * sina + yd * cosa };

	// Cramer's rule for bp + s U + t V = λ r. Each triple product is
	// linear in the ray r = (sx, sy, D), so it splits into a constant
	// term (D folded in) and slopes along sx and sy:
	//   c = (U × V) · r,   a = (V × bp) · r,   b = (bp × U) · r
	float cx = up.y * vp.z - vp.y * up.z;
	float cy = vp.x * up.z - up.x * vp.z;
	float cz = (up.x * vp.y - vp.x * up.y) * PLANE_DISTANCE;
	float ax = vp.y * bp.z - bp.y * vp.z;
	float ay = bp.x * vp.z - vp.x * bp.z;
	float az = (vp.x * bp.y - bp.x * vp.y) * PLANE_DISTANCE;
	float bx = bp.y * up.z - up.y * bp.z;
	float by = up.x * bp.z - bp.x * up.z;
	float bz = (bp.x * up.y - up.x * bp.y) * PLANE_DISTANCE;

	// Draw the floor. Each scanline is one sy; c does not change along it
	// because U × V is vertical. Start at the left edge (sx = −W/2) and
	// step (s, t) by (ax, bx) / c, which is one pixel of sx.
	for (int y = PLANE_FIRST_ROW; y < RETRO_HEIGHT; y++) {
		float sy = y - (RETRO_HEIGHT / 2);
		float sx0 = -(RETRO_WIDTH / 2);
		float a = az + ay * sy + ax * sx0;
		float b = bz + by * sy + bx * sx0;
		float c = cz + cy * sy + cx * sx0;

		// Horizon is sy = 0, where c vanishes. Cap 1/c and keep its sign.
		float ic = fabs(c) > PLANE_EPSILON ? 1.0f / c : copysignf(1.0f / PLANE_EPSILON, c);
		float s = a * ic;
		float t = b * ic;
		float ds = ax * ic;
		float dt = bx * ic;

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
