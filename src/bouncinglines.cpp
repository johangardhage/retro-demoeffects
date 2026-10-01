//
// Bouncing lines
//
// LINES lines, each with its two ends flying about the screen on their own,
// straight and at a steady speed, and bouncing off its edges. Behind it a line
// leaves a trail: it is drawn TRAIL times over, as it was every TRAIL_STEP
// back in time, the oldest first and the darkest, so the trail fans out
// behind the line where its ends go different ways and folds over itself
// where one of them has just bounced.
//
// An end that bounces between two walls goes back and forth over the same
// stretch, which is a triangle wave. A coordinate that would have reached p
// had there been no walls is folded back into the screen,
//
//   m = p mod 2 (size - 1)
//   m ≤ size - 1:  m                   on its way out
//   otherwise:     2 (size - 1) - m    on its way back
//
// so where an end is follows from the time alone, and the line as it was a
// while ago is the same sum with an earlier time and nothing has to be kept.
// The speeds are whole pixels per second, so every coordinate is back where
// it began after LINE_PERIOD, which phase lives on.
//
// A line goes round the color circle once in HUE_PERIOD, each line a part of
// a turn ahead of the first, and what it leaves behind keeps the hue it had
// when it was there. So the trail is not one color fading out but runs through
// the hues the line has just been, and the palette, which has an entry for
// every place in every trail, is set anew every frame.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define LINES 2
#define TRAIL 28 // times a line is drawn
#define TRAIL_STEP 0.035 // seconds from one of them back to the next
#define LINE_RAMP0 1 // palette entry the first line's trail starts at, past the background
#define LINE_PERIOD 152482.0 // seconds, lcm(2 (WIDTH - 1), 2 (HEIGHT - 1))
#define HUE_PERIOD 11.0 // seconds round the color circle, a divisor of LINE_PERIOD

struct Line {
	vec2 from, fromspeed; // where one end began, in pixels, and its pixels per second
	vec2 to, tospeed; // and the other
	float hue; // turns round the color circle it is ahead of the first line
};

static const Line Lines[LINES] = {
	{ { 40, 60 }, { 97, 71 }, { 250, 30 }, { -83, 113 }, 0 },
	{ { 200, 200 }, { 61, -103 }, { 90, 120 }, { 119, 67 }, 0.5f },
};

//
// Where between the walls at 0 and size - 1 a coordinate is that would have
// been at position without them
//
static int Bounce(double position, int size)
{
	double m = mod(position, 2 * (size - 1));
	return lround(m > size - 1 ? 2 * (size - 1) - m : m);
}

//
// The color a part of a turn round the color circle: red at 0, then yellow,
// green, cyan, blue and magenta a sixth of a turn apart
//
static RETRO_Palette Hue(double turns)
{
	double sixths = fract(turns) * 6;
	return {
		(unsigned char)(255 * CLAMP01(fabs(sixths - 3) - 1)),
		(unsigned char)(255 * CLAMP01(2 - fabs(sixths - 2))),
		(unsigned char)(255 * CLAMP01(2 - fabs(sixths - 4))),
	};
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time, LINE_PERIOD);

	// Draw lines, each as it was every step back in time, the oldest first and
	// the darkest, in the hue the line had then
	for (int i = 0; i < LINES; i++) {
		const Line &line = Lines[i];

		for (int j = TRAIL - 1; j >= 0; j--) {
			double t = phase - j * TRAIL_STEP;
			int entry = LINE_RAMP0 + i * TRAIL + j;
			RETRO_Palette color = Hue(line.hue + t / HUE_PERIOD);
			RETRO_SetColor(entry, color.r * (TRAIL - j) / TRAIL, color.g * (TRAIL - j) / TRAIL, color.b * (TRAIL - j) / TRAIL);

			int x1 = Bounce(line.from.x + line.fromspeed.x * t, RETRO_WIDTH);
			int y1 = Bounce(line.from.y + line.fromspeed.y * t, RETRO_HEIGHT);
			int x2 = Bounce(line.to.x + line.tospeed.x * t, RETRO_WIDTH);
			int y2 = Bounce(line.to.y + line.tospeed.y * t, RETRO_HEIGHT);

			RETRO_DrawLine(x1, y1, x2, y2, entry);
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Entry 0 is the black background. The entries after it, one
	// for every place in every trail, are set as the lines are drawn
	RETRO_SetColor(0, RETRO_BLACK);
}
