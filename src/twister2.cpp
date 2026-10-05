//
// Twister 2
//
// A square column drawn one scanline at a time, as twister.cpp does, but on
// an axis that will not hold still. At row y the four vertices sit on the
// circle x = cx(y) + RADIUS sin(θ(y) + k·90°), k = 0..3, and both cx and θ
// are waves running down the column off one clock:
//
//   θ(y)  = TWIST · (1 + sin(phase + y · TWIST_WAVE)) / 2
//   centerx(y) = CENTER_X + SWAY · sin(phase + y · SWAY_WAVE)
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
// as the column turns. Each face has a palette ramp of its own, as long as
// the widest face is, running from a dark shade of its hue up to a bright
// one: pink, lilac, sky blue and gold. A face on face therefore sweeps its
// whole ramp, an edge on one stays in the dark end of it, and at every
// shared vertex the bright end of one hue meets the dark start of the next.
//
// phase lives on the 256-unit angle table.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TWISTER_CENTER_X (RETRO_WIDTH / 2.0)
#define TWISTER_RADIUS 32 // half the column's width
#define TWISTER_SWAY 32 // how far the axis wanders off center
#define TWISTER_TWIST 512 // angle units the twist sweeps between its extremes, two whole turns
#define TWISTER_TWIST_WAVE 0.125 // angle units the twist wave advances per row
#define TWISTER_SWAY_WAVE 0.5 // and the axis wave, four times as fast
#define TWISTER_SPEED 70 // angle units a second, the original's one a frame at the VGA's 70 Hz
#define TWISTER_PERIOD RETRO_ANGLES_PER_TURN

#define TWISTER_FACE_COLOR 64 // base color of the first face
#define TWISTER_FACE_STEP 48 // and the step from one face's base to the next, the widest face rounded up

//
// One scanline of one face, half-open in x
//
// The color starts at the face's base and steps one entry per pixel from where the span
// would have begun, so clipping the left edge shifts the ramp rather than restarting it.
// A back-facing edge has left >= right and covers nothing, which is the silhouette test.
//
static void DrawSpan(int left, int right, int y, unsigned char color)
{
	int start = left;

	left = MAX(left, 0);
	right = MIN(right, RETRO_WIDTH);

	unsigned char *row = RETRO_FrameBuffer() + y * RETRO_WIDTH;
	for (int x = left; x < right; x++) {
		row[x] = color + (x - start);
	}
}

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * TWISTER_SPEED, TWISTER_PERIOD);

	// Draw column
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double twist = TWISTER_TWIST * (1 + SIN(phase + y * TWISTER_TWIST_WAVE)) / 2;
		double centerx = TWISTER_CENTER_X + TWISTER_SWAY * SIN(phase + y * TWISTER_SWAY_WAVE);

		double sinradius = TWISTER_RADIUS * SIN(twist);
		double cosradius = TWISTER_RADIUS * COS(twist);
		int cornerx[4] = {
			(int)lround(centerx + sinradius),
			(int)lround(centerx + cosradius),
			(int)lround(centerx - sinradius),
			(int)lround(centerx - cosradius),
		};

		for (int corner = 0; corner < 4; corner++) {
			DrawSpan(cornerx[corner], cornerx[(corner + 1) & 3], y, TWISTER_FACE_COLOR + corner * TWISTER_FACE_STEP);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(64, 112, RETRO_WINE, RETRO_PINK);
	RETRO_CreateGradientPalette(112, 160, RETRO_INDIGOBLACK, RETRO_LILAC);
	RETRO_CreateGradientPalette(160, 208, RETRO_NAVY, RETRO_LIGHTSKYBLUE);
	RETRO_CreateGradientPalette(208, RETRO_COLORS, RETRO_SCORCHED, RETRO_MARIGOLD);
}
