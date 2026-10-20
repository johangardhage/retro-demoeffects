//
// Dot torus, glowing
//
// The vertices of torus.obj, each stamping a 5×5 blob into a
// framebuffer that is never cleared. Blobs add where they overlap. After
// the stamps, a 4-neighbor diffuse blur (no self) subtracts TRAIL_DECAY,
// so each vertex leaves a trail that fades over a fixed number of steps.
//
// The mesh is a 10×10 torus whose hole sits well left of the origin; the
// right-hand tube passes through it. The turn is about a point on the
// tube, not the hole, so the body sweeps a wide orbit.
//
// The turn is not in SO(3). The pair
//
//   (cos θ,  SQUASH sin θ)
//
// is fed to sequential Rx, Ry, Rz. Each plane map scales that plane by
//
//   r(θ) = √(cos²θ + SQUASH² sin²θ)   ∈ [SQUASH, 1]
//
// and leaves its axis alone, so the composition is not in SO(3) except
// at θ = 0, π (where r = 1). The model is squashed as it turns and the
// trails swell and shrink instead of tracing the same path.
//
// DEMO_FixedUpdate is a fixed 1/60 s step, so a step turns θ by speed/60,
// 10/3 STAMP_SPACING. The step is split into ceil(10/3) = 4 evenly
// spaced stamps ending on the new θ, so no two stamps of a vertex are
// further apart than STAMP_SPACING and the trail has no gap.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrogfx.h"

constexpr int BLOB_SIZE = 5; // the blob stamped at each vertex, in pixels
constexpr int BLOB_LEVEL = 15; // brightness of one level of the blob
constexpr int GLOW_COLORS = 163; // palette entries the glow ramps over
constexpr int TRAIL_DECAY = 3; // brightness the blur takes off each step, so the trail fades
constexpr double SQUASH = 0.7; // how far the turn departs from a rotation
constexpr double ROTATION_SPEED = radians(100); // the turn advances per second
constexpr double STAMP_SPACING = radians(0.5); // at most between two stamps of a vertex
constexpr int PROJECTION_SCALE = 1; // the model is built in pixels, so the projection adds no scale

static const unsigned char Blob[BLOB_SIZE][BLOB_SIZE] = {
	{BLOB_LEVEL * 0, BLOB_LEVEL * 3, BLOB_LEVEL * 3, BLOB_LEVEL * 3, BLOB_LEVEL * 0},
	{BLOB_LEVEL * 2, BLOB_LEVEL * 4, BLOB_LEVEL * 4, BLOB_LEVEL * 4, BLOB_LEVEL * 2},
	{BLOB_LEVEL * 3, BLOB_LEVEL * 4, BLOB_LEVEL * 5, BLOB_LEVEL * 4, BLOB_LEVEL * 3},
	{BLOB_LEVEL * 2, BLOB_LEVEL * 4, BLOB_LEVEL * 4, BLOB_LEVEL * 4, BLOB_LEVEL * 2},
	{BLOB_LEVEL * 0, BLOB_LEVEL * 3, BLOB_LEVEL * 3, BLOB_LEVEL * 3, BLOB_LEVEL * 0}
};

void DEMO_FixedUpdate(RETRO_Time time)
{
	// Calculate phase
	static double phase = 0;
	double start = phase;
	double moved = time.delta * ROTATION_SPEED;
	phase = fmod(phase + moved, 2 * M_PI);

	// Split the step into stamps no further apart than STAMP_SPACING so the trail has no gaps.
	int stamps = MAX((int)ceil(moved / STAMP_SPACING), 1);

	Model3D *model = RETRO_Get3DModel();
	Vertex *vertex = model->vertex;

	// Draw blobs
	for (int step = 0; step < stamps; step++) {
		double turn = start + moved * (step + 1) / stamps;
		float c = cos(turn);
		float s = SQUASH * sin(turn);
		// Not a rotation: the same (cos, sin) pair on all three axes. Each
		// plane map scales that plane by r = sqrt(cos^2+sin^2) and leaves its
		// axis alone, so a pair that is not unit squashes the model as it
		// turns and the composition is not in SO(3).
		mat3 matrix = rotate(c, s, c, s, c, s);

		for (int p = 0; p < model->vertices; p++) {
			RETRO_RotateVertex(&vertex[p], matrix);
			RETRO_ProjectVertex(&vertex[p], PROJECTION_SCALE);

			for (int y = 0; y < BLOB_SIZE; y++) {
				for (int x = 0; x < BLOB_SIZE; x++) {
					int px = vertex[p].spos.x + x;
					int py = vertex[p].spos.y + y;

					if (!RETRO_OnScreen(px, py)) {
						continue;
					}

					int color = RETRO_GetPixel(px, py) + Blob[y][x];

					RETRO_PutPixel(px, py, CLAMP(color, 0, GLOW_COLORS));
				}
			}
		}
	}

	// Blur trail
	RETRO_Blur(RETRO_BLUR_DIFFUSE, TRAIL_DECAY);
}

void DEMO_Initialize(void)
{
	// Init palette. Where the dots pile up, red ~ intensity² and white ~ intensity⁷, so the
	// glow stays red for a long time and only the hottest pile-ups go white.
	for (int i = 0; i < GLOW_COLORS; i++) {
		double intensity = (double)i / (GLOW_COLORS - 10);
		unsigned char red = GLOW_COLORS * pow(intensity, 2);
		unsigned char white = GLOW_COLORS * pow(intensity, 7);
		RETRO_SetColor(i, red, white, white);
	}

	RETRO_Load3DModel("assets/torus.obj");
}
