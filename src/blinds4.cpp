//
// Blinds 4
//
// A horizontal-blind transition between two pictures. The screen is split into
// narrow horizontal slats, each carrying a strip of one picture on its front
// and the same strip of the other picture on its back. Each slat turns half a
// revolution around its center: its front is squeezed toward a line, and its
// back opens out from that line in its place, with the black curtain showing
// in the gaps while the slat is turned. Each slat starts its turn a little
// later than the one above it - the delay is a linear ramp down the screen -
// so the turn travels downwards instead of flipping every slat at once. The
// finished transition rests on the second picture for a moment, then runs
// backwards: the slats turn back to the first picture, the turning traveling
// upwards, and the cycle starts over.
//
// A slat is darkened as it turns away. The two pictures share one palette, so
// the darker shades of a color are the nearest entries that palette already
// has, looked up in a shade table.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define BLIND_HEIGHT 16
#define BLINDS ((RETRO_HEIGHT + BLIND_HEIGHT - 1) / BLIND_HEIGHT)
#define PHASE_WINDOW 0.72 // part of the transition spent rotating one slat
#define TIME_TRANSITION 2.0
#define TIME_HOLD 1.0 // seconds held on either picture
#define TIME_CYCLE (2 * (TIME_HOLD + TIME_TRANSITION))
#define CURTAIN 0
#define SHADES 16
#define AMBIENT 0.25f // light left on a slat that is nearly edge-on

static RETRO_Image *PictureA;
static RETRO_Image *PictureB;
static unsigned char ShadeTable[RETRO_COLORS][SHADES];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase: it rises to 1 as the slats turn over, stays there over
	// the second picture, and falls back to 0 over the last TIME_TRANSITION of
	// the cycle
	double cycle = fmod(time, TIME_CYCLE);
	double phase = CLAMP01(MIN(cycle - TIME_HOLD, TIME_CYCLE - cycle) / TIME_TRANSITION);

	unsigned char *buffer = RETRO_FrameBuffer();

	// RETRO clears the framebuffer before this callback, leaving the curtain.
	for (int blind = 0; blind < BLINDS; blind++) {
		int top = blind * BLIND_HEIGHT;
		int height = MIN(BLIND_HEIGHT, RETRO_HEIGHT - top);

		// The rotation starts at the top and travels down. Each slat turns
		// through 180 degrees: front broadside, edge-on, back broadside.
		double delay = (double)blind / (BLINDS - 1) * (1 - PHASE_WINDOW);
		double rotation = CLAMP01((phase - delay) / PHASE_WINDOW);
		double facing = cos(rotation * M_PI);
		double fold = fabs(facing);

		// Project the rotating slat about its center: the visible span is the
		// height foreshortened, laid out centered on the slat's own center. An
		// odd leftover splits half a pixel upwards, which is invisible against
		// a 16-pixel slat.
		int visible = lround(height * fold);
		int first = top + (height - visible) / 2;
		int shade = lround(fold * (SHADES - 1));

		// The back carries its strip the way it reads once the slat has turned
		// over, so both sides are sampled top to bottom.
		unsigned char *image = facing < 0 ? PictureB->data : PictureA->data;

		for (int y = 0; y < visible; y++) {
			// The whole strip is squeezed into the visible span, sampled at
			// pixel centers
			int row = (2 * y + 1) * height / (2 * visible);
			unsigned char *source = image + (top + row) * RETRO_WIDTH;
			unsigned char *target = buffer + (first + y) * RETRO_WIDTH;

			for (int x = 0; x < RETRO_WIDTH; x++) {
				target[x] = ShadeTable[source[x]][shade];
			}
		}
	}
}

void DEMO_Initialize(void)
{
	PictureA = RETRO_LoadImage("assets/monkey_320x240_quantizized.pcx", true);
	PictureB = RETRO_LoadImage("assets/flowers_320x240_quantizized.pcx");

	if (PictureA->width != RETRO_WIDTH || PictureA->height != RETRO_HEIGHT ||
		PictureB->width != RETRO_WIDTH || PictureB->height != RETRO_HEIGHT) {
		RETRO_RageQuit("The images must be the size of the screen\n");
	}

	// One entry is taken back for the curtain. The pictures use it as one of
	// their own colors, so those pixels move to the nearest of the other
	// entries.
	RETRO_Palette *palette = PictureA->palette;
	unsigned char nearest = 1 + RETRO_NearestPaletteIndex(palette[CURTAIN], palette + 1, RETRO_COLORS - 1);
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		if (PictureA->data[i] == CURTAIN) {
			PictureA->data[i] = nearest;
		}
		if (PictureB->data[i] == CURTAIN) {
			PictureB->data[i] = nearest;
		}
	}
	RETRO_SetColor(CURTAIN, RETRO_BLACK, palette);
	RETRO_SetColor(CURTAIN, RETRO_BLACK);

	// The last shade is every color as it is, so a broadside slat shows its
	// picture untouched. The floor keeps the darkest shades among colors the
	// pictures have plenty of.
	RETRO_CreateShadeTable(palette, RETRO_COLORS, SHADES, &ShadeTable[0][0], AMBIENT);
}
