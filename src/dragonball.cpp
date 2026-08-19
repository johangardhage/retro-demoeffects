//
// Dragon ball
//
// Sync/Dreamdealers' DragonBall (and Robster's of the same name): a filled
// ellipse that stands in for a sphere, six five-pointed stars sitting on it,
// bouncing and squashing onto a copper-split floor that reflects the ball.
//
// The ball is not a mesh. It is an axis-aligned ellipse. Airborne it is a
// ballistic parabola, y = APEX + ½ g t², hitting the floor at speed v. Contact
// is a linear spring of half-period T_contact = π / ω = π D / v, so the centre keeps
// that speed at both ends of the squash and the compression is
//
//   p = (v / ω) sin(ω τ),   ω = v / D
//
// peaking at D = R − R_min. The bottom stays pinned,
//
//   ra + rb = 2 R,   cy + rb = FLOOR
//
// so the radii inherit p' = v cos(ω τ). At takeoff that slope is −v; a damped
// oscillator at a slightly higher ω continues it,
//
//   d(t) = e^{−ζ ω t} (−v / ω_d) sin(ω_d t)
//
// which is C¹ in both the centre and the radii: the pancake unsquashes through
// a circle into a vertical stretch and settles on the way up.
//
// Six stars live on the sphere. Three of them sit in the planes x = +c,
// y = −c, z = +c as 10-vertex pentagrams, and the other three are their
// antipodes −P. Each star also spins in its own plane. After the sphere's
// Euler rotation they are projected with a pinhole and then scaled by
// (ra/R, rb/R) so they squash with the ellipse. The rotated plane normal
// chooses the bit: facing the camera adds 4, facing away adds 2, the ellipse
// itself is 1. Those three bits pick a colour, and a scanline above, on, or
// below the horizon picks which of the three banks that 8 is drawn from —
// sky, floor, or the reflected copy of the scanlines above the mirror.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define BALL_RADIUS 70.0f
#define BALL_APEX 72.0f // centre at the top of the bounce
#define BALL_FLOOR 200.0f // scanline the bottom of the ellipse is pinned to
#define BALL_RBMIN (30.0f / 65.0f * BALL_RADIUS) // original 30 on a radius of 65
#define BALL_AIR (BALL_FLOOR - BALL_RADIUS - BALL_APEX) // apex to the contact point
#define BALL_PENMAX (BALL_RADIUS - BALL_RBMIN) // peak spring compression, pixels
#define BALL_TDOWN 0.58f // seconds of free fall, and of the rise after takeoff
#define BALL_G (2.0f * BALL_AIR / (BALL_TDOWN * BALL_TDOWN))
#define BALL_VHIT (BALL_G * BALL_TDOWN)
#define BALL_OMEGA (BALL_VHIT / BALL_PENMAX) // contact spring; p_max = v / ω
#define BALL_TCONTACT ((float)M_PI / BALL_OMEGA)
#define BALL_PERIOD (2.0f * BALL_TDOWN + BALL_TCONTACT)
#define BALL_ZETA 0.55f // underdamping of the takeoff oscillator
#define BALL_WOBBLE_OMEGA (BALL_OMEGA * 1.4f) // faster than contact so the stretch overshoots less
#define BALL_WD (BALL_WOBBLE_OMEGA * sqrtf(1.0f - BALL_ZETA * BALL_ZETA))
#define BALL_HORIZON 158 // copper split, sky above, floor below
#define BALL_REFLECTION 200 // first mirrored scanline, the copper's negative modulo
#define BALL_SPEEDX 0.87f // radians a second, the original's 1/2/3 degrees a PAL frame
#define BALL_SPEEDY 1.75f
#define BALL_SPEEDZ -2.62f
#define BALL_CX (RETRO_WIDTH / 2)
#define BALL_EYE 3.2f // closer than RETRO_PROJECTION_EYE so the sphere bulges

