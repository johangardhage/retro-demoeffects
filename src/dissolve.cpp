//
// Dissolve
//
// A picture taken away one pixel at a time, in an order that repeats nothing
// and misses nothing. The address of the next pixel is the state of a 17-bit
// maximal-length Galois LFSR:
//
//   s' = (s >> 1) ^ (TAPS * (s & 1))
//
// with taps x^17 + x^14 + 1. Its 2^17 - 1 nonzero states are a permutation of
// 1 .. 131071, so every one of them comes up exactly once before the register
// returns to its seed. That is the whole trick: the walk carries one register
// and no record of where it has been, where dealing from a shuffled list of
// addresses would carry all 76800 of them.
//
// The screen holds 76800 addresses and the register counts to 131071, so a
// state past the last pixel is skipped and 54272 of the pass's steps land on
// nothing. Zero is the one state the register cannot reach, being its own
// successor, so pixel 0 is written by hand when a pass starts.
//
// A pass writes the picture or writes the curtain, alternately, with a hold on
// each. The framebuffer is never cleared: what stands on screen is what the
// passes have left there.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define LFSR_TAPS 0x12000 // x^17 + x^14 + 1, the bits fed back when a 1 shifts out
#define LFSR_STATES ((1 << 17) - 1) // states the register takes before it repeats
#define LFSR_SEED 1
#define TIME_DISSOLVE 2.5 // seconds a pass takes
#define TIME_HOLD 0.5 // seconds held on the picture or on the curtain
#define CURTAIN 0 // palette entry the picture dissolves into

enum { DISSOLVE_IN, DISSOLVE_OUT, HOLD };

void DEMO_Render2(double time, double deltatime)
{
	static int state = HOLD;
	static int next = DISSOLVE_IN;
	static double hold = TIME_HOLD;
	static int lfsr = LFSR_SEED;
	static double phase = 0; // register steps owed to the pass
	static int walked = 0; // register steps the pass has taken

	unsigned char *image = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	if (state == HOLD) {
		hold -= deltatime;
		if (hold <= 0) {
			state = next;
			lfsr = LFSR_SEED;
			phase = 0;
			walked = 0;

			// The register never takes the value 0, so pixel 0 is written here
			buffer[0] = state == DISSOLVE_IN ? image[0] : CURTAIN;
		}
	} else {
		phase += deltatime * (LFSR_STATES / TIME_DISSOLVE);
		int steps = MIN((int)phase, LFSR_STATES);

		while (walked < steps) {
			lfsr = (lfsr >> 1) ^ (LFSR_TAPS * (lfsr & 1));
			walked++;

			if (lfsr < RETRO_WIDTH * RETRO_HEIGHT) {
				buffer[lfsr] = state == DISSOLVE_IN ? image[lfsr] : CURTAIN;
			}
		}

		if (walked == LFSR_STATES) {
			next = state == DISSOLVE_IN ? DISSOLVE_OUT : DISSOLVE_IN;
			state = HOLD;
			hold = TIME_HOLD;
		}
	}

	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	RETRO_Image *image = RETRO_LoadImage("assets/monkey_320x240.pcx", true);
	if (image->width != RETRO_WIDTH || image->height != RETRO_HEIGHT) {
		RETRO_RageQuit("The image must be the size of the screen\n");
	}

	// One entry is taken back for the curtain. The picture keeps the entry as
	// one of its own colors, so wherever it uses it those pixels are black
	// before the dissolve reaches them, which on this photo is a scattering in
	// the dark background.
	RETRO_SetColor(CURTAIN, RETRO_BLACK);
}
