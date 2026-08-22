//
// Dot torus, glowing
//
// The vertices of assets/torus.obj, each stamping a 5×5 blob into a
// framebuffer that is never cleared. Blobs add where they overlap. After
// the stamps, a 4-neighbour diffuse blur (no self) subtracts TRAIL_DECAY,
// so each vertex leaves a trail that fades over a fixed number of steps.
//
// The mesh is a 10×10 torus whose hole sits well left of the origin; the
// right-hand tube passes through it. The turn is about a point on the
// tube, not the hole, so the body sweeps a wide orbit.
//
// The turn is not in SO(3). A table of 720 pairs
//
//   (cos θ,  SQUASH sin θ),   θ = 2π i / 720
//
// is fed to sequential Rx, Ry, Rz. Each plane map scales that plane by
//
//   r(θ) = √(cos²θ + SQUASH² sin²θ)   ∈ [SQUASH, 1]
//
// and leaves its axis alone, so the composition is not in SO(3) except
// at θ = 0, π (where r = 1). The model is squashed as it turns and the
// trails swell and shrink instead of tracing the same path.
//
// DEMO_Update is a fixed 1/60 s step, so a step crosses speed/60 = 10/3
// table slots. Every integer slot from ceil(10/3) = 4 behind the new
// phase through the phase is stamped, so the 1/3 leftover does not leave
// a gap. The table itself divides 2π evenly.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrogfx.h"

#define BLOB_SIZE 5 // the blob stamped at each vertex, in pixels
#define BLOB_LEVEL 15 // brightness of one level of the blob
#define GLOW_COLORS 163 // palette entries the glow ramps over
#define TRAIL_DECAY 3 // brightness the blur takes off each step, so the trail fades
#define SQUASH 0.7 // how far the turn departs from a rotation
#define ROTATION_SPEED 200 // table entries travelled per second
#define PROJECTION_SCALE 1 // the model is built in pixels, so the projection adds no scale
#define SINE_VALUES 720

unsigned char Blob[BLOB_SIZE][BLOB_SIZE] = {
	{BLOB_LEVEL * 0, BLOB_LEVEL * 3, BLOB_LEVEL * 3, BLOB_LEVEL * 3, BLOB_LEVEL * 0},
	{BLOB_LEVEL * 2, BLOB_LEVEL * 4, BLOB_LEVEL * 4, BLOB_LEVEL * 4, BLOB_LEVEL * 2},
	{BLOB_LEVEL * 3, BLOB_LEVEL * 4, BLOB_LEVEL * 5, BLOB_LEVEL * 4, BLOB_LEVEL * 3},
	{BLOB_LEVEL * 2, BLOB_LEVEL * 4, BLOB_LEVEL * 4, BLOB_LEVEL * 4, BLOB_LEVEL * 2},
	{BLOB_LEVEL * 0, BLOB_LEVEL * 3, BLOB_LEVEL * 3, BLOB_LEVEL * 3, BLOB_LEVEL * 0}
};

float SinTable[SINE_VALUES];
float CosTable[SINE_VALUES];

void DEMO_Update(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	double moved = deltatime * ROTATION_SPEED;
	phase = fmod(phase + moved, SINE_VALUES);
	int iphase = phase;

	// Stamp every table slot crossed this step so the trail has no gaps.
	int slots = (int)ceil(moved);
	if (slots < 1) {
		slots = 1;
	}

	Model3D *model = RETRO_Get3DModel();
	Vertex *vertex = model->vertex;

	// Draw blobs
	for (int step = 0; step < slots; step++) {
		int turn = (iphase - slots + 1 + step + SINE_VALUES) % SINE_VALUES;

		for (int p = 0; p < model->vertices; p++) {
			RETRO_SpinVertex(&vertex[p], CosTable[turn], SinTable[turn]);
			RETRO_ProjectVertex(&vertex[p], PROJECTION_SCALE);

			for (int y = 0; y < BLOB_SIZE; y++) {
				for (int x = 0; x < BLOB_SIZE; x++) {
					int px = vertex[p].sx + x;
					int py = vertex[p].sy + y;

					if (px < 0 || px >= RETRO_WIDTH || py < 0 || py >= RETRO_HEIGHT) {
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
	// Init tables
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * 2 * M_PI / SINE_VALUES) * SQUASH;
		CosTable[i] = cos(i * 2 * M_PI / SINE_VALUES);
	}

	// Init palette. Where the dots pile up, red ~ intensity² and white ~ intensity⁷, so the
	// glow stays red for a long time and only the hottest pile-ups go white.
	for (int i = 0; i < GLOW_COLORS; i++) {
		double intensity = (double) i / (GLOW_COLORS - 10);
		unsigned char red = GLOW_COLORS * pow(intensity, 2);
		unsigned char white = GLOW_COLORS * pow(intensity, 7);
		RETRO_SetColor(i, red, white, white);
	}

	RETRO_Load3DModel("assets/torus.obj");
}
