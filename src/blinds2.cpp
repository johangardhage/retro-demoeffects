//
// Blinds 2
//
// A vertical-blind transition over a picture. The picture is split into narrow
// vertical slats, each carrying its own strip of it. Each slat folds around its
// center, so its strip is squeezed toward a line and reveals the black curtain
// behind it. Each slat starts its fold a little later than the one to its
// right - the delay is a linear ramp across the screen - so the fold travels
// leftwards instead of closing every slat at once. The finished transition
// rests on black for a moment, then runs backwards: the slats unfold from
// black to the picture, the unfolding traveling rightwards, and the cycle
// starts over.
//
// A slat is darkened as it turns away. The picture keeps its own palette, so
// the darker shades of a color are the nearest entries that palette already
// has, looked up in a shade table.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define BLIND_WIDTH 16
#define BLINDS ((RETRO_WIDTH + BLIND_WIDTH - 1) / BLIND_WIDTH)
#define PHASE_WINDOW 0.72 // part of the transition spent rotating one slat
#define TIME_TRANSITION 2.0
#define TIME_HOLD 1.0 // seconds held on the picture or on the curtain
#define TIME_CYCLE (2 * (TIME_HOLD + TIME_TRANSITION))
#define CURTAIN 0
#define SHADES 16
#define AMBIENT 0.25f // light left on a slat that is nearly edge-on

static unsigned char ShadeTable[RETRO_COLORS][SHADES];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase: it rises to 1 as the slats fold away, stays there over
	// the curtain, and falls back to 0 over the last TIME_TRANSITION of the cycle
	double cycle = fmod(time, TIME_CYCLE);
	double phase = CLAMP01(MIN(cycle - TIME_HOLD, TIME_CYCLE - cycle) / TIME_TRANSITION);

	unsigned char *image = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	// RETRO clears the framebuffer before this callback, leaving the curtain.
	for (int blind = 0; blind < BLINDS; blind++) {
		int left = blind * BLIND_WIDTH;
		int width = MIN(BLIND_WIDTH, RETRO_WIDTH - left);

		// The rotation starts at the right and travels left. Each slat turns
		// through 270 degrees: edge-on, broadside once more, then edge-on a
		// second time.
		double delay = (double)(BLINDS - 1 - blind) / (BLINDS - 1)
			* (1 - PHASE_WINDOW);
		double rotation = CLAMP01((phase - delay) / PHASE_WINDOW);
		double facing = cos(rotation * 1.5 * M_PI);
		double fold = fabs(facing);

		// Project the rotating slat about its center: the visible span is the
		// width foreshortened, laid out centered on the slat's own center. An
		// odd leftover splits half a pixel to the left, which is invisible
		// against a 16-pixel slat.
		int visible = lround(width * fold);
		int first = left + (width - visible) / 2;
		int shade = lround(fold * (SHADES - 1));

		for (int x = 0; x < visible; x++) {
			// The whole strip is squeezed into the visible span, sampled at
			// pixel centers. Past edge-on the slat shows its back, where the
			// strip reads right to left.
			int column = (2 * x + 1) * width / (2 * visible);
			if (facing < 0) {
				column = width - 1 - column;
			}

			for (int y = 0; y < RETRO_HEIGHT; y++) {
				int offset = y * RETRO_WIDTH;
				buffer[offset + first + x] = ShadeTable[image[offset + left + column]][shade];
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *image = RETRO_LoadImage("assets/monkey_320x240.pcx", true);
	if (image->width != RETRO_WIDTH || image->height != RETRO_HEIGHT) {
		RETRO_RageQuit("The image must be the size of the screen\n");
	}

	// One entry is taken back for the curtain. The picture uses it as one of
	// its own colors, so those pixels move to the nearest of the other entries.
	unsigned char nearest = 1 + RETRO_NearestPaletteIndex(image->palette[CURTAIN], image->palette + 1, RETRO_COLORS - 1);
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		if (image->data[i] == CURTAIN) {
			image->data[i] = nearest;
		}
	}
	RETRO_SetColor(CURTAIN, RETRO_BLACK, image->palette);
	RETRO_SetColor(CURTAIN, RETRO_BLACK);

	// The last shade is every color as it is, so a broadside slat shows the
	// picture untouched. The floor keeps the darkest shades among colors this
	// photo has plenty of.
	RETRO_CreateShadeTable(image->palette, RETRO_COLORS, SHADES, &ShadeTable[0][0], AMBIENT);
}
