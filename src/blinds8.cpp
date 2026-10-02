//
// Blinds 8
//
// A vertical-blind transition between two pictures, with the slats in
// perspective. The screen is split into vertical slats, each carrying a strip
// of one picture on its front and the same strip of the other picture on its
// back. Each slat turns half a revolution around a vertical axis through its
// center, and is seen by an eye in front of the middle of the screen. The edge
// of a slat that swings toward the eye grows taller and the edge that swings
// away grows shorter, a slat off to one side still shows a sliver of a face
// when it is edge-on to the screen, and a turned slat can reach in front of
// its neighbor. The black curtain shows wherever no slat is.
//
// A point of a slat at the distance u from its axis, turned by the angle a, is
//
//   X = c + u cos a
//   Z = u sin a
//
// with c the axis and Z the depth behind the screen. The eye is at the depth
// -D, in front of the screen center x0, and sees that point on the screen at
// x0 + (X - x0) D / (D + Z). The slat is drawn from the other end: the ray
// through a screen column x meets the slat's plane at
//
//   u = (x - c) / (cos a - (x - x0) sin a / D)
//
// and the column belongs to the slat if u is within half its width. The sign
// of the denominator is the side the eye is looking at. The axis is vertical,
// so a whole screen column of a slat is at the one depth Z = u sin a: the
// picture's rows are scaled around the screen center by D / (D + Z), and a
// nearer slat hides a farther one a whole column at a time, which takes one
// depth per screen column to settle.
//
// Each slat starts its turn a little later than the one to its right - the
// delay is a linear ramp across the screen - so the turn travels leftwards
// instead of flipping every slat at once. The finished transition rests on the
// second picture for a moment, then runs backwards: the slats turn back to the
// first picture, the turning traveling rightwards, and the cycle starts over.
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

#define BLIND_WIDTH 32
#define BLINDS ((RETRO_WIDTH + BLIND_WIDTH - 1) / BLIND_WIDTH)
#define EYE_DISTANCE 160.0 // pixels from the eye to the screen
#define CENTER_X (RETRO_WIDTH / 2.0)
#define CENTER_Y (RETRO_HEIGHT / 2.0)
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

	// The depth of the nearest slat drawn in every screen column
	double depth[RETRO_WIDTH];
	for (int x = 0; x < RETRO_WIDTH; x++) {
		depth[x] = EYE_DISTANCE;
	}

	// RETRO clears the framebuffer before this callback, leaving the curtain.
	for (int blind = 0; blind < BLINDS; blind++) {
		int left = blind * BLIND_WIDTH;
		int width = MIN(BLIND_WIDTH, RETRO_WIDTH - left);
		double half = width / 2.0;
		double center = left + half;

		// The rotation starts at the right and travels left. Each slat turns
		// through 180 degrees: front broadside, edge-on, back broadside.
		double delay = (double)(BLINDS - 1 - blind) / (BLINDS - 1)
			* (1 - PHASE_WINDOW);
		double rotation = CLAMP01((phase - delay) / PHASE_WINDOW);
		double cosine = cos(rotation * M_PI);
		double sine = sin(rotation * M_PI);
		int shade = lround(fabs(cosine) * (SHADES - 1));

		// The screen columns between the slat's two edges, each edge projected
		// from where the turn has taken it
		double edgea = CENTER_X + (center - half * cosine - CENTER_X)
			* EYE_DISTANCE / (EYE_DISTANCE - half * sine);
		double edgeb = CENTER_X + (center + half * cosine - CENTER_X)
			* EYE_DISTANCE / (EYE_DISTANCE + half * sine);
		int first = MAX((int)floor(MIN(edgea, edgeb)), 0);
		int last = MIN((int)ceil(MAX(edgea, edgeb)), RETRO_WIDTH);

		for (int x = first; x < last; x++) {
			// Where the ray through the center of this column meets the slat.
			// A ray along the slat's own plane meets it nowhere.
			double facing = cosine - (x + 0.5 - CENTER_X) * sine / EYE_DISTANCE;
			if (facing == 0) {
				continue;
			}
			double u = (x + 0.5 - center) / facing;
			if (u < -half || u >= half) {
				continue;
			}

			// A nearer slat already has this column
			double z = u * sine;
			if (z >= depth[x]) {
				continue;
			}
			depth[x] = z;

			// The back carries its strip the way it reads once the slat has
			// turned over, which on the slat itself is right to left
			unsigned char *image = facing < 0 ? PictureB->data : PictureA->data;
			int column = (int)(u + half);
			if (facing < 0) {
				column = width - 1 - column;
			}

			// The rows this column shows, farther apart in the picture the
			// farther away the column is. Past the picture's top and bottom
			// the curtain shows.
			double scale = (EYE_DISTANCE + z) / EYE_DISTANCE;
			for (int y = 0; y < RETRO_HEIGHT; y++) {
				int row = (int)floor(CENTER_Y + (y + 0.5 - CENTER_Y) * scale);
				if (row >= 0 && row < RETRO_HEIGHT) {
					buffer[y * RETRO_WIDTH + x] = ShadeTable[image[row * RETRO_WIDTH + left + column]][shade];
				}
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
