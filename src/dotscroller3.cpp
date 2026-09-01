//
// Dot world scroller
//
// The same 128x128 rotating landscape as dotlandscape.cpp, with a scroller
// (see FONT below) packed into one strip at startup. Every lit texel is a
// world-space point on the rotating island,
//
//   localx = MAP_WIDTH + column · LETTER_DOT_SPACING − phase
//   localz = LETTER_BASE_Z + row · LETTER_ROW_SPACING
//   worldy = height(localx, localz) + LETTER_HEIGHT_OFFSET
//
// so the letters follow hills and valleys. phase lives on
// MAP_WIDTH + stripwidth · LETTER_DOT_SPACING, in world units per second.
// A zero texel is a gap, never a colour: the strip's own palette is unused.
//
// The island look is the terrain library's: the camera is pitched down so
// the island fills the frame, the patch turns about its centre, and the
// camera dollies along the viewing axis between stops that keep the finite
// patch in view.
// Left and Right rotate the terrain and text. Up/W and Down/S move the
// camera forward and backward.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroterrain.h"

#define MAP_WIDTH 128
#define MAP_HEIGHT 128
#define WORLD_HEIGHT_SCALE 0.34f

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define LETTER_COLOR_BASE 224
#define LETTER_DOT_SPACING 1.35f
#define LETTER_ROW_SPACING 1.35f
#define LETTER_BASE_Z 54.4f
#define LETTER_HEIGHT_OFFSET 2.5f
#define SCROLL_SPEED 17.0f // world units per second

static const char *const ScrollText[] = { " RETRO DEMOEFFECTS...    " };

// HeightMap stores terrain altitude. ColorMap stores the palette index of the
// corresponding dot, so neither value has to be recalculated while rendering.
unsigned char HeightMap[MAP_WIDTH * MAP_HEIGHT];
unsigned char ColorMap[MAP_WIDTH * MAP_HEIGHT];
RETRO_Image *ScrollImage;

// More than one terrain or letter dot may land on the same screen pixel. The
// depth buffer ensures that the nearest one remains visible.
unsigned int DotWorldZBuffer[RETRO_WIDTH * RETRO_HEIGHT];

static void PlotWorldDot(float x, float y, float z, unsigned char color, const RETRO_TerrainIslandFrame &frame)
{
	RETRO_TerrainEye eye = RETRO_TerrainIslandEye(x, y, z, frame);
	if (eye.depth <= RETRO_TerrainView.nearplane || fabsf(eye.side) > eye.depth * RETRO_TerrainViewCullSlope()) return;

	RETRO_TerrainPoint point = RETRO_ProjectTerrainView(eye);
	int sx = (int)point.sx;
	int sy = (int)point.sy;
	if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) return;

	unsigned int idepth = (unsigned int)(eye.depth * 256.0f);
	int screenindex = sy * RETRO_WIDTH + sx;
	if (idepth < DotWorldZBuffer[screenindex]) {
		DotWorldZBuffer[screenindex] = idepth;
		RETRO_PutPixel(sx, sy, color);
	}
}

// Draw the finite 128x128 terrain through the island look.
static void DrawTerrainDots(const RETRO_TerrainIslandFrame &frame)
{
	int width = RETRO_Terrain.width;
	int height = RETRO_Terrain.height;

	for (int z = 0; z < height; z++) {
		for (int x = 0; x < width; x++) {
			PlotWorldDot(x, RETRO_TerrainHeight(x, z), z, RETRO_TerrainColor(x, z), frame);
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	RETRO_UpdateTerrainIsland(deltatime);

	// Calculate phase
	float scrollcycle = MAP_WIDTH + ScrollImage->width * LETTER_DOT_SPACING;
	float phase = fmod(time * SCROLL_SPEED, scrollcycle);

	memset(DotWorldZBuffer, 0xFF, sizeof(DotWorldZBuffer));
	RETRO_TerrainIslandFrame frame = RETRO_BuildTerrainIslandFrame();
	DrawTerrainDots(frame);

	int width = RETRO_Terrain.width;
	for (int sy = 0; sy < ScrollImage->height; sy++) {
		// The camera looks toward decreasing Z, so the font's top row uses the
		// smaller (farther) coordinate and the text reads upright on the ground.
		float localz = LETTER_BASE_Z + sy * LETTER_ROW_SPACING;
		for (int sx = 0; sx < ScrollImage->width; sx++) {
			if (ScrollImage->data[sy * ScrollImage->width + sx] == 0) continue;
			float localx = width + sx * LETTER_DOT_SPACING - phase;
			if (localx < 0 || localx >= width) continue;
			float worldy = RETRO_TerrainHeightLinear(localx, localz) + LETTER_HEIGHT_OFFSET;
			PlotWorldDot(localx, worldy, localz, LETTER_COLOR_BASE + sy, frame);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");
	RETRO_SetColor(0, RETRO_NIGHTSKY);
	RETRO_DownsampleTerrain(HeightMap, ColorMap, MAP_WIDTH, MAP_HEIGHT);
	RETRO_SetTerrain(MAP_WIDTH, MAP_HEIGHT, WORLD_HEIGHT_SCALE, HeightMap, ColorMap, false);
	RETRO_LookDownAtTerrain();

	// Font pixels are only used as a lit/unlit mask, never as color, so the
	// strip's own palette is never applied here.
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));

	// The terrain colormap does not use indices 224..239, so the lettering can
	// own this range without recoloring isolated terrain dots.
	RETRO_CreateGradientPalette(LETTER_COLOR_BASE, LETTER_COLOR_BASE + ScrollImage->height, RETRO_GOLD, RETRO_WHITE);
}
