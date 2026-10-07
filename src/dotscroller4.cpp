//
// Dot world scroller
//
// The 128x128 voxel_height_128x128.pcx landscape in a zoomed-out island view,
// with a scroller (see FONT below) packed into one strip at startup. Every
// lit texel is a world-space point on the rotating island,
//
//   mapx = mapwidth + column · LETTER_DOT_SPACING − phase
//   mapz = LETTER_BASE_Z + row · LETTER_ROW_SPACING
//   worldy = height(mapx, mapz) + LETTER_HEIGHT_OFFSET
//
// so the letters follow hills and valleys. phase lives on
// mapwidth + stripwidth · LETTER_DOT_SPACING, in world units per second.
// A zero texel is a gap, never a color: the strip's own palette is unused.
//
// The island look is the terrain library's: the camera stands outside the
// finite patch, pitched down so the island fills the frame, and the patch
// turns about its own center under it rather than the camera turning. Left
// and Right rotate the terrain and text. Up/W and Down/S dolly the camera
// forward and backward, held between stops that keep the patch in frame.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retropoly.h"
#include "lib/retroterrain.h"

#define WORLD_HEIGHT_SCALE (1.0f / 8.0f) // Same relief as dotlandscape2
#define ISLAND_START_ROTATION -0.5f // A modest turn off dead-on at startup

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define LETTER_COLOR_BASE 224
#define LETTER_DOT_SPACING 1.35f
#define LETTER_ROW_SPACING 1.35f
#define LETTER_BASE_Z 54.4f
#define LETTER_HEIGHT_OFFSET 2.5f
#define SCROLL_SPEED 34.0f // world units per second

static const char *const ScrollText[] = { " RETRO DEMOEFFECTS...    " };
static RETRO_Image *ScrollImage;

// Terrain and letters use the same perspective and pixel depth buffer.
static void PlotDot(float x, float y, float z, const RETRO_TerrainIslandFrame &frame, unsigned char color)
{
	vec3 eye = RETRO_TerrainIslandEye(x, y, z, frame);
	if (eye.z <= RETRO_TerrainLens.nearplane) return;

	PolygonPoint point = RETRO_ProjectViewPoint(RETRO_TerrainLens, eye);
	if (point.pos.x < 0 || point.pos.x >= RETRO_WIDTH || point.pos.y < 0 || point.pos.y >= RETRO_HEIGHT) return;

	int sx = (int)point.pos.x;
	int sy = (int)point.pos.y;
	if (RETRO_DepthTest(sy * RETRO_WIDTH + sx, point.q)) {
		RETRO_PutPixel(sx, sy, color);
	}
}

void DEMO_Render(RETRO_Time time)
{
	int mapwidth = RETRO_Terrain.width;
	int mapheight = RETRO_Terrain.height;

	RETRO_UpdateTerrainIsland(time.delta);
	RETRO_ClearDepthBuffer();
	RETRO_TerrainIslandFrame frame = RETRO_BuildTerrainIslandFrame();

	for (int z = 0; z < mapheight; z++) {
		for (int x = 0; x < mapwidth; x++) {
			PlotDot(x, RETRO_TerrainHeight(x, z), z, frame, RETRO_TerrainColor(x, z));
		}
	}

	// The camera looks toward decreasing Z, so the font's top row uses the
	// smaller (farther) coordinate and the text reads upright on the ground.
	float scrollcycle = mapwidth + ScrollImage->width * LETTER_DOT_SPACING;
	float phase = fmod(time.total * SCROLL_SPEED, scrollcycle);
	for (int sy = 0; sy < ScrollImage->height; sy++) {
		float mapz = LETTER_BASE_Z + sy * LETTER_ROW_SPACING;
		for (int sx = 0; sx < ScrollImage->width; sx++) {
			if (ScrollImage->data[sy * ScrollImage->width + sx] == 0) continue;
			float mapx = mapwidth + sx * LETTER_DOT_SPACING - phase;
			if (mapx < 0 || mapx >= mapwidth) continue;
			float height = RETRO_TerrainHeightLinear(mapx, mapz) + LETTER_HEIGHT_OFFSET;
			PlotDot(mapx, height, mapz, frame, LETTER_COLOR_BASE + sy);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_128x128.pcx", "assets/voxel_height_128x128.pcx", WORLD_HEIGHT_SCALE, false);
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	if (ScrollImage->height > RETRO_COLORS - LETTER_COLOR_BASE) {
		RETRO_RageQuit("Scroller font is too tall for the letter palette\n");
	}

	RETRO_CreateGradientPalette(LETTER_COLOR_BASE, LETTER_COLOR_BASE + ScrollImage->height, RETRO_GOLD, RETRO_WHITE);
	RETRO_SetColor(0, RETRO_NIGHTSKY);

	RETRO_LookDownAtTerrain();
	RETRO_Island.rotation = ISLAND_START_ROTATION;
}
