//
// Ripples
//
// A reflection of the still picture in a horizontal trough at WATER_YPOS
// (the first water row; on this photo that is the hot-spring surface).
// Row y ≥ WATER_YPOS samples
//
//   ysrc = 2 WATER_YPOS − y + A sin(2π N_waves (y + t) / N)
//
// the integer mirror of y in the trough, plus a sine of WATER_WAVES
// cycles over the N-entry table. N holds a whole number of waves so the
// wrap is exact; t lives on N. With A = 3 the source stays on the
// picture (ysrc ∈ [128, 188]). The source is the already-blitted
// framebuffer, so a sample that lands on a previous water row reads the
// reflection of the reflection; that is occasional and is the look.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define WATER_YPOS 185 // first water row; mirror plane
#define WATER_WAVES 11 // cycles packed into the table
#define WATER_AMPLITUDE 3 // peak row offset, pixels
#define SINE_VALUES 100 // table length; 11 waves so the wrap is exact
#define WATER_SPEED 30 // table entries travelled per second

int SinTable[SINE_VALUES];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * WATER_SPEED, SINE_VALUES);
	int iphase = (int)phase;

	unsigned char *buffer = RETRO_FrameBuffer();

	// Draw background
	RETRO_Blit(RETRO_ImageData());

	// Draw ripples
	for (int y = WATER_YPOS; y < RETRO_HEIGHT; y++) {
		int ysrc = WATER_YPOS + (WATER_YPOS - y) + SinTable[WRAP(y + iphase, SINE_VALUES)];

		RETRO_Blit(buffer + ysrc * RETRO_WIDTH, RETRO_WIDTH, buffer + y * RETRO_WIDTH);
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/monkey_320x240.pcx");
	RETRO_SetPalette(RETRO_ImagePalette());

	// Init sine table with a whole number of waves, so it wraps smoothly
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = lround(WATER_AMPLITUDE * sin(2 * M_PI * i * WATER_WAVES / SINE_VALUES));
	}
}
