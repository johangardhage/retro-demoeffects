//
// Plasma, chunky
//
// Three moving sine waves are averaged into a low-resolution color field. One
// wave travels horizontally, another vertically, and a third crosses the image
// diagonally. The diagonal wave continually changes its horizontal and vertical
// slopes, bending the interference bands into drifting blobs and ribbons. Each
// wave is raised and scaled into 0..255, and the phase grows smoothly with time.
//
//   W(a) = 255 (1 + sin a) / 2
//   sx = π (1 + sin 3t),  sy = π (1 + cos 5t)
//   c = (W(k x + 2t) + W(k y + 3t) + W((sx x + sy y) / w + t)) / 3
//
// The field is calculated at 80x50 and every sample is expanded to a 4x4 block,
// giving the effect its deliberately chunky pixels. Color indices run through a
// fire palette from dark blue and red to orange, yellow and white.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

constexpr int PLASMA_SCALE = 4; // pixels on a side of the block a sample is expanded to
constexpr int PLASMA_WIDTH = RETRO_WIDTH / PLASMA_SCALE;
constexpr int PLASMA_HEIGHT = RETRO_HEIGHT / PLASMA_SCALE;
constexpr double PLASMA_SPEED = radians(70); // a second of the slowest wave
constexpr double PLASMA_DENSITY = radians(3); // per sample of the horizontal and vertical waves

// A sine raised and scaled into 0..255
static double Wave(double angle)
{
	return 255 * (1 + sin(angle)) / 2;
}

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	double phase = fmod(time.total * PLASMA_SPEED, 2 * M_PI);

	unsigned char *buffer = RETRO_FrameBuffer();

	// Radians the diagonal wave turns per field width, up to one turn. Both axes
	// share the width so the wave is as steep down as across
	double slopex = M_PI * (1 + sin(phase * 3));
	double slopey = M_PI * (1 + cos(phase * 5));

	for (int y = 0; y < PLASMA_HEIGHT; y++) {
		double vertical = Wave(y * PLASMA_DENSITY + phase * 3);

		for (int x = 0; x < PLASMA_WIDTH; x++) {
			double horizontal = Wave(x * PLASMA_DENSITY + phase * 2);
			double diagonal = Wave((x * slopex + y * slopey) / PLASMA_WIDTH + phase);
			unsigned char color = (horizontal + diagonal + vertical) / 3;

			int left = x * PLASMA_SCALE;
			int top = y * PLASMA_SCALE;
			for (int yy = 0; yy < PLASMA_SCALE; yy++) {
				memset(buffer + (top + yy) * RETRO_WIDTH + left, color, PLASMA_SCALE);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, 8, RETRO_BLACK, RETRO_BLUEBLACK);
	RETRO_CreateGradientPalette(8, 24, RETRO_BLUEBLACK, RETRO_DARKRED);
	RETRO_CreateGradientPalette(24, 56, RETRO_DARKRED, RETRO_SCARLET);
	RETRO_CreateGradientPalette(56, 192, RETRO_SCARLET, RETRO_YELLOW);
	RETRO_CreateGradientPalette(192, RETRO_COLORS, RETRO_YELLOW, RETRO_WHITE);
}
