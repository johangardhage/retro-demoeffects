//
// Blobs
//
// A swarm of additive blobs on a random walk. Every blob is the same
// precalculated Wyvill kernel of the normalised squared radius s = r²/R²:
//
//   K(s) = (1 − s)² (9 − 4s) / 9     (= 1 − 22/9 s + 17/9 s² − 4/9 s³)
//   I(p) = min(255, sum_i  PEAK · K(|p − x_i|² / R²))
//
// f(0) = 1 and the double root at s = 1 makes f and f' vanish there, so
// extending by zero outside R is C¹. The cubic goes positive again for
// s > 1, which is why those pixels are stored as 0 rather than evaluated.
// PEAK = 80: three overlaps reach 240; four saturate the ramp.
//
// Each axis steps uniformly in {−k, …, k}, k = 2. Variance is k(k+1)/3 = 2
// per step, so after n steps the RMS displacement is √(2n) pixels per axis.
// The shorter half-screen is 120: √(2n) = 120 at n = 7200, about two
// minutes at 60 steps/s. One step is a square; many steps are an isotropic
// Gaussian (CLT). The edge absorbs, the centre emits, so the swarm stays
// a cloud, not a uniform haze.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define NUM_BLOBS 160
#define BLOB_RADIUS 20
#define BLOB_SIZE (BLOB_RADIUS * 2 + 1) // odd, so the peak lands on an exact center pixel
#define BLOB_PEAK 80 // peak of one kernel; three overlaps almost fill the ramp, four saturate
#define BLOB_STEP 2 // maximum walk on one axis per simulation step

unsigned char BlobShape[BLOB_SIZE * BLOB_SIZE];
Point2D BlobPositions[NUM_BLOBS];

//
// Advance the walk one fixed step
//
// Each axis takes an independent step drawn uniformly from the integers
// {-k, ..., k} with k = BLOB_STEP. A uniform draw over 2k+1 consecutive integers
// has variance k(k+1)/3, so one step has variance
//
//   BLOB_STEP (BLOB_STEP + 1) / 3 = 2
//
// square pixels per axis. Independent steps add variances, so after n steps the
// RMS displacement is sqrt(2n) pixels per axis. The spread therefore counts
// steps, not seconds, which is why the walk is advanced at a fixed rate.
//
// One step is a square (a diagonal step reaches further than an axial one). That
// does not survive accumulation: many independent equal-variance steps are an
// isotropic Gaussian by the central limit theorem.
//
void DEMO_Update(double deltatime)
{
	// Move blobs
	for (int i = 0; i < NUM_BLOBS; i++) {
		BlobPositions[i].x += RANDOM(BLOB_STEP * 2 + 1) - BLOB_STEP;
		BlobPositions[i].y += RANDOM(BLOB_STEP * 2 + 1) - BLOB_STEP;

		// The kernel covers [center - R, center + R]. Once it has left the screen
		// the edge absorbs the blob and the centre emits a new one, so the swarm
		// stays a cloud brightest in the middle rather than an even haze.
		if (BlobPositions[i].x + BLOB_RADIUS < 0 || BlobPositions[i].x - BLOB_RADIUS >= RETRO_WIDTH ||
			BlobPositions[i].y + BLOB_RADIUS < 0 || BlobPositions[i].y - BLOB_RADIUS >= RETRO_HEIGHT) {
			BlobPositions[i].x = RETRO_WIDTH / 2;
			BlobPositions[i].y = RETRO_HEIGHT / 2;
		}
	}
}

void DEMO_Render(double deltatime)
{
	// Draw blobs
	for (int i = 0; i < NUM_BLOBS; i++) {
		int left = BlobPositions[i].x - BLOB_RADIUS;
		int top = BlobPositions[i].y - BLOB_RADIUS;

		int xstart = MAX(0, -left);
		int xend = MIN(BLOB_SIZE, RETRO_WIDTH - left);
		int ystart = MAX(0, -top);
		int yend = MIN(BLOB_SIZE, RETRO_HEIGHT - top);

		// Every addend is non-negative, so clamping per pixel equals summing first
		// and clamping the total.
		for (int y = ystart; y < yend; y++) {
			for (int x = xstart; x < xend; x++) {
				unsigned char color = CLAMP256(RETRO_GetPixel(left + x, top + y) + BlobShape[y * BLOB_SIZE + x]);
				RETRO_PutPixel(left + x, top + y, color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_LIGHTBLUE);

	// Init positions. RMS is √(2n) per axis. The shorter half-screen
	// (height 120) is reached at n = 7200, about two minutes at 60 Hz.
	for (int i = 0; i < NUM_BLOBS; i++) {
		BlobPositions[i].x = RETRO_WIDTH / 2;
		BlobPositions[i].y = RETRO_HEIGHT / 2;
	}

	// Init kernel. Wyvill's soft-object kernel of the normalized squared distance
	// s = r^2 / R^2, a cubic that approximates a Gaussian:
	//
	//   f(s) = 1 - (22/9)s + (17/9)s^2 - (4/9)s^3
	//        = (1 - s)^2 (9 - 4s) / 9
	//
	// f(0) = 1, and the double root at s = 1 makes both f and f' vanish there, so
	// extending by zero outside the radius is C^1. Beyond the radius the cubic
	// turns positive again, so those pixels are cleared rather than evaluated.
	int radiussquared = BLOB_RADIUS * BLOB_RADIUS;

	for (int y = 0; y < BLOB_SIZE; y++) {
		for (int x = 0; x < BLOB_SIZE; x++) {
			int dx = x - BLOB_RADIUS;
			int dy = y - BLOB_RADIUS;
			int distancesquared = dx * dx + dy * dy;

			if (distancesquared <= radiussquared) {
				float s = (float)distancesquared / radiussquared;
				float density = pow(1 - s, 2) * (9 - 4 * s) / 9;
				BlobShape[y * BLOB_SIZE + x] = density * BLOB_PEAK + 0.5;
			} else {
				BlobShape[y * BLOB_SIZE + x] = 0;
			}
		}
	}
}
