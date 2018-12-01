//
// blobs.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define NUM_BLOBS 160
#define BLOB_RADIUS 20
#define BLOB_DRADIUS (BLOB_RADIUS * 2)
#define BLOB_SRADIUS (BLOB_RADIUS * BLOB_RADIUS)

unsigned char Blob[BLOB_DRADIUS * BLOB_DRADIUS];
Point2D Blobs[NUM_BLOBS];

void DEMO_Render(double deltatime)
{
	// Calculate movement
	for (int i = 0; i < NUM_BLOBS; i++) {
		Blobs[i].x += -2 + (5.0 * (rand() / (RAND_MAX + 2.0)));
		Blobs[i].y += -2 + (5.0 * (rand() / (RAND_MAX + 2.0)));
	}

	// Draw blobs
	for (int i = 0; i < NUM_BLOBS; i++) {
		if (Blobs[i].x > 0 && Blobs[i].x < RETRO_WIDTH - BLOB_DRADIUS && Blobs[i].y > 0 && Blobs[i].y < RETRO_HEIGHT - BLOB_DRADIUS) {
			for (int y = 0; y < BLOB_DRADIUS; y++) {
				for (int x = 0; x < BLOB_DRADIUS; x++) {
					unsigned char color = CLAMP256(RETRO_GetPixel(Blobs[i].x + x, Blobs[i].y + y) + Blob[y * BLOB_DRADIUS + x]);
					RETRO_PutPixel(Blobs[i].x + x, Blobs[i].y + y, color);
				}
			}
		} else {
			Blobs[i].x = (RETRO_WIDTH / 2) - BLOB_RADIUS;
			Blobs[i].y = (RETRO_HEIGHT / 2) - BLOB_RADIUS;
		}
	}
}

void DEMO_Initialize()
{
	// Init palette
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i / 2.5, i / 1.5, i);
	}
	RETRO_SetPalette();

	// Init blob positions
	for (int i = 0; i < NUM_BLOBS; i++) {
		Blobs[i].x = (RETRO_WIDTH / 2) - BLOB_RADIUS;
		Blobs[i].y = (RETRO_HEIGHT / 2) - BLOB_RADIUS;
	}

	// Init blob shape
	for (int y = 0; y < BLOB_DRADIUS; y++) {
		for (int x = 0; x < BLOB_DRADIUS; x++) {
			float distance = (y - BLOB_RADIUS) * (y - BLOB_RADIUS) + (x - BLOB_RADIUS) * (x - BLOB_RADIUS);

			if (distance <= BLOB_SRADIUS) {
				float fraction = distance / BLOB_SRADIUS;
				Blob[y * BLOB_DRADIUS + x] = (unsigned char) (pow((0.7 - (fraction * fraction)), 3.3) * 255.0);
			} else {
				Blob[y * BLOB_DRADIUS + x] = 0;
			}
		}
	}
}
