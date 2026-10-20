//
// Dot flag
//
// The flag of Sweden as a grid of dots. SFS 1982:269 is 16:10, split
// 5:2:9 hoist-to-fly and 4:2:4 top-to-bottom (cross toward the hoist,
// both arms two cells). Each spec cell is FLAG_SUBDIV² dots. The rest
// positions span (n − 1) DOT_SPACING, and that box is centered on the
// screen.
//
// Each dot is displaced by a traveling sine, rounded to whole pixels.
// The phase t grows smoothly with time.
//
//   Δx = A sin(t + (xp+yp) κ + xp σ)
//   Δy = A sin(t + (xp+yp) κ + yp σ)
//
// κ is radians of phase per dot along the diagonal, σ along the axis the
// displacement acts on. Without σ the two sines are equal and every
// dot moves on the same diagonal.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

constexpr int FLAG_SUBDIV = 4; // dots per cell of the flag, in each direction
constexpr int DOT_SPACING = 4; // pixels between neighboring dots

constexpr int WAVE_AMPLITUDE = 10; // pixels a dot is displaced at the crest
constexpr double WAVE_CURVE = radians(5); // phase per dot along the diagonal
constexpr double WAVE_SKEW = radians(2); // and per dot along the axis the displacement acts on
constexpr double WAVE_SPEED = radians(200); // phase the wave travels per second

// Flag of Sweden, SFS 1982:269: 16:10, divided 5:2:9 across and 4:2:4 down.
static const unsigned char SwedishFlag[][16] = {
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

constexpr int FLAG_COLUMNS = (int)sizeof(SwedishFlag[0]);
constexpr int FLAG_ROWS = (int)(sizeof(SwedishFlag) / sizeof(SwedishFlag[0]));

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * WAVE_SPEED, 2 * M_PI);

	int left = (RETRO_WIDTH - (FLAG_COLUMNS * FLAG_SUBDIV - 1) * DOT_SPACING) / 2;
	int top = (RETRO_HEIGHT - (FLAG_ROWS * FLAG_SUBDIV - 1) * DOT_SPACING) / 2;

	// Draw flag
	for (int celly = 0; celly < FLAG_ROWS; celly++) {
		for (int cellx = 0; cellx < FLAG_COLUMNS; cellx++) {
			unsigned char color = SwedishFlag[celly][cellx];

			for (int doty = 0; doty < FLAG_SUBDIV; doty++) {
				for (int dotx = 0; dotx < FLAG_SUBDIV; dotx++) {
					int xp = cellx * FLAG_SUBDIV + dotx;
					int yp = celly * FLAG_SUBDIV + doty;

					double along = phase + (xp + yp) * WAVE_CURVE;
					int wavex = lround(WAVE_AMPLITUDE * sin(along + xp * WAVE_SKEW));
					int wavey = lround(WAVE_AMPLITUDE * sin(along + yp * WAVE_SKEW));

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
}
