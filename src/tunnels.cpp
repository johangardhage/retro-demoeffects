//
// Two tunnels
//
// Two tunnels down the same tube, one flying in and one flying out, added
// together in the palette. Both are read from one 16-color texture,
// tunnels_256x256.pcx, its rows around the tube and its columns along it.
// The near layer takes the texel as the low nibble of the color, the far
// layer takes it as the high nibble, and the picture's palette holds the
// 16 × 16 blends, so the sum of the two is the color itself.
//
// A pixel at radius r from the screen center is at depth DEPTH_SCALE /
// (r + DEPTH_OFFSET) along the tube, and at its angle around it, 256 to a
// turn. Both wrap around the texture on their own, so a layer scrolls
// without seams. The near layer flies in a texel each step and turns half
// a row; the far layer flies out two texels and turns a row.
//
// Both layers step TICKS_PER_SECOND times a second, without end, fading in
// over the first FADE_TICKS.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

constexpr double DEPTH_SCALE = 5120.0;
constexpr double DEPTH_OFFSET = 18.0; // keeps the depth finite at the center, so the tube has no hole

constexpr double TICKS_PER_SECOND = 60.0;
constexpr int FADE_TICKS = 64;

static unsigned char DepthTable[RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char AngleTable[RETRO_WIDTH * RETRO_HEIGHT];

// The texel a pixel shows with its layer moved depth texels along and angle
// rows around
static int Texel(const unsigned char *texture, int offset, int depth, int angle)
{
	return texture[(unsigned char)(AngleTable[offset] + angle) * 256 + (unsigned char)(DepthTable[offset] + depth)];
}

void DEMO_Render(RETRO_Time time)
{
	unsigned char *texture = RETRO_ImageData();
	unsigned char *buffer = RETRO_FrameBuffer();

	int tick = (int)RETRO_FramePosition(time.total, TICKS_PER_SECOND);
	RETRO_Fade((float)tick / FADE_TICKS, RETRO_ImagePalette());

	// Draw tunnels. A texel is 0 to 15, four bits, so two fit in one color
	// index: the far layer's in the high four bits, the near layer's in the
	// low four. Palette entry far * 16 + near holds the mix of the two
	// shades, so the palette does the blending and a pixel costs one shift
	// and one add. The two never overlap, so the add carries nothing over.
	for (int offset = 0; offset < RETRO_WIDTH * RETRO_HEIGHT; offset++) {
		int nearcolor = Texel(texture, offset, tick, tick / 2);
		int farcolor = Texel(texture, offset, -2 * tick, -tick);
		buffer[offset] = (farcolor << 4) + nearcolor;
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/tunnels_256x256.pcx");

	// Init tables
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double dx = x + 0.5 - RETRO_WIDTH / 2.0;
			double dy = y + 0.5 - RETRO_HEIGHT / 2.0;
			DepthTable[y * RETRO_WIDTH + x] = (unsigned char)WRAP(DEPTH_SCALE / (hypot(dx, dy) + DEPTH_OFFSET), 256);
			AngleTable[y * RETRO_WIDTH + x] = (unsigned char)WRAP(atan2(dy, -dx) * 128 / M_PI, 256);
		}
	}
}
