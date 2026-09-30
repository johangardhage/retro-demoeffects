//
// Blinds 7
//
// A blinds wipe between two pictures. The screen is split into vertical slats
// and nothing turns: every slat uncovers its strip of the second picture
// behind an edge that starts at the slat's left side and moves to its right.
// All the edges move together, so the second picture arrives through every
// slat at once, in stripes that widen until they meet.
//
//   open = round(p BLIND_WIDTH)
//
// p runs from 0 to 1 across the transition. The finished transition rests on
// the second picture for a moment, then runs backwards: the edges move back to
// the left and the stripes of the first picture widen in their place, and the
// cycle starts over.
//
// The two pictures share one palette, so a stripe of either is copied to the
// screen as it is.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"

#define BLIND_WIDTH 32
#define BLINDS ((RETRO_WIDTH + BLIND_WIDTH - 1) / BLIND_WIDTH)
#define TIME_TRANSITION 1.5
#define TIME_HOLD 1.0 // seconds held on either picture
#define TIME_CYCLE (2 * (TIME_HOLD + TIME_TRANSITION))

RETRO_Image *PictureA;
RETRO_Image *PictureB;

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase: it rises to 1 as the slats open, stays there over the
	// second picture, and falls back to 0 over the last TIME_TRANSITION of the
	// cycle
	double cycle = fmod(time, TIME_CYCLE);
	double phase = CLAMP01(MIN(cycle - TIME_HOLD, TIME_CYCLE - cycle) / TIME_TRANSITION);

	unsigned char *buffer = RETRO_FrameBuffer();

	for (int blind = 0; blind < BLINDS; blind++) {
		int left = blind * BLIND_WIDTH;
		int width = MIN(BLIND_WIDTH, RETRO_WIDTH - left);

		// The edge is as far into every slat, and a slat cut short by the side
		// of the screen is full when the edge reaches the cut
		int open = MIN((int)lround(phase * BLIND_WIDTH), width);

		// The second picture left of the edge, the first right of it
		for (int y = 0; y < RETRO_HEIGHT; y++) {
			int offset = y * RETRO_WIDTH + left;
			memcpy(buffer + offset, PictureB->data + offset, open);
			memcpy(buffer + offset + open, PictureA->data + offset + open, width - open);
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
}
