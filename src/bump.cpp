//
// Bump mapping
//
// A height map lit by a precomputed light map. The light map L(u, v) is the
// brightness of a cone of radius LIGHT_SIZE:
//
//   L(u, v) = max(0, 1 - |(u, v)| / LIGHT_SIZE)
//
// Pixel (x, y) looks up L at the offset from the light, displaced by the
// central-difference slope of the height field:
//
//   dh/dx = (h(x+1) - h(x-1)) * LIGHT_DEPTH
//   lookup = (lx - x + dh/dx,  ly - y + dh/dy)
//
// The difference spans two pixels, so the relief reads as deep as 2*LIGHT_DEPTH.
// The lookup is brightest where the slope runs against the light, which is
// where a Lambert term peaks as well. The map is dark on its border, so a
// lookup that leaves it is clamped to no light rather than wrapping. The
// light rides a 1:2 Lissajous; angle lives on 360°.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define LIGHT_SIZE 128 // radius of the light in pixels, which is also the center of the light map
#define LIGHTMAP_WIDTH (LIGHT_SIZE * 2 + 1) // odd, so the peak lands on an exact center pixel
#define LIGHTMAP_HEIGHT (LIGHT_SIZE * 2 + 1)
#define LIGHT_DEPTH 1 // scales the slope of the height map, so how deep the relief reads
#define LIGHT_COLORS 128 // palette entries the light map ramps over
#define LIGHT_ORBIT 128 // radius of the light's path across the screen, in pixels
#define LIGHT_SPEED 100 // degrees of that path per second

unsigned char LightMap[LIGHTMAP_HEIGHT * LIGHTMAP_WIDTH];

// The height map never changes, so neither do its slopes
int SlopeX[RETRO_HEIGHT * RETRO_WIDTH];
int SlopeY[RETRO_HEIGHT * RETRO_WIDTH];

void DEMO_Render(double deltatime)
{
	unsigned char *buffer = RETRO_FrameBuffer();

	// Calculate light. Lissajous: twice around vertically for every turn horizontally.
	static double angle = 0;
	angle = fmod(angle + deltatime * LIGHT_SPEED, RETRO_DEGREES_PER_TURN);

	int lx = RETRO_WIDTH / 2 + LIGHT_ORBIT * cos(angle * DEG2RAD);
	int ly = RETRO_HEIGHT / 2 + LIGHT_ORBIT * sin(2 * angle * DEG2RAD);

	int maporiginx = lx + LIGHT_SIZE;
	int maporiginy = ly + LIGHT_SIZE;

	// Draw bump
	for (int y = 1; y < RETRO_HEIGHT-1; y++) {
		for (int x = 1; x < RETRO_WIDTH-1; x++) {
			int offset = y * RETRO_WIDTH + x;

			int mapx = maporiginx - x + SlopeX[offset];
			int mapy = maporiginy - y + SlopeY[offset];

			mapx = CLAMP(mapx, 0, LIGHTMAP_WIDTH);
			mapy = CLAMP(mapy, 0, LIGHTMAP_HEIGHT);

			buffer[offset] = LightMap[mapy * LIGHTMAP_WIDTH + mapx];
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/bump_320x240.pcx");

	// Init slopes. The picture never changes, so neither do they.
	unsigned char *image = RETRO_ImageData();
	for (int y = 1; y < RETRO_HEIGHT - 1; y++) {
		for (int x = 1; x < RETRO_WIDTH - 1; x++) {
			int offset = y * RETRO_WIDTH + x;
			SlopeX[offset] = (image[offset + 1] - image[offset - 1]) * LIGHT_DEPTH;
			SlopeY[offset] = (image[offset + RETRO_WIDTH] - image[offset - RETRO_WIDTH]) * LIGHT_DEPTH;
		}
	}

	// Init palette
	RETRO_CreateGradientPalette(0, LIGHT_COLORS * 3 / 4, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(LIGHT_COLORS * 3 / 4, LIGHT_COLORS, RETRO_RED, RETRO_WHITE);

	// Init light map. Offset measured in units of LIGHT_SIZE, so the renderer lights a pixel by
	// looking up its offset. This is a cone, not a Lambert term: reading the
	// offset as the tilt of a unit normal would give sqrt(1 - r^2), which is
	// brighter everywhere between the centre and the rim.
	for (int y = 0; y < LIGHTMAP_HEIGHT; y++) {
		for (int x = 0; x < LIGHTMAP_WIDTH; x++) {
			float offsetx = (x - LIGHT_SIZE) / (float)LIGHT_SIZE;
			float offsety = (y - LIGHT_SIZE) / (float)LIGHT_SIZE;
			float distance = sqrt(offsetx * offsetx + offsety * offsety);

			float intensity = distance < 1 ? 1 - distance : 0;

			LightMap[y * LIGHTMAP_WIDTH + x] = (LIGHT_COLORS - 1) * intensity;
		}
	}
}
