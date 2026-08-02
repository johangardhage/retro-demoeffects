//
// Blobs
//
// A swarm of additive blobs performing a random walk. Every blob is the same
// precalculated, radially symmetric shape. Where blobs overlap their intensities
// add up, so dense clusters glow brighter and eventually saturate.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"

#define NUM_BLOBS 160
#define BLOB_RADIUS 20
#define BLOB_SIZE (BLOB_RADIUS * 2 + 1) // odd, so the peak lands on an exact center pixel
#define BLOB_PEAK 80 // peak value of a single blob, ~255/80 blobs must overlap to reach white
#define BLOB_STEP 2 // maximum random walk displacement per axis and simulation step

unsigned char BlobShape[BLOB_SIZE * BLOB_SIZE];
Point2D BlobPositions[NUM_BLOBS];

void DEMO_Render(double deltatime)
{
	// Advance the walk in fixed steps. A random walk accumulates variance per step, so its
	// spread is set by steps per second.
	while (RETRO_PerformSimulation()) {
		for (int i = 0; i < NUM_BLOBS; i++) {
			BlobPositions[i].x += RANDOM(BLOB_STEP * 2 + 1) - BLOB_STEP;
			BlobPositions[i].y += RANDOM(BLOB_STEP * 2 + 1) - BLOB_STEP;

			// Respawn blobs once they have drifted completely off screen and stopped
			// contributing anything. The blob covers [center - BLOB_RADIUS, center + BLOB_RADIUS].
			if (BlobPositions[i].x + BLOB_RADIUS < 0 || BlobPositions[i].x - BLOB_RADIUS >= RETRO_WIDTH ||
				BlobPositions[i].y + BLOB_RADIUS < 0 || BlobPositions[i].y - BLOB_RADIUS >= RETRO_HEIGHT) {
				BlobPositions[i].x = RETRO_WIDTH / 2;
				BlobPositions[i].y = RETRO_HEIGHT / 2;
			}
		}
	}

	// Draw blobs
	for (int i = 0; i < NUM_BLOBS; i++) {
		int left = BlobPositions[i].x - BLOB_RADIUS;
		int top = BlobPositions[i].y - BLOB_RADIUS;

		// Clip the blob against the screen so blobs slide off the edges instead of
		// disappearing. Clipping the loop bounds once is cheaper than testing every pixel.
		int xstart = MAX(0, -left);
		int xend = MIN(BLOB_SIZE, RETRO_WIDTH - left);
		int ystart = MAX(0, -top);
		int yend = MIN(BLOB_SIZE, RETRO_HEIGHT - top);

		// Draw the blob, saturating at white
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
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i / 2.5, i / 1.5, i);
	}

	// Init blob positions
	for (int i = 0; i < NUM_BLOBS; i++) {
		BlobPositions[i].x = RETRO_WIDTH / 2;
		BlobPositions[i].y = RETRO_HEIGHT / 2;
	}

	// Init blob shape
	//
	// The falloff is Wyvill's soft object function of the normalized squared distance
	// s = r^2 / BLOB_RADIUS^2, a cubic in s that closely approximates a Gaussian:
	//
	//   f(s) = 1 - (22/9)s + (17/9)s^2 - (4/9)s^3
	//
	// f(0) = 1 and f(1) = f'(1) = 0, so the blob fades out to nothing at the radius
	// without leaving a visible rim. Beyond the radius the polynomial curves upwards
	// again, so those pixels are cleared instead of evaluated.
	for (int y = 0; y < BLOB_SIZE; y++) {
		for (int x = 0; x < BLOB_SIZE; x++) {
			float dx = x - BLOB_RADIUS;
			float dy = y - BLOB_RADIUS;
			float distancesquared = dx * dx + dy * dy;

			if (distancesquared <= BLOB_RADIUS * BLOB_RADIUS) {
				float s = distancesquared / (BLOB_RADIUS * BLOB_RADIUS);
				float density = 1 - (22.0 / 9.0) * s + (17.0 / 9.0) * pow(s, 2) - (4.0 / 9.0) * pow(s, 3);
				BlobShape[y * BLOB_SIZE + x] = density * BLOB_PEAK + 0.5;
			} else {
				BlobShape[y * BLOB_SIZE + x] = 0;
			}
		}
	}
}
