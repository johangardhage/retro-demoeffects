//
// Twister 2
//
// A square column drawn one scanline at a time, as twister.cpp does, but on
// an axis that will not hold still. At row y the four vertices sit on the
// circle x = cx(y) + RADIUS sin(θ(y) + k·90°), k = 0..3, and both cx and θ
// are waves running down the column off one clock:
//
//   θ(y)  = TWIST · (1 + sin(phase + y · TWIST_WAVE)) / 2
//   cx(y) = CX + SWAY · sin(phase + y · SWAY_WAVE)
//
// So the twist is not linear in y the way twister.cpp's is. It is a sine of
// y whose amplitude is TWIST, two whole turns, which is what lets the column
// corkscrew back on itself several times over rather than lean through a
// third of a turn. And the axis itself snakes: SWAY_WAVE runs four times as
// fast as the twist wave, so the column wanders about half a period of S
// curve down its length while the twist is still working through an eighth
// of its own.
//
// An edge is drawn only when its left x is smaller than its right x, which
// is the facing test for a 2D silhouette - a back edge has x_left > x_right
// and covers nothing. Spans are half-open, so two faces sharing a vertex
// tile exactly.
//
// The shading is the original's, and it is not a lighting model: a face
// starts at its own base color and steps one entry along the palette per
// pixel drawn. A face that is nearly edge on gets a short ramp and a face
// turned toward the viewer a long one, so the sheen stretches and squeezes
// as the column turns. On the fire ramp that runs each face from a dark red
// up into orange.
//
// phase lives on the 256-unit angle table.
//
// Replicated from https://github.com/root42/twister.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"

#define TWISTER_CX (RETRO_WIDTH / 2)
#define TWISTER_RADIUS 32 // half the column's width
#define TWISTER_SWAY 32 // how far the axis wanders off centre
#define TWISTER_TWIST 512 // angle units the twist sweeps between its extremes, two whole turns
#define TWIST_WAVE 0.125 // angle units the twist wave advances per row
#define SWAY_WAVE 0.5 // and the axis wave, four times as fast
#define TWISTER_SPEED 70 // angle units a second, the original's one a frame at the VGA's 70 Hz

#define FACE_COLOR 33 // base color of the first face
#define FACE_STEP 16 // and the step from one face's base to the next

#define FIRE_EMBER RETRO_RGB(0x080028) // the blue an all but dead ember glows

//
// One scanline of one face, half-open in x
//
// The color starts at the face's base and steps one entry per pixel from where the span
// would have begun, so clipping the left edge shifts the ramp rather than restarting it.
// A back-facing edge has left >= right and covers nothing, which is the silhouette test.
//
void DrawSpan(int left, int right, int y, unsigned char color)
{
	int start = left;

	left = MAX(left, 0);
	right = MIN(right, RETRO_WIDTH);

	unsigned char *row = RETRO_FrameBuffer() + y * RETRO_WIDTH;
	for (int x = left; x < right; x++) {
		row[x] = color + (x - start);
	}
}

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static double phase = 0;
	phase = fmod(phase + deltatime * TWISTER_SPEED, RETRO_SINCOS_ANGLE);

	// Draw column
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double twist = TWISTER_TWIST * (1 + SIN(phase + y * TWIST_WAVE)) / 2;
		double cx = TWISTER_CX + TWISTER_SWAY * SIN(phase + y * SWAY_WAVE);

		int x[4];
		for (int j = 0; j < 4; j++) {
			x[j] = lround(cx + TWISTER_RADIUS * SIN(twist + j * (RETRO_SINCOS_ANGLE / 4)));
		}

		for (int j = 0; j < 4; j++) {
			DrawSpan(x[j], x[(j + 1) % 4], y, FACE_COLOR + j * FACE_STEP);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The original's fire ramp: a dying ember glows blue, warms through
	// dark red into scarlet, then holds red while the green climbs to yellow, and only
	// the last quarter lets the blue back in to reach white.
	RETRO_CreateGradientPalette(0, 8, RETRO_BLACK, FIRE_EMBER);
	RETRO_CreateGradientPalette(8, 24, FIRE_EMBER, RETRO_DARKRED);
	RETRO_CreateGradientPalette(24, 56, RETRO_DARKRED, RETRO_SCARLET);
	RETRO_CreateGradientPalette(56, 187, RETRO_SCARLET, RETRO_YELLOW);
	RETRO_CreateGradientPalette(187, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
