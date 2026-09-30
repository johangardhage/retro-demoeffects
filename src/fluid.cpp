//
// Fluid
//
// Two inks stirred into water. The same stable-fluids solver as smoke.cpp,
// with the buoyancy taken out and the nozzles replaced by two paddles that
// sweep Lissajous paths through the tank, each dragging the water with it
// and trailing its own ink. Each fixed step:
//
//   1. Stir     paddle k sits at
//                 p_k(phase) = centre + (STIR_REACH_X sin(a_k phase + α_k),
//                                        STIR_REACH_Y sin(b_k phase + β_k))
//               with (a, b) = (3, 2) and (2, 3), and moves at
//                 dp_k/dt = dp_k/dphase · 2π / STIR_PERIOD.
//               It pulls u, v toward that velocity and adds its ink, both
//               weighted by exp(−r² / EMIT_RADIUS²).
//   2. Confine  vorticity confinement, as in smoke.cpp:
//                 f = CONFINEMENT (N_y ω, −N_x ω),  N = ∇|ω| / |∇|ω||
//   3. Project  PROJECT_ITERATIONS of Gauss-Seidel on ∇²p = ∇·u, warm
//               started from the previous step, then u −= ∇p.
//   4. Advect   u, v backward along themselves and project again; both inks
//               are advected along that velocity, then ×= DENSITY_KEEP.
//
// The tank is closed. Each ink's opacity is o = 1 − exp(−ρ), and √o is
// quantized to INK_LEVELS levels with a 4×4 Bayer threshold, so the pair
// fits the palette as index = level1 · INK_LEVELS + level2. The palette
// squares the level back, o = (level / (INK_LEVELS − 1))², which spends the
// levels where they show: the first step above clear water is 1/225 of full
// ink, so thin ink dithers the dark water only faintly. Ink 2 uses the
// inverted threshold, 1 − t, so where both inks are dithered one steps up
// where the other steps down and the brightness holds steadier. The entry is the water plus both inks added, so where they
// cross the orange and the azure sum toward white.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define GRID_WIDTH (RETRO_WIDTH / 2) // interior cells across
#define GRID_HEIGHT (RETRO_HEIGHT / 2) // interior cells down
#define GRID_STRIDE (GRID_WIDTH + 2) // one wall cell each side
#define GRID_SIZE (GRID_STRIDE * (GRID_HEIGHT + 2))

#define EMIT_RADIUS 5.0f // cells, the 1/e radius of a paddle
#define EMIT_DENSITY 0.3f // ink added at a paddle's centre each step
#define STIR_REACH_X (GRID_WIDTH * 0.36f) // cells either side of the centre
#define STIR_REACH_Y (GRID_HEIGHT * 0.32f) // cells above and below the centre
#define STIR_PERIOD 24.0 // seconds for both paddles to close their paths
#define CONFINEMENT 20.0f // strength of the vorticity confinement
#define PROJECT_ITERATIONS 20 // Gauss-Seidel sweeps of the pressure solve
#define DENSITY_KEEP 0.996f // kept per step, so how long a trail lasts
#define INK_LEVELS 16 // levels per ink, INK_LEVELS² palette entries

static const RETRO_Palette Water = { 2, 8, 24 };
static const RETRO_Palette Ink1 = RETRO_ORANGE;
static const RETRO_Palette Ink2 = RETRO_AZURE;

static const int Bayer4x4[4][4] = {
	{  0,  8,  2, 10 },
	{ 12,  4, 14,  6 },
	{  3, 11,  1,  9 },
	{ 15,  7, 13,  5 }
};

enum { BOUNDARY_SCALAR, BOUNDARY_U, BOUNDARY_V };

float U[GRID_SIZE];
float V[GRID_SIZE];
float UPrevious[GRID_SIZE];
float VPrevious[GRID_SIZE];
float Density1[GRID_SIZE];
float Density2[GRID_SIZE];
float DensityPrevious[GRID_SIZE];
float Pressure[GRID_SIZE];
float Divergence[GRID_SIZE];
float Curl[GRID_SIZE];

static inline int Cell(int x, int y)
{
	return y * GRID_STRIDE + x;
}

//
// Fill the wall cells from the interior next to them
//
// The component normal to a wall is mirrored with its sign flipped, so the
// wall face sees zero flow through it. Everything else is copied, so its
// gradient across the wall is zero. A corner is the mean of its two
// neighbours.
//
static void SetBoundary(int boundary, float *field)
{
	for (int x = 1; x <= GRID_WIDTH; x++) {
		float sign = boundary == BOUNDARY_V ? -1.0f : 1.0f;
		field[Cell(x, 0)] = sign * field[Cell(x, 1)];
		field[Cell(x, GRID_HEIGHT + 1)] = sign * field[Cell(x, GRID_HEIGHT)];
	}
	for (int y = 1; y <= GRID_HEIGHT; y++) {
		float sign = boundary == BOUNDARY_U ? -1.0f : 1.0f;
		field[Cell(0, y)] = sign * field[Cell(1, y)];
		field[Cell(GRID_WIDTH + 1, y)] = sign * field[Cell(GRID_WIDTH, y)];
	}
	field[Cell(0, 0)] = 0.5f * (field[Cell(1, 0)] + field[Cell(0, 1)]);
	field[Cell(GRID_WIDTH + 1, 0)] = 0.5f * (field[Cell(GRID_WIDTH, 0)] + field[Cell(GRID_WIDTH + 1, 1)]);
	field[Cell(0, GRID_HEIGHT + 1)] = 0.5f * (field[Cell(1, GRID_HEIGHT + 1)] + field[Cell(0, GRID_HEIGHT)]);
	field[Cell(GRID_WIDTH + 1, GRID_HEIGHT + 1)] = 0.5f * (field[Cell(GRID_WIDTH, GRID_HEIGHT + 1)] + field[Cell(GRID_WIDTH + 1, GRID_HEIGHT)]);
}

