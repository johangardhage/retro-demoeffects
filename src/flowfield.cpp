//
// Perlin flow field
//
// Particles carried by a field of directions, drawn by the trails they leave.
// The field is Perlin noise read as an angle,
//
//   angle = 2π FIELD_TURNS noise(FIELD_SCALE x, FIELD_SCALE y, phase)
//
// and a particle takes one step of PARTICLE_SPEED along the angle it stands on:
//
//   x' = x + PARTICLE_SPEED cos(angle)
//   y' = y + PARTICLE_SPEED sin(angle)
//
// The noise is smooth, so neighbors head nearly the same way and the screen
// fills with currents. The field is not free of divergence: where directions
// close in on each other the particles are gathered into one line and where
// they open up the screen is swept empty, which is what gives the picture its
// strands. A particle that leaves the screen, or has lived PARTICLE_LIFE steps,
// is put back at random, so the empty places keep being sown and no strand
// keeps its particles for good.
//
// The noise is gradient noise. Each corner of the unit lattice holds a
// direction, picked from the twelve edges of a cube by a hash of the corner,
// and gives the point the dot product of that direction with the offset to the
// point. The eight corners of the cell the point is in are blended by the
// quintic
//
//   fade(t) = 6t⁵ − 15t⁴ + 10t³
//
// whose first and second derivatives are zero at both ends, so the field has no
// crease where two cells meet. The hash is a shuffled table of 256 entries, so
// the noise repeats every 256 cells along each axis. The screen is a few cells
// across. The third axis is time, and phase wraps on the whole table, where
// the field is the one it started as.
//
// A particle adds to a count where it stands, shared between the four pixels
// around the point by how near it is to each, and every step the count keeps
// TRAIL_KEEP of itself. A pixel that takes d a step settles at d / (1 −
// TRAIL_KEEP), so a strand is as bright as it is crowded, and the color is the
// count through 1 − exp(−count / TRAIL_EXPOSURE), which lets a strand of a few
// particles show and one of hundreds stay short of white.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define NUM_PARTICLES 4000
#define PARTICLE_SPEED 0.7f // pixels a step
#define PARTICLE_LIFE 400 // steps a particle is carried for, at the most
#define FIELD_SCALE (3.0f / RETRO_WIDTH) // noise cells a pixel
#define FIELD_TURNS 2.0f // turns the angle makes as the noise goes from 0 to 1
#define FIELD_DRIFT 0.08 // noise cells a second the field moves through time
#define NOISE_PERIOD 256 // cells the noise repeats after
#define TRAIL_KEEP 0.97f // of its count a pixel keeps a step
#define TRAIL_EXPOSURE 6.0f // the count that reaches 1 − 1/e of the ramp

struct Particle {
	float x, y;
	int life;
};