#define STAR_COUNT 6
#define STAR_POINTS 5
#define STAR_VERTS (STAR_POINTS * 2)
#define STAR_OFFSET 0.962f // plane the pentagram sits in, original 875/910
#define STAR_OUTER 0.275f // original 250/910
#define STAR_INNER 0.165f // original 150/910
#define STAR_SPIN 3.49f // radians a second, the original's 4 degrees a PAL frame

#define BIT_BALL 1
#define BIT_BACK 2 // far side of the sphere, teal
#define BIT_FRONT 4 // near side, blue
#define BANK_FLOOR 8
#define BANK_REFLECT 16

// Continuation of the contact spring after takeoff. d(0) = 0 and d'(0) = −v
// so the radii stay C¹ with p = (v/ω) sin(ω τ) at τ = π/ω.
static float BallDeform(float t)
{
	return exp(-BALL_ZETA * BALL_WOBBLE_OMEGA * t) * (-BALL_VHIT / BALL_WD) * sin(BALL_WD * t);
}

// Even-odd fill of a concave pentagram. Pixels already clear of the ellipse
// stay clear: a star that would leak past the silhouette is clipped to it.
static void FillStar(const Point2D *pts, int n, unsigned char bit)
{
	int ymin = RETRO_HEIGHT;
	int ymax = -1;
	for (int i = 0; i < n; i++) {
		ymin = MIN(ymin, pts[i].y);
		ymax = MAX(ymax, pts[i].y);
	}
	ymin = MAX(ymin, 0);
	ymax = MIN(ymax, RETRO_HEIGHT - 1);
	if (ymin > ymax) {
		return;
	}

	for (int y = ymin; y <= ymax; y++) {
		int xs[STAR_VERTS];
		int nx = 0;
		for (int i = 0; i < n; i++) {
			int x0 = pts[i].x;
			int y0 = pts[i].y;
			int x1 = pts[(i + 1) % n].x;
			int y1 = pts[(i + 1) % n].y;
			if (y0 == y1) {
				continue;
			}
			if (y0 > y1) {
				SWAP(x0, x1);
				SWAP(y0, y1);
			}
			if (y >= y0 && y < y1) {
				xs[nx++] = x0 + (int)((x1 - x0) * (double)(y - y0) / (y1 - y0));
			}
		}
		for (int a = 1; a < nx; a++) {
			int v = xs[a];
			int b = a;
			while (b > 0 && xs[b - 1] > v) {
				xs[b] = xs[b - 1];
				b--;
			}
			xs[b] = v;
		}

		unsigned char *row = RETRO.framebuffer + y * RETRO_WIDTH;
		for (int k = 0; k + 1 < nx; k += 2) {
			int x0 = MAX(xs[k], 0);
			int x1 = MIN(xs[k + 1], RETRO_WIDTH - 1);
			for (int x = x0; x <= x1; x++) {
				if (row[x] & BIT_BALL) {
					row[x] |= bit;
				}
			}
		}
	}
}

static void BuildStar(Vertex *out, int axis, bool antipode, float spin)
{
	for (int i = 0; i < STAR_VERTS; i++) {
		float a = (float)i * M_PI / STAR_POINTS + spin;
		float r = (i & 1) ? STAR_INNER : STAR_OUTER;
		float u = r * cos(a);
		float v = r * sin(a);
		float x, y, z;
		if (axis == 0) {
			x = STAR_OFFSET;
			y = u;
			z = v;
		} else if (axis == 1) {
			x = u;
			y = -STAR_OFFSET;
			z = v;
		} else {
			x = u;
			y = v;
			z = STAR_OFFSET;
		}
		if (antipode) {
			x = -x;
			y = -y;
			z = -z;
		}
		int dst = antipode ? STAR_VERTS - 1 - i : i;
		out[dst].x = x;
		out[dst].y = y;
		out[dst].z = z;
	}
}

