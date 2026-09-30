//
// Truchet
//
// An endless floor of Truchet tiles, turning and zooming as it scrolls past,
// whose arcs join into winding tubes with stripes flowing along them.
//
// Each tile of the grid holds two quarter circles of radius one half, round
// two opposite corners, and a hash of the tile's position picks which pair.
// Every arc ends at the middle of an edge, where the next tile's arc starts,
// so the arcs join into unbroken curves that wander across the whole floor.
//
// The curves cut the floor into regions that take two colors, no two of the
// same color touching. Color the grid's corners like a checkerboard, by the
// parity of x + y. A tile's two arcs are round opposite corners, which share
// a parity, and the region between them holds the other two, so a pixel
// inside an arc takes its corner's color and any other pixel the opposite.
//
// The stripes flow the same way along every curve. Each curve has one color
// on its left and the other on its right, so it flows with color A on its
// right. On an arc that is clockwise round the corner when the corner is A,
// and anticlockwise when it is B, and the stripes are
//
//   s = ±TRUCHET_STRIPES u − phase
//
// with u the angle across the quarter, from 0 to 1. A whole number of stripes
// fits each quarter, so they meet where the arcs join.
//
// The tubes are shaded as cylinders and outlined in black, and the floor
// darkens toward them as if in their shadow.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TILE_SIZE 32.0 // pixels across a tile, halfway through the zoom
#define TILE_ZOOM 12.0 // pixels either side of TILE_SIZE
#define ZOOM_SPEED 0.35 // radians a second
#define TURN_ANGLE 0.5 // radians either way the floor turns
#define TURN_SPEED 0.21
#define SCROLL_SPEED 0.9 // tiles a second
#define SCROLL_SWAY 3.0 // tiles either side the path sways
#define SWAY_SPEED 0.13
#define TUBE_WIDTH 0.2 // of a tile
#define TUBE_OUTLINE 1.0 // pixels
#define TRUCHET_SHADOW 0.15 // of a tile, how far the floor darkens beside a tube
#define TRUCHET_STRIPES 2 // per quarter circle
#define FLOW_SPEED 1.2 // stripes a second
#define SHADES 32 // palette entries per ramp
#define FLOOR 1 // palette ranges, two floor colors then two stripe colors; entry 0 is the outline
#define TUBE (FLOOR + 2 * SHADES)

void DEMO_Render(double time, double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Calculate camera
	double zoomphase = fmod(time * ZOOM_SPEED, 2 * M_PI);
	double turnphase = fmod(time * TURN_SPEED, 2 * M_PI);
	double swayphase = fmod(time * SWAY_SPEED, 2 * M_PI);
	double phase = fmod(time * FLOW_SPEED, 1.0);
	double size = TILE_SIZE + TILE_ZOOM * sin(zoomphase);
	double angle = TURN_ANGLE * sin(turnphase);
	double camerax = time * SCROLL_SPEED;
	double cameray = SCROLL_SWAY * sin(swayphase);
	double stepx = cos(angle) / size, stepy = sin(angle) / size;
	double outline = TUBE_OUTLINE / size;

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// Find the tile and the corner of its nearer arc
			int px = x - RETRO_WIDTH / 2, py = y - RETRO_HEIGHT / 2;
			double wx = camerax + px * stepx - py * stepy;
			double wy = cameray + px * stepy + py * stepx;
			int tilex = (int)floor(wx), tiley = (int)floor(wy);
			double fx = wx - tilex, fy = wy - tiley;
			int cornerx, cornery;
			if (RETRO_Hash(tilex, tiley) & 1) {
				// Arcs round the top right and bottom left corners
				cornerx = fx > fy;
				cornery = 1 - cornerx;
			} else {
				// Round the top left and bottom right
				cornerx = cornery = fx + fy > 1;
			}
			double vx = fx - cornerx, vy = fy - cornery;
			double r = sqrt(vx * vx + vy * vy);
			int parity = (tilex + cornerx + tiley + cornery) & 1;
			double off = fabs(r - 0.5);

			unsigned char *pixel = buffer + y * RETRO_WIDTH + x;
			if (off < TUBE_WIDTH / 2) {
				// Tube, a cylinder with stripes flowing along it
				double across = off / (TUBE_WIDTH / 2);
				double a = atan2(vy, vx) / (M_PI / 2);
				double u = a - floor(a);
				double s = (parity ? -1 : 1) * TRUCHET_STRIPES * u - phase;
				int stripe = s - floor(s) < 0.5;
				*pixel = TUBE + stripe * SHADES + CLAMP(sqrt(1 - across * across) * SHADES, 0, SHADES);
			} else if (off < TUBE_WIDTH / 2 + outline) {
				*pixel = 0;
			} else {
				// Floor, its color by the side of the arc, in the tube's shadow near it
				int color = r < 0.5 ? parity : 1 - parity;
				double light = 0.55 + 0.45 * MIN((off - TUBE_WIDTH / 2) / TRUCHET_SHADOW, 1.0);
				*pixel = FLOOR + color * SHADES + CLAMP(light * SHADES, 0, SHADES);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette. The two floor colors and the two stripe colors, each
	// a ramp from dark to full
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(FLOOR, FLOOR + SHADES, RETRO_BLACK, RETRO_OCEANBLUE);
	RETRO_CreateGradientPalette(FLOOR + SHADES, FLOOR + 2 * SHADES, RETRO_BLACK, RETRO_TEAL);
	RETRO_CreateGradientPalette(TUBE, TUBE + SHADES * 3 / 4, RETRO_SCORCHED, RETRO_ORANGE);
	RETRO_CreateGradientPalette(TUBE + SHADES * 3 / 4, TUBE + SHADES, RETRO_ORANGE, RETRO_JASMINE);
	RETRO_CreateGradientPalette(TUBE + SHADES, TUBE + SHADES * 7 / 4, RETRO_MUDGOLD, RETRO_GOLD);
	RETRO_CreateGradientPalette(TUBE + SHADES * 7 / 4, TUBE + 2 * SHADES, RETRO_GOLD, RETRO_CREAM);
}
