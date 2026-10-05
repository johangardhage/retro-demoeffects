//
// Smoke
//
// Stable fluids on a half-resolution collocated grid: a velocity field
// (u, v) and a smoke density ρ, both advected semi-Lagrangian and kept
// incompressible by a pressure projection. Inviscid; the only diffusion
// is the bilinear resampling in the advection. Each fixed step:
//
//   1. Emit     two nozzles near the floor pull u, v toward their jet and
//               add ρ, both weighted by exp(−r² / EMIT_RADIUS²). The jets
//               swing about the vertical at different rates.
//   2. Buoyancy v −= BUOYANCY ρ dt (y grows down, so this is up).
//   3. Confine  vorticity confinement puts back the small swirls that the
//               advection smears out:
//                 ω = ∂v/∂x − ∂u/∂y
//                 N = ∇|ω| / |∇|ω||
//                 f = CONFINEMENT (N × ω ẑ) = CONFINEMENT (N_y ω, −N_x ω)
//   4. Project  solve ∇²p = ∇·u with PROJECT_ITERATIONS of Gauss-Seidel,
//               then u −= ∇p. The pressure is kept from the step before as
//               the starting guess, so few sweeps are needed.
//   5. Advect   u, v backward along themselves, sampled bilinearly,
//                 q'(x) = q(x − u(x) dt)
//               and project again, since advection is not divergence-free.
//               ρ is advected the same way along that velocity, then
//               ρ ×= DENSITY_KEEP.
//
// The box is closed: the normal velocity is mirrored with its sign flipped
// at the walls, and ρ and p are mirrored as they are. Velocities are in
// grid cells per second. The screen is the density resampled bilinearly
// to full resolution through 1 − exp(−ρ), so thick smoke saturates to the
// top of the ramp instead of clipping.
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

#define EMIT_RADIUS 4.0f // cells, the 1/e radius of a nozzle
#define EMIT_SPEED 70.0f // cells per second along the jet
#define EMIT_DENSITY 0.35f // added at a nozzle's center each step
#define EMIT_SWING 0.6f // radians the jet swings either side of vertical
#define EMIT_PERIOD 9.0 // seconds for the slower nozzle's swing
#define BUOYANCY 30.0f // upward acceleration per unit of density
#define CONFINEMENT 12.0f // strength of the vorticity confinement
#define PROJECT_ITERATIONS 20 // Gauss-Seidel sweeps of the pressure solve
#define DENSITY_KEEP 0.992f // kept per step, so how fast smoke thins out

enum { BOUNDARY_SCALAR, BOUNDARY_U, BOUNDARY_V };

static float U[GRID_SIZE];
static float V[GRID_SIZE];
static float UPrevious[GRID_SIZE];
static float VPrevious[GRID_SIZE];
static float Density[GRID_SIZE];
static float DensityPrevious[GRID_SIZE];
static float Pressure[GRID_SIZE];
static float Divergence[GRID_SIZE];
static float Curl[GRID_SIZE];

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
// neighbors.
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
	x = clamp(x, 0.5f, GRID_WIDTH + 0.5f);
	y = clamp(y, 0.5f, GRID_HEIGHT + 0.5f);
	int x0 = (int)x;
	int y0 = (int)y;
	float fx = x - x0;
	float fy = y - y0;
	float top = mix(field[Cell(x0, y0)], field[Cell(x0 + 1, y0)], fx);
	float bottom = mix(field[Cell(x0, y0 + 1)], field[Cell(x0 + 1, y0 + 1)], fx);
	return mix(top, bottom, fy);
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
// Vorticity confinement: push along N × ω, toward the center of each swirl
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
			float length = hypotf(nx, ny) + 1e-5f;
			U[i] += CONFINEMENT * dt * (ny / length) * Curl[i];
			V[i] -= CONFINEMENT * dt * (nx / length) * Curl[i];
		}
	}
}

//
// Pull the velocity toward a jet and add smoke, weighted by exp(−r² / R²)
//
static void Emit(float cx, float cy, float angle)
{
	float vx = EMIT_SPEED * sinf(angle);
	float vy = -EMIT_SPEED * cosf(angle);
	int reach = (int)(EMIT_RADIUS * 2);
	for (int y = MAX((int)cy - reach, 1); y <= MIN((int)cy + reach, GRID_HEIGHT); y++) {
		for (int x = MAX((int)cx - reach, 1); x <= MIN((int)cx + reach, GRID_WIDTH); x++) {
			int i = Cell(x, y);
			float r2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
			float weight = expf(-r2 / (EMIT_RADIUS * EMIT_RADIUS));
			U[i] = mix(U[i], vx, weight);
			V[i] = mix(V[i], vy, weight);
			Density[i] += EMIT_DENSITY * weight;
		}
	}
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	float dt = (float)time.delta;

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + time.delta * 2 * M_PI / EMIT_PERIOD, 2 * M_PI);

	// Emit smoke
	Emit(GRID_WIDTH / 3.0f, GRID_HEIGHT - 8.0f, EMIT_SWING * (float)sin(phase));
	Emit(GRID_WIDTH * 2 / 3.0f, GRID_HEIGHT - 8.0f, EMIT_SWING * (float)sin(2 * phase + M_PI / 3));

	// Add buoyancy
	for (int y = 1; y <= GRID_HEIGHT; y++) {
		for (int x = 1; x <= GRID_WIDTH; x++) {
			int i = Cell(x, y);
			V[i] -= BUOYANCY * Density[i] * dt;
		}
	}

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

	// Advect density and thin it
	memcpy(DensityPrevious, Density, sizeof(Density));
	Advect(BOUNDARY_SCALAR, Density, DensityPrevious, dt);
	for (float &density : Density) {
		density *= DENSITY_KEEP;
	}
}

void DEMO_Render(RETRO_Time time)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Draw smoke. Screen pixel x covers grid cell x / 2 + 1, whose center is
	// at screen x = 2 (cell − 1) + 1, so the pixel center sits at cell
	// (x + 0.5) / 2 + 0.5.
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float gy = (y + 0.5f) * GRID_HEIGHT / RETRO_HEIGHT + 0.5f;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float gx = (x + 0.5f) * GRID_WIDTH / RETRO_WIDTH + 0.5f;
			float density = Sample(Density, gx, gy);
			*buffer++ = CLAMP256((1.0f - expf(-density)) * RETRO_COLORS);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Index is opacity: thin smoke is a cold blue-gray over black,
	// thick smoke is white.
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_MUTEDDARKSLATEBLUE);
	RETRO_CreateGradientPalette(64, 160, RETRO_MUTEDDARKSLATEBLUE, RETRO_LIGHTSLATEGRAY);
	RETRO_CreateGradientPalette(160, RETRO_COLORS, RETRO_LIGHTSLATEGRAY, RETRO_GHOSTWHITE);
}
