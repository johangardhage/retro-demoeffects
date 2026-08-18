//
// Dot morph
//
// A cloud of points that is a torus, then a sphere, then a torus again. Both
// shapes hold the same POINTS identities, so the cloud is the linear interpolant
//
//   p(t) = (1 − t) p_from + t p_to,   t ∈ [0, 1]
//
// The two shapes are sampled independently, so point i is not the same place
// on both and a point crosses the interior rather than sliding over a
// surface. That scatter is the effect, not an error in it.
//
// The cycle is morph, hold, morph back, hold (256 + 512 + 256 + 512).
// t = phase / (MORPH_STEPS − 1) reaches exactly 1, so each change arrives
// on its shape rather than just short of it. The phase lives on the cycle.
//
// The shade is monotone in depth, so keeping the brighter of two dots on one
// pixel is an exact depth test.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrocolor.h"

#define POINTS 4096
#define PROJECTION_SCALE 80 // the shapes are built at unit size, so the projection scales them to pixels
#define ROTATION_SPEED 100 // degrees per second

#define TORUS_RING 0.8 // distance from the axis out to the middle of the tube
#define TORUS_TUBE 0.4 // and the radius of the tube itself

#define MORPH_STEPS 256 // steps a change from one shape to the other takes
#define HOLD_STEPS 512 // and steps a shape is held for once it arrives
#define CYCLE_STEPS (2 * (MORPH_STEPS + HOLD_STEPS)) // morph, hold, morph back, hold
#define MORPH_SPEED 200 // steps per second

Vertex Sphere[POINTS];
Vertex Torus[POINTS];
Vertex Morph[POINTS];

void MorphShapes(Vertex *from, Vertex *to, float t)
{
	for (int i = 0; i < POINTS; i++) {
		Morph[i].x = from[i].x + (to[i].x - from[i].x) * t;
		Morph[i].y = from[i].y + (to[i].y - from[i].y) * t;
		Morph[i].z = from[i].z + (to[i].z - from[i].z) * t;
	}
}

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * MORPH_SPEED, CYCLE_STEPS);
	int iphase = phase;

	static double angle = 0;
	angle = fmod(angle + deltatime * ROTATION_SPEED * DEG2RAD, 2 * M_PI);

	// Morph shapes
	if (iphase < MORPH_STEPS) {
		MorphShapes(Torus, Sphere, (float)iphase / (MORPH_STEPS - 1));
	} else if (iphase >= MORPH_STEPS + HOLD_STEPS && iphase < 2 * MORPH_STEPS + HOLD_STEPS) {
		MorphShapes(Sphere, Torus, (float)(iphase - MORPH_STEPS - HOLD_STEPS) / (MORPH_STEPS - 1));
	}

	// No point is further from the centre than the torus's outer rim, and
	// rotation cannot change that, so the shade ramp covers [-r, r] in depth.
	double furthest = TORUS_RING + TORUS_TUBE;

	// Draw points
	for (int i = 0; i < POINTS; i++) {
		RETRO_RotateVertex(&Morph[i], 0, angle, angle);
		RETRO_ProjectVertex(&Morph[i], PROJECTION_SCALE);

		int x = Morph[i].sx;
		int y = Morph[i].sy;

		if (x >= 0 && x < RETRO_WIDTH && y >= 0 && y < RETRO_HEIGHT) {
			unsigned char color = CLAMP256((RETRO_COLORS - 1) * (furthest - Morph[i].rz) / (2 * furthest));

			if (color > RETRO_GetPixel(x, y)) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_WHITE);

	// Init sphere. A band of a unit sphere between two heights has area that depends only on
	// how far apart they are, so drawing z ~ U[-1, 1] and φ ~ U[0, 2π) is
	// uniform on the surface:
	//
	//   r = sqrt(1 - z^2),   (x, y) = r (cos φ, sin φ)
	//
	for (int i = 0; i < POINTS; i++) {
		float z = 1 - RANDOMF(2);
		float r = sqrt(1 - z * z);
		float phi = RANDOMF(2 * M_PI);

		Sphere[i].x = r * cos(phi);
		Sphere[i].y = r * sin(phi);
		Sphere[i].z = z;
	}

	// Init torus. The area element of a torus carries (R + r cos θ). Drawing θ uniformly
	// would crowd the inner rim. Keep a candidate only with probability
	// proportional to that factor:
	//
	//   accept if U * (R + r) < R + r cos θ
	//
	for (int i = 0; i < POINTS; i++) {
		float theta;
		do {
			theta = RANDOMF(2 * M_PI);
		} while (RANDOMF(1) * (TORUS_RING + TORUS_TUBE) > TORUS_RING + TORUS_TUBE * cos(theta));

		float phi = RANDOMF(2 * M_PI);
		float ring = TORUS_RING + TORUS_TUBE * cos(theta);

		Torus[i].x = ring * cos(phi);
		Torus[i].y = ring * sin(phi);
		Torus[i].z = TORUS_TUBE * sin(theta);
	}

	MorphShapes(Torus, Torus, 0);
}
