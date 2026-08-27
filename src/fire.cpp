//
// Fire
//
// A temperature field, one byte per pixel. Nothing is drawn as a flame:
// heat is seeded at the bottom and the picture is whatever that heat
// becomes after it has risen and cooled. This is not the heat equation
// (no self term, no κ∇²T). It is the classic 8-tap fire CA:
//
//   T'(x, y) = max(0, mean(T at the eight taps) − FIRE_DECAY)
//
// Taps: left/right on this row, and the three cells one and two rows
// below. No self, so a cell keeps none of its own heat. Six of the
// eight sit below it (y grows down), so each step lifts heat from
// y+1, y+2 onto y. Every tap reads the previous step. Out-of-range
// taps wrap on both axes: the x wrap joins the flame's left and right
// edges. The
// y wrap only hits the last two hidden rows, and those taps land on
// buffer rows 0 and 1. A spark writes 255
// into the bottom FIRE_HEIGHT rows of a column with probability
// 1/FIRE_CHAOS. The blit hides that bed and leaves a clear strip
// above the flame.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retrocolor.h"

#define FIRE_HEIGHT 10 // rows of fuel along the bottom, cropped off the blit so the bed is hidden
#define FIRE_CHAOS 6 // a column is sparked with probability 1 / FIRE_CHAOS each step
#define FIRE_DECAY 3 // subtracted after the 8-tap average, so how fast a flame dies as it rises

unsigned char FireBuffer[RETRO_HEIGHT * RETRO_WIDTH];

//
// Advance the field one fixed step
//
// A spark writes 255 into the bottom FIRE_HEIGHT pixels of a column. The blur then
// replaces every cell with the mean of eight neighbours - left and right on its own
// row, and the three cells one and two rows below - and subtracts FIRE_DECAY:
//
//   T'(x, y) = max(0, (T(x-1, y) + T(x+1, y)
//                    + T(x-1, y+1) + T(x, y+1) + T(x+1, y+1)
//                    + T(x-1, y+2) + T(x, y+2) + T(x+1, y+2)) / 8
//                    - FIRE_DECAY)
//
// There is no self term, so a cell keeps none of its own heat. Six of the eight taps
// sit below it, so the mean is a sample of the heat underneath, which is what makes
// the field rise: each step copies heat from y+1, y+2 up onto y. The two side taps
// smear a column into its neighbours, and wrapping those in x lets a flame at the
// edge continue on the other side.
//
// RETRO_Blur keeps the rows it has already written, so every tap reads the previous
// step: a Jacobi update on both axes, and the flame rises straight.
//
// A uniform column that only decayed would fall from 255 to 0 in 255 / FIRE_DECAY
// steps, about eighty-five here. Averaging with cooler neighbours kills a flame
// sooner, so the visible height is shorter than that and set by how often the bed
// is re-sparked.
//
// The step is the unit of both the rise and the cooling, which is why the field is
// advanced at a fixed rate instead of once per frame.
//
void DEMO_FixedUpdate(double timestep)
{
	// Seed sparks
	for (int x = 0; x < RETRO_WIDTH; x++) {
		if (RANDOM(FIRE_CHAOS) == 0) {
			for (int y = RETRO_HEIGHT - FIRE_HEIGHT; y < RETRO_HEIGHT; y++) {
				FireBuffer[y * RETRO_WIDTH + x] = 255;
			}
		}
	}

	// Blur field
	RETRO_Blur(RETRO_BLUR_FIRE, FIRE_DECAY, RETRO_BLUR_WRAP, FireBuffer);
}

void DEMO_Render(double deltatime)
{
	// Draw fire. The fuel bed is the bottom FIRE_HEIGHT rows. Blitting from the top of the field
	// onto the screen starting FIRE_HEIGHT rows down hides that bed and leaves a strip
	// of cleared background above the flame.
	RETRO_Blit(FireBuffer, (RETRO_HEIGHT - FIRE_HEIGHT) * RETRO_WIDTH, RETRO_FrameBuffer() + FIRE_HEIGHT * RETRO_WIDTH);
}

void DEMO_Initialize(void)
{
	// Init palette. Index is temperature, so the ramp is the cooling curve: white
	// at the source, through yellow and red, into a blue smoke that fades to black.
	RETRO_CreateGradientPalette(0, 32, RETRO_BLACK, RETRO_BLUEBLACK);
	RETRO_CreateGradientPalette(32, 64, RETRO_BLUEBLACK, RETRO_RED);
	RETRO_CreateGradientPalette(64, 128, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(128, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
