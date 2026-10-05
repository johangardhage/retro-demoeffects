//
// Unlimited bobs
//
// A chain of bobs that never ends, from one bob drawn per step. There are
// BOB_SCREENS screens, none of them ever cleared, and step n draws a single
// bob at the path's position p(n) into screen n mod BOB_SCREENS, which is the
// one shown. That screen still holds the bobs of steps n − BOB_SCREENS,
// n − 2 BOB_SCREENS and so on, so what shows is a chain
//
//   p(n), p(n − S), p(n − 2 S), …   S = BOB_SCREENS
//
// with the newest on top. The next step shows the next screen, whose chain is
// the same one a step further along, so the whole chain appears to move
// along the path, its spacing S steps of travel, and to grow by one bob every
// S steps. Nothing is ever erased, so the count only grows.
//
// The path is a Lissajous figure whose y rate swings slowly about its mean,
//
//   x = W/2 + BOB_AMPX sin xphase
//   y = H/2 + BOB_AMPY sin yphase
//   yphase' = BOB_SPEEDY (1 + BOB_WOBBLE sin driftphase)
//
// so the figure keeps reshaping and the new chain crosses the old ones
// rather than retracing them. The bob is a shaded ball, and every
// BOB_COLORTIME seconds it takes the next of BOB_COLORS ramps, so each pass
// of the path is laid down in its own color.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retropalette.h"

#define BOB_SCREENS 8 // screens drawn in turn, and the chain's spacing in steps
#define BOB_SIZE 24 // pixels across
#define BOB_AMPX 136 // pixels either side of the center
#define BOB_AMPY 96
#define BOB_SPEEDX 0.9 // radians a second
#define BOB_SPEEDY 1.3
#define BOB_WOBBLE 0.3 // how far the y rate swings either side of BOB_SPEEDY
#define BOB_DRIFT 0.11 // radians a second the swing moves through
#define BOB_COLORS 4 // ramps, one per pass
#define BOB_SHADES 60 // palette entries a ramp spends on the ball
#define BOB_COLORTIME 5.0 // seconds a ramp is drawn before the next
#define BOB_SHADOW 0.6 // brightness of a ramp's darkest shade, as a fraction of its hue

static const RETRO_Palette BobHues[BOB_COLORS] = { RETRO_CYAN, RETRO_ORANGE, RETRO_SPRINGGREEN, RETRO_HOTPINK };

static unsigned char Screens[BOB_SCREENS][RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char BobMap[BOB_COLORS][BOB_SIZE * BOB_SIZE];
static int Step;

void DEMO_FixedUpdate(RETRO_Time time)
{
	// Calculate phase
	static double xphase = 0, yphase = M_PI / 2, driftphase = 0;

	xphase = fmod(xphase + BOB_SPEEDX * time.delta, 2 * M_PI);
	yphase = fmod(yphase + BOB_SPEEDY * (1 + BOB_WOBBLE * sin(driftphase)) * time.delta, 2 * M_PI);
	driftphase = fmod(driftphase + BOB_DRIFT * time.delta, 2 * M_PI);

	// Draw one bob into the next screen
	Step = (Step + 1) % BOB_SCREENS;
	int x = RETRO_WIDTH / 2 + BOB_AMPX * sin(xphase);
	int y = RETRO_HEIGHT / 2 + BOB_AMPY * sin(yphase);
	int color = (int)(time.total / BOB_COLORTIME) % BOB_COLORS;
	RETRO_DrawSprite(x, y, BOB_SIZE, BOB_SIZE, BOB_SIZE, BOB_SIZE, BobMap[color], 0, -1, {}, Screens[Step]);
}

void DEMO_Render(RETRO_Time time)
{
	// Show the screen the last bob went into
	RETRO_Blit(Screens[Step]);
}

void DEMO_Initialize(void)
{
	// Init palette. One ramp per color, shadow to hue to white. Entry 0 is the
	// background and the sprites' alpha. The shadow is a dim hue, not black,
	// so where a bob covers another its unlit side shows as a soft rim in its
	// own color rather than a black one
	RETRO_SetColor(0, RETRO_BLACK);

	for (int k = 0; k < BOB_COLORS; k++) {
		int ramp = 1 + k * BOB_SHADES;
		int middle = ramp + (BOB_SHADES * 2) / 3;
		RETRO_Palette hue = BobHues[k];
		RETRO_Palette shadow = hue * BOB_SHADOW;

		RETRO_CreateGradientPalette(ramp, middle, shadow, hue);
		RETRO_CreateGradientPalette(middle, ramp + BOB_SHADES, hue, RETRO_WHITE);
	}

	// Init bob sprites, one per ramp
	for (int k = 0; k < BOB_COLORS; k++) {
		RETRO_CreateBallMap(BobMap[k], NULL, BOB_SIZE, 1 + k * BOB_SHADES, BOB_SHADES);
	}
}