void DEMO_Render(double deltatime)
{
	static double t = 0;
	t = fmod(t + deltatime, BALL_PERIOD);

	static float ax, ay, az, spin;
	ax = fmod(ax + deltatime * BALL_SPEEDX, 2 * M_PI);
	ay = fmod(ay + deltatime * BALL_SPEEDY, 2 * M_PI);
	az = fmod(az + deltatime * BALL_SPEEDZ, 2 * M_PI);
	spin = fmod(spin + deltatime * STAR_SPIN, 2 * M_PI);

	// First fall has no previous takeoff to continue, so the oscillator
	// only runs after the ball has hit the floor once.
	static bool launched = false;
	if (t >= BALL_TDOWN) {
		launched = true;
	}

	float ra, rb, cy;
	if (t < BALL_TDOWN) {
		float tau = (float)t;
		cy = BALL_APEX + 0.5f * BALL_G * tau * tau;
		float deform = launched ? BallDeform(tau + BALL_TDOWN) : 0.0f;
		ra = BALL_RADIUS + deform;
		rb = BALL_RADIUS - deform;
	} else if (t < BALL_TDOWN + BALL_TCONTACT) {
		float tau = (float)t - BALL_TDOWN;
		float pen = BALL_PENMAX * sin(BALL_OMEGA * tau);
		ra = BALL_RADIUS + pen;
		rb = BALL_RADIUS - pen;
		cy = BALL_FLOOR - rb;
	} else {
		float tau = (float)t - BALL_TDOWN - BALL_TCONTACT;
		cy = (BALL_FLOOR - BALL_RADIUS) - BALL_VHIT * tau + 0.5f * BALL_G * tau * tau;
		float deform = BallDeform(tau);
		ra = BALL_RADIUS + deform;
		rb = BALL_RADIUS - deform;
	}

	if (rb < 1.0f) {
		rb = 1.0f;
		ra = 2.0f * BALL_RADIUS - rb;
	}

	// Floor, then the ball, then the stars. Background 0 is already the sky.
	for (int y = BALL_HORIZON; y < BALL_REFLECTION; y++) {
		memset(RETRO.framebuffer + y * RETRO_WIDTH, BANK_FLOOR, RETRO_WIDTH);
	}

	RETRO_DrawEllipse((float)BALL_CX, cy, ra, rb, BIT_BALL);

	// The ellipse is a single palette index; the copper split is the floor
	// bank ORed onto every ball pixel on or below the horizon.
	int y0 = MAX((int)ceil(cy - rb), BALL_HORIZON);
	int y1 = MIN((int)floor(cy + rb), RETRO_HEIGHT - 1);
	for (int y = y0; y <= y1; y++) {
		unsigned char *row = RETRO.framebuffer + y * RETRO_WIDTH;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			if (row[x] & BIT_BALL) {
				row[x] |= BANK_FLOOR;
			}
		}
	}

	for (int s = 0; s < STAR_COUNT; s++) {
		int axis = s % 3;
		bool antipode = s >= 3;
		Vertex star[STAR_VERTS];
		BuildStar(star, axis, antipode, spin);

		Vertex normal;
		normal.x = (axis == 0) ? STAR_OFFSET : 0.0f;
		normal.y = (axis == 1) ? -STAR_OFFSET : 0.0f;
		normal.z = (axis == 2) ? STAR_OFFSET : 0.0f;
		if (antipode) {
			normal.x = -normal.x;
			normal.y = -normal.y;
			normal.z = -normal.z;
		}
		RETRO_RotateVertex(&normal, ax, ay, az);

		Point2D pts[STAR_VERTS];
		for (int i = 0; i < STAR_VERTS; i++) {
			RETRO_RotateVertex(&star[i], ax, ay, az);
			float depth = BALL_EYE + star[i].rz;
			if (depth < 0.1f) {
				depth = 0.1f;
			}
			float q = 1.0f / depth;
			pts[i].x = lround(BALL_CX + ra * star[i].rx * BALL_EYE * q);
			pts[i].y = lround(cy + rb * star[i].ry * BALL_EYE * q);
		}

		unsigned char bit = normal.rz < 0.0f ? BIT_FRONT : BIT_BACK;
		FillStar(pts, STAR_VERTS, bit);
	}

	// Copper negative-modulo: every line from REFLECTION down is the line the
	// same distance above it, remapped into the reflection bank
	for (int y = BALL_REFLECTION; y < RETRO_HEIGHT; y++) {
		int ysrc = 2 * BALL_REFLECTION - 1 - y;
		if (ysrc < 0) {
			memset(RETRO.framebuffer + y * RETRO_WIDTH, BANK_REFLECT, RETRO_WIDTH);
			continue;
		}
		unsigned char *src = RETRO.framebuffer + ysrc * RETRO_WIDTH;
		unsigned char *dst = RETRO.framebuffer + y * RETRO_WIDTH;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			dst[x] = (src[x] & 7) | BANK_REFLECT;
		}
	}
}

