//
// Fractal clouds
//
// A field of value noise built by diamond-square, scrolled under a palette that
// cycles. Nothing is drawn: the picture is one 256x256 field, sampled once per
// pixel, and everything that moves is an index. Bilinear subpixel sampling ensures
// butter-smooth continuous scrolling without integer pixel jitter.
//
// Diamond-square fills the field one level at a time, halving the step and the
// displacement together. At step s the diamond pass takes each square's centre
// from its four corners, and the square pass takes each diamond's centre from
// its four neighbours, both plus a uniform jitter of the current amplitude:
//
//   h(centre) = mean(neighbours) + U(-A, A),   A' = ROUGHNESS * A
//
// Amplitude halves as frequency doubles, so a detail s cells wide stands A(s)
// high with A proportional to s, and the field is fractional Brownian motion
// with a 1/f amplitude spectrum. That is the exponent that reads as cloud: a
// smaller ROUGHNESS gives smooth blobs, a larger one gives noise.
//
// Every index wraps, so the field is a torus and it can be scrolled forever.
// The first level is one square whose four corners are all the same cell, which
// is why the field is zeroed first and starts from one seed rather than four.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define MAP_SIZE 256 // cells on a side, a power of two, and the field wraps
#define MAP_MASK (MAP_SIZE - 1)
#define ROUGHNESS 0.5 // what the displacement is multiplied by at each level
#define SCROLL_X 11.0 // cells a second the field drifts across
#define SCROLL_Y 7.0 // cells a second it drifts down
#define CYCLE_SPEED 24.0 // palette entries a second the colors travel

unsigned char Field[MAP_SIZE * MAP_SIZE];

//
// The field's value at a wrapped cell
//
inline float Cell(float *field, int x, int y)
{
	return field[(y & MAP_MASK) * MAP_SIZE + (x & MAP_MASK)];
}

//
// Diamond-square over a torus, normalized into the byte range
//
void BuildField(void)
{
	static float field[MAP_SIZE * MAP_SIZE];
	float amplitude = MAP_SIZE / 2;

	for (int step = MAP_SIZE; step > 1; step /= 2) {
		int half = step / 2;

		// Diamond: each square's centre, from the four corners
		for (int y = 0; y < MAP_SIZE; y += step) {
			for (int x = 0; x < MAP_SIZE; x += step) {
				float mean = (Cell(field, x, y) + Cell(field, x + step, y) +
							  Cell(field, x, y + step) + Cell(field, x + step, y + step)) / 4;
				field[(y + half) * MAP_SIZE + x + half] = mean + RANDOMF(2 * amplitude) - amplitude;
			}
		}

		// Square: each diamond's centre, from the four points around it. Rows
		// alternate which column they start on, which is the half-step offset
		// that makes the two passes interleave
		for (int y = 0; y < MAP_SIZE; y += half) {
			for (int x = (y / half) % 2 == 0 ? half : 0; x < MAP_SIZE; x += step) {
				float mean = (Cell(field, x - half, y) + Cell(field, x + half, y) +
							  Cell(field, x, y - half) + Cell(field, x, y + half)) / 4;
				field[y * MAP_SIZE + x] = mean + RANDOMF(2 * amplitude) - amplitude;
			}
		}

		amplitude *= ROUGHNESS;
	}

	// A displacement sum has no fixed range, so the field is stretched into the
	// byte range it is read in rather than assumed to arrive there
	float low = field[0], high = field[0];
	for (int i = 0; i < MAP_SIZE * MAP_SIZE; i++) {
		low = MIN(low, field[i]);
		high = MAX(high, field[i]);
	}
	for (int i = 0; i < MAP_SIZE * MAP_SIZE; i++) {
		Field[i] = CLAMP256((field[i] - low) * 255 / (high - low));
	}
}

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double scrollx = 0, scrolly = 0, cycle = 0;
	scrollx = fmod(scrollx + deltatime * SCROLL_X, MAP_SIZE);
	scrolly = fmod(scrolly + deltatime * SCROLL_Y, MAP_SIZE);
	cycle = fmod(cycle + deltatime * CYCLE_SPEED, RETRO_COLORS);

	int ix = (int)scrollx;
	int iy = (int)scrolly;
	float fx = (float)(scrollx - ix);
	float fy = (float)(scrolly - iy);
	float shift = (float)cycle;

	unsigned char *buffer = RETRO_FrameBuffer();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		int y0 = (y + iy) & MAP_MASK;
		int y1 = (y + iy + 1) & MAP_MASK;
		int row0 = y0 * MAP_SIZE;
		int row1 = y1 * MAP_SIZE;

		for (int x = 0; x < RETRO_WIDTH; x++) {
			int x0 = (x + ix) & MAP_MASK;
			int x1 = (x + ix + 1) & MAP_MASK;

			float v00 = Field[row0 + x0];
			float v10 = Field[row0 + x1];
			float v01 = Field[row1 + x0];
			float v11 = Field[row1 + x1];

			float val = (1.0f - fx) * (1.0f - fy) * v00 +
			            fx * (1.0f - fy) * v10 +
			            (1.0f - fx) * fy * v01 +
			            fx * fy * v11;

			int color_idx = ((int)(val + shift)) & 0xff;
			buffer[y * RETRO_WIDTH + x] = color_idx;
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 64, RETRO_DARKMIDNIGHTBLUE, RETRO_STEELBLUE);
	RETRO_CreateGradientPalette(64, 128, RETRO_STEELBLUE, RETRO_WHITE);
	RETRO_CreateGradientPalette(128, 192, RETRO_WHITE, RETRO_STEELBLUE);
	RETRO_CreateGradientPalette(192, RETRO_COLORS, RETRO_STEELBLUE, RETRO_DARKMIDNIGHTBLUE);

	BuildField();
}
