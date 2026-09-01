//
// Dot flag
//
// The flag of Sweden as a grid of dots. SFS 1982:269 is 16:10, split
// 5:2:9 hoist-to-fly and 4:2:4 top-to-bottom (cross towards the hoist,
// both arms two cells). Each spec cell is FLAG_SUBDIV² dots. The rest
// positions span (n − 1) DOT_SPACING, and that box is centred on the
// screen.
//
// Each dot is displaced by a travelling sine. The table has N entries
// at θ = 2π i / N, so it closes; t is kept on that table.
//
//   Δx = A sin(2π (t + (xp+yp) κ + xp σ) / N)
//   Δy = A sin(2π (t + (xp+yp) κ + yp σ) / N)
//
// κ is phase per dot along the diagonal, σ along the axis the
// displacement acts on. Without σ the two sines are equal and every
// dot moves on the same diagonal.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define FLAG_SUBDIV 4 // dots per cell of the flag, in each direction
#define DOT_SPACING 4 // pixels between neighbouring dots

#define SINE_VALUES 255 // entries in the sine table, covering one whole turn
#define WAVE_AMPLITUDE 10 // pixels a dot is displaced at the crest
#define WAVE_CURVE 5 // table entries of phase per dot along the diagonal
#define WAVE_SKEW 2 // and per dot along the axis the displacement acts on
#define WAVE_SPEED 200 // table entries travelled per second

// Flag of Sweden, SFS 1982:269: 16:10, divided 5:2:9 across and 4:2:4 down.
unsigned char SwedishFlag[10][16] = {
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

int SineTable[SINE_VALUES];

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * WAVE_SPEED, SINE_VALUES);
	int iphase = phase;

	int flagwidth = sizeof(SwedishFlag[0]);
	int flagheight = sizeof(SwedishFlag) / sizeof(SwedishFlag[0]);

	int left = (RETRO_WIDTH - (flagwidth * FLAG_SUBDIV - 1) * DOT_SPACING) / 2;
	int top = (RETRO_HEIGHT - (flagheight * FLAG_SUBDIV - 1) * DOT_SPACING) / 2;

	// Draw flag
	for (int celly = 0; celly < flagheight; celly++) {
		for (int cellx = 0; cellx < flagwidth; cellx++) {
			unsigned char color = SwedishFlag[celly][cellx];

			for (int doty = 0; doty < FLAG_SUBDIV; doty++) {
				for (int dotx = 0; dotx < FLAG_SUBDIV; dotx++) {
					int xp = cellx * FLAG_SUBDIV + dotx;
					int yp = celly * FLAG_SUBDIV + doty;

					int wavex = SineTable[WRAP(iphase + (xp + yp) * WAVE_CURVE + xp * WAVE_SKEW, SINE_VALUES)];
					int wavey = SineTable[WRAP(iphase + (xp + yp) * WAVE_CURVE + yp * WAVE_SKEW, SINE_VALUES)];

					RETRO_PutPixel(left + xp * DOT_SPACING + wavex, top + yp * DOT_SPACING + wavey, color);
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(1, RETRO_CERULEAN);
	RETRO_SetColor(2, RETRO_GOLD);

	// Init tables
	for (int i = 0; i < SINE_VALUES; i++) {
		SineTable[i] = lround(WAVE_AMPLITUDE * sin(2 * M_PI * i / SINE_VALUES));
	}
}