void DEMO_Initialize(void)
{
	// Three banks of the same 8 slots: sky, floor, reflection. The body is
	// seagreen, far-side stars mediumseagreen, near-side stars steelblue.
	// Unused combinations (star bits without the ball) still get a colour
	// so a clipped-out leak does not flash palette 0.
	RETRO_SetColor(0, RETRO_REBECCAPURPLE);
	RETRO_SetColor(BIT_BALL, RETRO_SEAGREEN);
	RETRO_SetColor(BIT_BACK, RETRO_SEAGREEN);
	RETRO_SetColor(BIT_BALL | BIT_BACK, RETRO_MEDIUMSEAGREEN);
	RETRO_SetColor(BIT_FRONT, RETRO_STEELBLUE);
	RETRO_SetColor(BIT_BALL | BIT_FRONT, RETRO_STEELBLUE);
	RETRO_SetColor(BIT_BACK | BIT_FRONT, RETRO_STEELBLUE);
	RETRO_SetColor(BIT_BALL | BIT_BACK | BIT_FRONT, RETRO_LIGHTSKYBLUE);

	RETRO_SetColor(BANK_FLOOR, RETRO_INDIGO);
	RETRO_SetColor(BANK_FLOOR | BIT_BALL, RETRO_TEAL);
	RETRO_SetColor(BANK_FLOOR | BIT_BACK, RETRO_TEAL);
	RETRO_SetColor(BANK_FLOOR | BIT_BALL | BIT_BACK, RETRO_SEAGREEN);
	RETRO_SetColor(BANK_FLOOR | BIT_FRONT, RETRO_DARKCYAN);
	RETRO_SetColor(BANK_FLOOR | BIT_BALL | BIT_FRONT, RETRO_DARKCYAN);
	RETRO_SetColor(BANK_FLOOR | BIT_BACK | BIT_FRONT, RETRO_DARKCYAN);
	RETRO_SetColor(BANK_FLOOR | BIT_BALL | BIT_BACK | BIT_FRONT, RETRO_STEELBLUE);

	RETRO_SetColor(BANK_REFLECT, RETRO_INDIGO);
	RETRO_SetColor(BANK_REFLECT | BIT_BALL, RETRO_DARKSLATEGRAY);
	RETRO_SetColor(BANK_REFLECT | BIT_BACK, RETRO_DARKSLATEGRAY);
	RETRO_SetColor(BANK_REFLECT | BIT_BALL | BIT_BACK, RETRO_TEAL);
	RETRO_SetColor(BANK_REFLECT | BIT_FRONT, RETRO_MIDNIGHTBLUE);
	RETRO_SetColor(BANK_REFLECT | BIT_BALL | BIT_FRONT, RETRO_MIDNIGHTBLUE);
	RETRO_SetColor(BANK_REFLECT | BIT_BACK | BIT_FRONT, RETRO_MIDNIGHTBLUE);
	RETRO_SetColor(BANK_REFLECT | BIT_BALL | BIT_BACK | BIT_FRONT, RETRO_DARKCYAN);
}