//
// Bilinear sample at a fractional cell position
//
static float Sample(const float *field, float x, float y)
{
	x = MIN(MAX(x, 0.5f), GRID_WIDTH + 0.5f);
	y = MIN(MAX(y, 0.5f), GRID_HEIGHT + 0.5f);
	int x0 = (int)x;
	int y0 = (int)y;
	float fx = x - x0;
	float fy = y - y0;
	float top = field[Cell(x0, y0)] + (field[Cell(x0 + 1, y0)] - field[Cell(x0, y0)]) * fx;
	float bottom = field[Cell(x0, y0 + 1)] + (field[Cell(x0 + 1, y0 + 1)] - field[Cell(x0, y0 + 1)]) * fx;
	return top + (bottom - top) * fy;
}

//
// q'(x) = q(x − u(x) dt), read from source and written to field
//
static void Advect(int boundary, float *field, const float *source, float dt)
{
	for (int y = 1; y <= GRID_HEIGHT; y++) {
		for (int x = 1; x <= GRID_WIDTH; x++) {
			int i = Cell(x, y);
			field[i] = Sample(source, x - U[i] * dt, y - V[i] * dt);
		}
	}
	SetBoundary(boundary, field);
}

//
// Remove the divergent part of (u, v)
//
// With unit cells the central-difference divergence is
//   d = ½ (u(x+1) − u(x−1) + v(y+1) − v(y−1))
// and ∇²p = d on the five-point stencil is swept in place:
//   p(x, y) = (p(x−1) + p(x+1) + p(y−1) + p(y+1) − d) / 4
// then u −= ½ (p(x+1) − p(x−1)), v −= ½ (p(y+1) − p(y−1)).
//
static void Project(void)
{
	for (int y = 1; y <= GRID_HEIGHT; y++) {
		for (int x = 1; x <= GRID_WIDTH; x++) {
			int i = Cell(x, y);
			Divergence[i] = 0.5f * (U[i + 1] - U[i - 1] + V[i + GRID_STRIDE] - V[i - GRID_STRIDE]);
		}
	}
	SetBoundary(BOUNDARY_SCALAR, Divergence);

	for (int k = 0; k < PROJECT_ITERATIONS; k++) {
		for (int y = 1; y <= GRID_HEIGHT; y++) {
			for (int x = 1; x <= GRID_WIDTH; x++) {
				int i = Cell(x, y);
				Pressure[i] = (Pressure[i - 1] + Pressure[i + 1] + Pressure[i - GRID_STRIDE] + Pressure[i + GRID_STRIDE] - Divergence[i]) * 0.25f;
			}
		}
		SetBoundary(BOUNDARY_SCALAR, Pressure);
	}

	for (int y = 1; y <= GRID_HEIGHT; y++) {
		for (int x = 1; x <= GRID_WIDTH; x++) {
			int i = Cell(x, y);
			U[i] -= 0.5f * (Pressure[i + 1] - Pressure[i - 1]);
			V[i] -= 0.5f * (Pressure[i + GRID_STRIDE] - Pressure[i - GRID_STRIDE]);
		}
	}
	SetBoundary(BOUNDARY_U, U);
	SetBoundary(BOUNDARY_V, V);
}

//
// Vorticity confinement: push along N × ω, toward the centre of each swirl
//
static void Confine(float dt)
{
	for (int y = 1; y <= GRID_HEIGHT; y++) {
		for (int x = 1; x <= GRID_WIDTH; x++) {
			int i = Cell(x, y);
			Curl[i] = 0.5f * (V[i + 1] - V[i - 1] - U[i + GRID_STRIDE] + U[i - GRID_STRIDE]);
		}
	}

	for (int y = 2; y < GRID_HEIGHT; y++) {
		for (int x = 2; x < GRID_WIDTH; x++) {
			int i = Cell(x, y);
			float nx = 0.5f * (fabsf(Curl[i + 1]) - fabsf(Curl[i - 1]));
			float ny = 0.5f * (fabsf(Curl[i + GRID_STRIDE]) - fabsf(Curl[i - GRID_STRIDE]));
			float length = sqrtf(nx * nx + ny * ny) + 1e-5f;
			U[i] += CONFINEMENT * dt * (ny / length) * Curl[i];
			V[i] -= CONFINEMENT * dt * (nx / length) * Curl[i];
		}
	}
}

