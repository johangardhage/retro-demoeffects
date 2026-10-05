//
// Roll-in
//
// A picture rolled up like a poster at the top of the screen unrolls down it,
// lies flat for a while and is rolled up again. The paper lies flat, picture
// up, down to the line y0 where the roll rests on it, and from there it is
// wound up and around the roll. The paper s further on than the line has
// turned through s / r about the middle of the roll, which is r above the
// line, and is seen straight from the front at
//
//   y = y0 + r sin(s / r)
//
// so a scanline within r of the line cuts the roll at up to two places, and
// the nearer of them to the eye that there is paper at wins:
//
//   outside  s = r (π - asin(d / r))      the top of the roll, back of the
//                                         paper up
//   inside   s = r asin(d / r), d ≥ 0     the paper on its way up from the
//                                         line, picture up
//
// with d = y - y0. Row y0 + s of the picture is drawn there. The paper is thin
// enough for the picture to show through its back, where it is mirrored: the
// row drawn goes up the picture as the scanline goes down the roll. Paper
// ends at the bottom row of the picture, so near the end, when less of it is
// left than goes around the roll, the roll opens into a lip that lies down
// flat. A scanline the roll does not cover shows the flat picture above the
// line and nothing below it.
//
// The roll gets thinner as the paper comes off it. The area of its end is the
// paper left on it seen edge on, and goes from that of a circle of
// RADIUS_ROLLED to that of one of RADIUS_UNROLLED in step with the rows left,
//
//   r² = RADIUS_UNROLLED² + (RADIUS_ROLLED² - RADIUS_UNROLLED²) left / HEIGHT
//
// The light comes straight from the front. The flat picture has all of it,
// and the roll the cosine of the angle it is turned away by, √(1 - (d / r)²),
// from all of it along the middle down to AMBIENT at the edges. The picture
// has a palette of its own with no darker copies of its colors in it, so the
// screen gets a palette fitted to the picture's colors at every one of SHADES
// levels of light, and a shade table holds the entry nearest every color at
// every level. The level a pixel is drawn at is ordered-dithered between
// neighbors.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define RADIUS_ROLLED 28.0f // pixels, of the roll with all of the picture on it
#define RADIUS_UNROLLED 9.0f // and with none of it
#define TIME_ROLL 4.0 // seconds the picture takes to unroll, or to be rolled up
#define TIME_FLAT 3.0 // seconds it lies flat
#define TIME_ROLLED 0.5 // seconds it stays rolled up
#define TIME_CYCLE (TIME_ROLLED + TIME_ROLL + TIME_FLAT + TIME_ROLL)
#define AMBIENT 0.2f // light on the roll where it turns edge on
#define SHADES 64 // levels of light

static RETRO_ColorHistogram Histogram;
static unsigned char ShadeTable[RETRO_COLORS * SHADES];

//
// A picture color in the light, shade going from the least of it to all of it
//
static RETRO_Palette Light(RETRO_Palette color, float shade, const float *tint)
{
	return RETRO_ShadeColor(color, mix(AMBIENT, 1, shade));
}

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase: rolled up, unrolling, flat, and being rolled up
	double phase = fmod(time.total, TIME_CYCLE);
	float unrolled = smoothstep(TIME_ROLLED, TIME_ROLLED + TIME_ROLL, phase) - smoothstep(TIME_CYCLE - TIME_ROLL, TIME_CYCLE, phase);

	// The line the roll rests on, the rows of paper left on it, and its radius
	float line = RETRO_HEIGHT * unrolled;
	float left = RETRO_HEIGHT - line;
	float radius = sqrtf(RADIUS_UNROLLED * RADIUS_UNROLLED + (RADIUS_ROLLED * RADIUS_ROLLED - RADIUS_UNROLLED * RADIUS_UNROLLED) * left / RETRO_HEIGHT);

	unsigned char *buffer = RETRO_FrameBuffer();
	unsigned char *image = RETRO_ImageData();

	// Draw picture, a scanline at a time
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		float d = (y + 0.5f - line) / radius;
		float along = -1; // paper from the line to what the scanline shows, if any
		float light = 1;

		if (fabsf(d) < 1) {
			float outside = radius * (M_PI - asinf(d));
			float inside = radius * asinf(d);

			if (outside < left) {
				along = outside;
			} else if (d >= 0 && inside < left) {
				along = inside;
			}
			light = sqrtf(1 - d * d);
		}
		if (along < 0) {
			// No roll here: the flat picture above the line, nothing below it
			if (d >= 0) {
				continue;
			}
			along = y + 0.5f - line;
			light = 1;
		}

		int row = MIN((int)(line + along), RETRO_HEIGHT - 1);
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int shade = MIN((int)(light * (SHADES - 1) + RETRO_DitherThreshold(x, y)), SHADES - 1);
			buffer[y * RETRO_WIDTH + x] = ShadeTable[image[row * RETRO_WIDTH + x] * SHADES + shade];
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/flowers_320x240_quantizized.pcx");

	// The flat picture is all at the top level, so that level is as common as
	// all the others together
	float lightweight[SHADES];
	for (float &weight : lightweight) {
		weight = 1;
	}
	lightweight[SHADES - 1] = SHADES;

	// Init palette, fitted to the picture's colors at every level, and the
	// table that lights the picture's colors onto it. Entry 0 is held for the
	// black of the screen where there is no paper
	RETRO_Palette palette[RETRO_COLORS];
	RETRO_Palette held[] = { RETRO_BLACK };
	RETRO_ShadeTable shadetable = { ShadeTable, RETRO_COLORS, SHADES };
	RETRO_AddShadeTableColors(&Histogram, 0, shadetable, lightweight, Light);
	RETRO_CreateHistogramPalette(&Histogram, palette, held, 1);
	RETRO_SetPalette(palette);
	RETRO_CreateShadeTable(RETRO_ImagePalette(), palette, shadetable, Light);
}
