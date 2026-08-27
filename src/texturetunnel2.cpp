//
// Tunnel, wandering mouth
//
// A classic polar-coordinate tunnel. For every screen pixel, its angle
// around the tunnel supplies the horizontal texture coordinate and inverse
// distance from the moving vanishing point supplies the coordinate down the
// tunnel. Advancing that second coordinate flies the camera forwards.
//
// Nothing is precomputed. The mouth wanders on two incommensurate sines per
// axis, so every pixel's angle and distance change from frame to frame, and
// no table would hold. The far end is an opening rather than a point, and a
// ripple on its radius around the angle keeps that opening off a circle.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define ANGLE_SCALE (TEXTURE_WIDTH / (2 * M_PI)) // texture columns per radian
#define DEPTH_SCALE 2350.0f
#define FLIGHT_SPEED 105.0f
#define ROLL_SPEED 9.0f
#define MOUTH_RADIUS 6.2f // texels, before the angular ripple below
#define MOUTH_RIPPLE 0.8f

void DEMO_Render(double deltatime)
{
	static float time = 0.0f;
	static float flight = 0.0f;
	static float roll = 0.0f;
	time += deltatime;
	flight += FLIGHT_SPEED * deltatime;
	roll += ROLL_SPEED * deltatime;

	unsigned char *image = RETRO_ImageData();

	// Two incommensurate motions keep the mouth from tracing a simple circle.
	float cx = RETRO_WIDTH * 0.5f + 23.0f * sin(time * 0.73f) + 9.0f * sin(time * 1.91f);
	float cy = RETRO_HEIGHT * 0.5f + 17.0f * cos(time * 0.61f) + 7.0f * sin(time * 1.37f);

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			float dx = x - cx;
			float dy = y - cy;
			float radius2 = dx * dx + dy * dy;
			float angle = atan2(dy, dx);

			// The far end is deliberately an irregular black opening.
			float mouth = MOUTH_RADIUS + MOUTH_RIPPLE * sin(angle * 5.0f + time * 1.7f);
			if (radius2 < mouth * mouth) {
				RETRO_PutPixel(x, y, 0);
				continue;
			}

			// Half the screen has a negative angle, so the column has to
			// floor: a cast toward zero would fold the cells either side of
			// 0 onto texel 0. The row down the tunnel is always positive.
			int u = (int)floor(angle * ANGLE_SCALE + roll) & (TEXTURE_WIDTH - 1);
			int v = (int)(DEPTH_SCALE / sqrt(radius2) + flight) & (TEXTURE_HEIGHT - 1);

			RETRO_PutPixel(x, y, image[v * TEXTURE_WIDTH + u]);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_256x256.pcx", true);

	// The picture owns the palette and leaves no entry spare, so the mouth
	// takes index 0. That entry is all but black already, and claiming it
	// outright costs the picture nothing.
	RETRO_SetColor(0, 0, 0, 0);
}