static Particle Particles[NUM_PARTICLES];
static float Trail[RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char Permutation[NOISE_PERIOD * 2];

//
// The gradient at a lattice corner, dotted with the offset from it
//
// The low four bits of the hash pick one of the twelve edge directions of a
// cube, (±1, ±1, 0), (±1, 0, ±1) and (0, ±1, ±1), four of them twice.
//
static float Gradient(int hash, float x, float y, float z)
{
	int h = hash & 15;
	float u = h < 8 ? x : y;
	float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);

	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

//
// Perlin noise, in [-1, 1]
//
static float Noise(float x, float y, float z)
{
	// The cell, and the point inside it
	int cellx = (int)floorf(x) & (NOISE_PERIOD - 1);
	int celly = (int)floorf(y) & (NOISE_PERIOD - 1);
	int cellz = (int)floorf(z) & (NOISE_PERIOD - 1);
	x = fract(x);
	y = fract(y);
	z = fract(z);

	float u = smootherstep(0.0f, 1.0f, x);
	float v = smootherstep(0.0f, 1.0f, y);
	float w = smootherstep(0.0f, 1.0f, z);

	// Hash the eight corners
	int a = Permutation[cellx] + celly;
	int aa = Permutation[a] + cellz;
	int ab = Permutation[a + 1] + cellz;
	int b = Permutation[cellx + 1] + celly;
	int ba = Permutation[b] + cellz;
	int bb = Permutation[b + 1] + cellz;

	// Blend along x, then y, then z
	float near = mix(mix(Gradient(Permutation[aa], x, y, z), Gradient(Permutation[ba], x - 1, y, z), u),
					 mix(Gradient(Permutation[ab], x, y - 1, z), Gradient(Permutation[bb], x - 1, y - 1, z), u), v);
	float far = mix(mix(Gradient(Permutation[aa + 1], x, y, z - 1), Gradient(Permutation[ba + 1], x - 1, y, z - 1), u),
					mix(Gradient(Permutation[ab + 1], x, y - 1, z - 1), Gradient(Permutation[bb + 1], x - 1, y - 1, z - 1), u), v);

	return mix(near, far, w);
}

static void SowParticle(Particle *particle)
{
	particle->x = RANDOMF(RETRO_WIDTH);
	particle->y = RANDOMF(RETRO_HEIGHT);
	particle->life = 1 + RANDOM(PARTICLE_LIFE);
}

void DEMO_FixedUpdate(double timestep)
{
	// Calculate phase. The noise repeats on its period, so the wrap is seamless
	static double phase = 0;
	phase = fmod(phase + timestep * FIELD_DRIFT, NOISE_PERIOD);

	// Fade the trails
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		Trail[i] *= TRAIL_KEEP;
	}

	for (Particle &particle : Particles) {
		// Carry the particle one step along the field
		float angle = 2 * M_PI * FIELD_TURNS * Noise(particle.x * FIELD_SCALE, particle.y * FIELD_SCALE, phase);
		particle.x += PARTICLE_SPEED * cosf(angle);
		particle.y += PARTICLE_SPEED * sinf(angle);
		particle.life--;

		// The four pixels around the point all have to be on the screen
		if (particle.life <= 0 || particle.x < 0.5f || particle.x >= RETRO_WIDTH - 0.5f ||
			particle.y < 0.5f || particle.y >= RETRO_HEIGHT - 0.5f) {
			SowParticle(&particle);
			continue;
		}

		// Share the particle between them. A pixel's center is half a pixel in
		float px = particle.x - 0.5f;
		float py = particle.y - 0.5f;
		int ix = px;
		int iy = py;
		float fx = px - ix;
		float fy = py - iy;
		int offset = iy * RETRO_WIDTH + ix;

		Trail[offset] += (1 - fx) * (1 - fy);
		Trail[offset + 1] += fx * (1 - fy);
		Trail[offset + RETRO_WIDTH] += (1 - fx) * fy;
		Trail[offset + RETRO_WIDTH + 1] += fx * fy;
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Draw trails
	unsigned char *buffer = RETRO_FrameBuffer();

	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		buffer[i] = (RETRO_COLORS - 1) * (1 - expf(-Trail[i] / TRAIL_EXPOSURE));
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 64, RETRO_BLACK, RETRO_INDIGO);
	RETRO_CreateGradientPalette(64, 128, RETRO_INDIGO, RETRO_DEEPPINK);
	RETRO_CreateGradientPalette(128, 208, RETRO_DEEPPINK, RETRO_MARIGOLD);
	RETRO_CreateGradientPalette(208, RETRO_COLORS, RETRO_MARIGOLD, RETRO_WHITE);

	// Shuffle the hash table, and lay it out twice so a corner one cell on
	// needs no wrap
	for (int i = 0; i < NOISE_PERIOD; i++) {
		Permutation[i] = i;
	}
	for (int i = NOISE_PERIOD - 1; i > 0; i--) {
		int j = RANDOM(i + 1);
		SWAP(Permutation[i], Permutation[j]);
	}
	for (int i = 0; i < NOISE_PERIOD; i++) {
		Permutation[NOISE_PERIOD + i] = Permutation[i];
	}

	for (Particle &particle : Particles) {
		SowParticle(&particle);
	}
}
