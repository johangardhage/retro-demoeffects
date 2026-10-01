//
// Blinds 9
//
// A vertical-blind transition between two pictures, with a glint on the
// turning slats. The screen is split into narrow vertical slats, each carrying
// a strip of one picture on its front and the same strip of the other picture
// on its back. Each slat turns half a revolution around its center: its front
// is squeezed toward a line, and its back opens out from that line in its
// place, with the black curtain showing in the gaps while the slat is turned.
//
// A slat is darkened as it turns away, and it catches a lamp on the way. There
// is a lamp to either side of the viewer, and a face glints when it is turned
// to mirror one of them into the eye, which is when its normal lies halfway
// between the two. For a face turned by the angle n away from the viewer and a
// halfway direction at GLINT_ANGLE,
//
//   glint = cos(n - GLINT_ANGLE) ^ GLINT_FALLOFF
//
// The front passes that angle soon after it starts to turn and the back just
// before it settles, so every slat flashes twice on its way over. The glint is
// the color of the lamp: it takes a color toward white rather than brightening
// it.
//
// Each slat starts its turn a little later than the one to its right - the
// delay is a linear ramp across the screen - so the turn travels leftwards
// instead of flipping every slat at once. The finished transition rests on the
// second picture for a moment, then runs backwards: the slats turn back to the
// first picture, the turning traveling rightwards, and the cycle starts over.
//
// The two pictures share one palette, so the darker shades of a color and the
// steps of it toward white are the nearest entries that palette already has,
// looked up in two tables, one after the other.
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
#define TIME_HOLD 1.0 // seconds held on either picture
#define TIME_CYCLE (2 * (TIME_HOLD + TIME_TRANSITION))
#define CURTAIN 0
#define SHADES 16
#define AMBIENT 0.25f // light left on a slat that is nearly edge-on
#define GLINTS 16
#define GLINT_ANGLE radians(35) // turn at which a face mirrors a lamp
#define GLINT_FALLOFF 24.0 // higher is a narrower, briefer flash
#define GLINT_STRENGTH 0.85f // how far toward white the peak of the flash goes

RETRO_Image *PictureA;
RETRO_Image *PictureB;
unsigned char ShadeTable[RETRO_COLORS][SHADES];
unsigned char GlintTable[RETRO_COLORS][GLINTS];

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
		int left = blind * BLIND_WIDTH;
		int width = MIN(BLIND_WIDTH, RETRO_WIDTH - left);

		// The rotation starts at the right and travels left. Each slat turns
		// through 180 degrees: front broadside, edge-on, back broadside.
		double delay = (double)(BLINDS - 1 - blind) / (BLINDS - 1)
			* (1 - PHASE_WINDOW);
		double rotation = CLAMP01((phase - delay) / PHASE_WINDOW);
		double facing = cos(rotation * M_PI);
		double fold = fabs(facing);

		// Project the rotating slat about its center: the visible span is the
		// width foreshortened, laid out centered on the slat's own center. An
		// odd leftover splits half a pixel to the left, which is invisible
		// against a 16-pixel slat.
		int visible = lround(width * fold);
		int first = left + (width - visible) / 2;
		int shade = lround(fold * (SHADES - 1));

		// The face in view is turned acos(fold) away from the viewer, whichever
		// face it is and whichever way it leans, and each has a lamp on the
		// side it leans to
		double mirror = cos(acos(fold) - GLINT_ANGLE);
		int glint = lround(pow(mirror, GLINT_FALLOFF) * (GLINTS - 1));

		// The back carries its strip the way it reads once the slat has turned
		// over, so both sides are sampled left to right.
		unsigned char *image = facing < 0 ? PictureB->data : PictureA->data;

		for (int x = 0; x < visible; x++) {
			// The whole strip is squeezed into the visible span, sampled at
			// pixel centers
			int column = (2 * x + 1) * width / (2 * visible);

			for (int y = 0; y < RETRO_HEIGHT; y++) {
				int offset = y * RETRO_WIDTH;
				int color = ShadeTable[image[offset + left + column]][shade];
				buffer[offset + first + x] = GlintTable[color][glint];
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

	// Every color taken toward white, in even steps from the color itself up
	// to GLINT_STRENGTH of the way there.
	for (int color = 0; color < RETRO_COLORS; color++) {
		for (int glint = 0; glint < GLINTS; glint++) {
			float level = GLINT_STRENGTH * glint / (GLINTS - 1);
			RETRO_Palette target = {
				(unsigned char)(mix(palette[color].r, 255, level) + 0.5f),
				(unsigned char)(mix(palette[color].g, 255, level) + 0.5f),
				(unsigned char)(mix(palette[color].b, 255, level) + 0.5f),
			};
			GlintTable[color][glint] = RETRO_NearestPaletteIndex(target, palette);
		}
	}
}
