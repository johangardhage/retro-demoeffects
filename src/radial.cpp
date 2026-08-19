//
// Radial zoom
//
// Concentric zoom-blur layers of a 320×240 picture, centred on the screen.
// For scales s = 32, 34, ..., 60, screen point p samples
//
//   source = p s/64 + centre (64-s)/64 + orbit (60-s)/128
//   destination += image(source) s/512
//
// The orbit is (100 cos phase, 40 sin phase), so the smaller layers move
// farther from the centre while the full-size layer stays fixed. Adding all
// fifteen layers gives the radial blur.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240
#define RADIAL_MIN_SCALE 32
#define RADIAL_MAX_SCALE 60
#define RADIAL_SCALE_STEP 2
#define RADIAL_SCALE_BASE 64
#define RADIAL_ORBIT_DIVISOR 128
#define RADIAL_BLEND_DIVISOR 512
#define RADIAL_X_RADIUS 80
#define RADIAL_X_ORBIT 100
#define RADIAL_Y_ORBIT 40
#define RADIAL_SPEED 3.0 // radians per second

void DEMO_Render(double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * RADIAL_SPEED, 2 * M_PI);

	float xorbit = RADIAL_X_ORBIT * cos(phase);
	float yorbit = RADIAL_Y_ORBIT * sin(phase);
	int cx = RETRO_WIDTH / 2;
	int cy = RETRO_HEIGHT / 2;

	// Draw zoom layers
	for (int scale = RADIAL_MIN_SCALE; scale <= RADIAL_MAX_SCALE; scale += RADIAL_SCALE_STEP) {
		float xcenter = cx * (RADIAL_SCALE_BASE - scale) / (float)RADIAL_SCALE_BASE;
		float ycenter = cy * (RADIAL_SCALE_BASE - scale) / (float)RADIAL_SCALE_BASE;
		float xoffset = (RADIAL_MAX_SCALE - scale) * xorbit / RADIAL_ORBIT_DIVISOR;
		float yoffset = (RADIAL_MAX_SCALE - scale) * yorbit / RADIAL_ORBIT_DIVISOR;

		int x0 = MAX((int)(cx - xcenter - RADIAL_X_RADIUS), 0);
		int x1 = MIN((int)(cx + xcenter + RADIAL_X_RADIUS), RETRO_WIDTH);
		int y0 = MAX((int)(cy - ycenter), 0);
		int y1 = MIN((int)(cy + ycenter), RETRO_HEIGHT);

		for (int y = y0; y < y1; y++) {
			for (int x = x0; x < x1; x++) {
				int sourcex = (int)(x * scale / (float)RADIAL_SCALE_BASE + xcenter + xoffset);
				int sourcey = (int)(y * scale / (float)RADIAL_SCALE_BASE + ycenter + yoffset);
				if (sourcex < 0 || sourcex >= IMAGE_WIDTH || sourcey < 0 || sourcey >= IMAGE_HEIGHT) {
					continue;
				}

				int offset = y * RETRO_WIDTH + x;
				float color = buffer[offset] + image[sourcey * IMAGE_WIDTH + sourcex] * scale / (float)RADIAL_BLEND_DIVISOR;
				buffer[offset] = CLAMP256(color);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/radial_320x240.pcx", true);
}
