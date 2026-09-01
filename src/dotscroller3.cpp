//
// Dot world scroller
//
// The same 128x128 rotating landscape as dotlandscape.cpp, with a 16x16
// scroller travelling across its surface. Every lit font texel is a
// world-space point whose height is sampled independently from the terrain,
// making the letters follow hills and valleys.
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
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retroterrain.h"

#define MAP_WIDTH 128
#define MAP_HEIGHT 128
#define WORLD_HEIGHT_SCALE 0.34f

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define FONT_IMAGE_WIDTH 944
#define LETTER_COLOR_BASE 224
#define SCROLL_TEXT " RETRO DEMOEFFECTS...    "
#define SCROLL_LENGTH (sizeof(SCROLL_TEXT) - 1)
#define SCROLL_WIDTH (FONT_WIDTH * SCROLL_LENGTH)
#define LETTER_DOT_SPACING 1.35f
#define LETTER_ROW_SPACING 1.35f
#define LETTER_BASE_Z 54.4f
#define LETTER_HEIGHT_OFFSET 2.5f
#define SCROLL_SPEED 17.0f // world units per second
#define SCROLL_CYCLE (MAP_WIDTH + SCROLL_WIDTH * LETTER_DOT_SPACING)

// HeightMap stores terrain altitude. ColorMap stores the palette index of the
// corresponding dot, so neither value has to be recalculated while rendering.
unsigned char HeightMap[MAP_WIDTH * MAP_HEIGHT];
unsigned char ColorMap[MAP_WIDTH * MAP_HEIGHT];
unsigned char ScrollBitmap[FONT_HEIGHT * SCROLL_WIDTH];

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
	float phase = fmod(time * SCROLL_SPEED, SCROLL_CYCLE);

	memset(DotWorldZBuffer, 0xFF, sizeof(DotWorldZBuffer));
	RETRO_TerrainIslandFrame frame = RETRO_BuildTerrainIslandFrame();
	DrawTerrainDots(frame);

	int width = RETRO_Terrain.width;
	for (int sy = 0; sy < FONT_HEIGHT; sy++) {
		// The camera looks toward decreasing Z, so the font's top row uses the
		// smaller (farther) coordinate and the text reads upright on the ground.
		float localz = LETTER_BASE_Z + sy * LETTER_ROW_SPACING;
		for (int sx = 0; sx < SCROLL_WIDTH; sx++) {
			if (ScrollBitmap[sy * SCROLL_WIDTH + sx] == 0) continue;
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
	// The terrain colormap does not use indices 224..239, so the lettering can
	// own this range without recoloring isolated terrain dots.
	RETRO_CreateGradientPalette(LETTER_COLOR_BASE, LETTER_COLOR_BASE + FONT_HEIGHT, RETRO_GOLD, RETRO_WHITE);

	RETRO_LoadImage("assets/font_16x16.pcx");
	unsigned char *image = RETRO_ImageData(2);
	for (int i = 0; i < (int)SCROLL_LENGTH; i++) {
		unsigned char *src = image + (SCROLL_TEXT[i] - 32) * FONT_WIDTH;
		unsigned char *dst = ScrollBitmap + i * FONT_WIDTH;
		for (int y = 0; y < FONT_HEIGHT; y++) {
			for (int x = 0; x < FONT_WIDTH; x++) {
				dst[y * SCROLL_WIDTH + x] = src[y * FONT_IMAGE_WIDTH + x];
			}
		}
	}
}