//
// Pull the velocity toward the paddle's and add its ink, weighted by exp(−r² / R²)
//
static void Emit(float cx, float cy, float vx, float vy, float *density)
{
	int reach = (int)(EMIT_RADIUS * 2);
	for (int y = MAX((int)cy - reach, 1); y <= MIN((int)cy + reach, GRID_HEIGHT); y++) {
		for (int x = MAX((int)cx - reach, 1); x <= MIN((int)cx + reach, GRID_WIDTH); x++) {
			int i = Cell(x, y);
			float r2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
			float weight = expf(-r2 / (EMIT_RADIUS * EMIT_RADIUS));
			U[i] += (vx - U[i]) * weight;
			V[i] += (vy - V[i]) * weight;
			density[i] += EMIT_DENSITY * weight;
		}
	}
}

//
// A paddle at (sin(a phase + α), sin(b phase + β)) about the centre, and its velocity
//
static void Stir(double phase, int a, double alpha, int b, double beta, float *density)
{
	float rate = (float)(2 * M_PI / STIR_PERIOD);
	float cx = GRID_WIDTH / 2.0f + STIR_REACH_X * (float)sin(a * phase + alpha);
	float cy = GRID_HEIGHT / 2.0f + STIR_REACH_Y * (float)sin(b * phase + beta);
	float vx = STIR_REACH_X * a * (float)cos(a * phase + alpha) * rate;
	float vy = STIR_REACH_Y * b * (float)cos(b * phase + beta) * rate;
	Emit(cx, cy, vx, vy, density);
}

void DEMO_FixedUpdate(double timestep)
{
	float dt = (float)timestep;

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + timestep * 2 * M_PI / STIR_PERIOD, 2 * M_PI);

	// Stir ink in
	Stir(phase, 3, 0, 2, M_PI / 2, Density1);
	Stir(phase, 2, M_PI, 3, 0, Density2);

	// Confine vorticity and project
	Confine(dt);
	SetBoundary(BOUNDARY_U, U);
	SetBoundary(BOUNDARY_V, V);
	Project();

	// Advect velocity along itself, then project again
	memcpy(UPrevious, U, sizeof(U));
	memcpy(VPrevious, V, sizeof(V));
	for (int y = 1; y <= GRID_HEIGHT; y++) {
		for (int x = 1; x <= GRID_WIDTH; x++) {
			int i = Cell(x, y);
			float sx = x - UPrevious[i] * dt;
			float sy = y - VPrevious[i] * dt;
			U[i] = Sample(UPrevious, sx, sy);
			V[i] = Sample(VPrevious, sx, sy);
		}
	}
	SetBoundary(BOUNDARY_U, U);
	SetBoundary(BOUNDARY_V, V);
	Project();

	// Advect ink and thin it
	memcpy(DensityPrevious, Density1, sizeof(Density1));
	Advect(BOUNDARY_SCALAR, Density1, DensityPrevious, dt);
	memcpy(DensityPrevious, Density2, sizeof(Density2));
	Advect(BOUNDARY_SCALAR, Density2, DensityPrevious, dt);
	for (int i = 0; i < GRID_SIZE; i++) {
		Density1[i] *= DENSITY_KEEP;
		Density2[i] *= DENSITY_KEEP;
	}
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Draw ink. Screen pixel x covers grid cell x / 2 + 1, whose centre is
	// at screen x = 2 (cell − 1) + 1, so the pixel centre sits at cell
	// (x + 0.5) / 2 + 0.5. The Bayer entry is the quantizing threshold.
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float gy = (y + 0.5f) * GRID_HEIGHT / RETRO_HEIGHT + 0.5f;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float gx = (x + 0.5f) * GRID_WIDTH / RETRO_WIDTH + 0.5f;
			float threshold = (Bayer4x4[y & 3][x & 3] + 0.5f) / 16.0f;
			float ink1 = sqrtf(1.0f - expf(-Sample(Density1, gx, gy)));
			float ink2 = sqrtf(1.0f - expf(-Sample(Density2, gx, gy)));
			int level1 = CLAMP(ink1 * (INK_LEVELS - 1) + threshold, 0, INK_LEVELS);
			int level2 = CLAMP(ink2 * (INK_LEVELS - 1) + 1.0f - threshold, 0, INK_LEVELS);
			*buffer++ = level1 * INK_LEVELS + level2;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Entry level1 · INK_LEVELS + level2 is the water with both
	// inks added at opacity (level / (INK_LEVELS − 1))², clipped at white.
	for (int level1 = 0; level1 < INK_LEVELS; level1++) {
		for (int level2 = 0; level2 < INK_LEVELS; level2++) {
			float a = (float)level1 / (INK_LEVELS - 1);
			float b = (float)level2 / (INK_LEVELS - 1);
			a *= a;
			b *= b;
			RETRO_SetColor(level1 * INK_LEVELS + level2,
				CLAMP256(Water.r + a * Ink1.r + b * Ink2.r),
				CLAMP256(Water.g + a * Ink1.g + b * Ink2.g),
				CLAMP256(Water.b + a * Ink1.b + b * Ink2.b));
		}
	}
}
