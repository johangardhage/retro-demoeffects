//
// Dot world scroller
//
// The 128x128 voxel_height_128x128.pcx landscape in a zoomed-out island view,
// with a scroller (see FONT below) packed into one strip at startup. Every
// lit texel is a world-space point on the rotating island,
//
//   mapx = MAP_WIDTH + column · LETTER_DOT_SPACING − phase
//   mapz = LETTER_BASE_Z + row · LETTER_ROW_SPACING
//   worldy = height(mapx, mapz) + LETTER_HEIGHT_OFFSET
//
// so the letters follow hills and valleys. phase lives on
// MAP_WIDTH + stripwidth · LETTER_DOT_SPACING, in world units per second.
// A zero texel is a gap, never a colour: the strip's own palette is unused.
//
// The camera stands outside the finite patch, pitched down so the island
// fills the frame; the patch turns about its own centre under it rather than
// the camera turning. Left and Right rotate the terrain and text. Up/W and
// Down/S dolly the camera forward and backward, held between stops that keep
// the patch in frame.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"

#define MAP_WIDTH 128
#define MAP_HEIGHT 128
#define WORLD_HEIGHT_SCALE (1.0f / 8.0f) // Same relief as dotscroller3
#define CENTER_X ((MAP_WIDTH - 1) * 0.5f)
#define CENTER_Z ((MAP_HEIGHT - 1) * 0.5f)

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define LETTER_COLOR_BASE 224
#define LETTER_DOT_SPACING 1.35f
#define LETTER_ROW_SPACING 1.35f
#define LETTER_BASE_Z 54.4f
#define LETTER_HEIGHT_OFFSET 2.5f
#define SCROLL_SPEED 34.0f // world units per second

#define ISLAND_PITCH 0.70f // Radians down from the horizon
#define ISLAND_START_Z (MAP_HEIGHT + 35.0f)
#define ISLAND_START_ROTATION -0.5f // A modest turn off dead-on at startup
#define ISLAND_MOVE_SPEED 24.0f
#define ISLAND_TURN_SPEED 1.35f
#define NEAR_PLANE 4.0f
#define FOCAL_X (RETRO_WIDTH * 0.54f)
#define FOCAL_Y (RETRO_HEIGHT * 0.78f)
#define HORIZON_Y (RETRO_HEIGHT * 0.46f)

static const char *const ScrollText[] = { " RETRO DEMOEFFECTS...    " };
static RETRO_Image *HeightMap, *ColorMap, *ScrollImage;

// The camera's dolly stops and the height its pitch was posed at, all fixed
// once the map is loaded: the patch turns and the camera dollies, but the
// pose that keeps the view centred on it never moves.
static float IslandHeight, IslandNearestZ, IslandFarthestZ;

static unsigned char TerrainSample(int x, int z)
{
	return HeightMap->data[CLAMP(z, 0, MAP_HEIGHT) * MAP_WIDTH + CLAMP(x, 0, MAP_WIDTH)];
}

static unsigned char ColorSample(int x, int z)
{
	return ColorMap->data[CLAMP(z, 0, MAP_HEIGHT) * MAP_WIDTH + CLAMP(x, 0, MAP_WIDTH)];
}

static float TerrainHeight(float x, float z)
{
	int ix = (int)floorf(x), iz = (int)floorf(z);
	float fx = x - ix, fz = z - iz;
	float top = TerrainSample(ix, iz) * (1 - fx) + TerrainSample(ix + 1, iz) * fx;
	float bottom = TerrainSample(ix, iz + 1) * (1 - fx) + TerrainSample(ix + 1, iz + 1) * fx;
	return (top * (1 - fz) + bottom * fz) * WORLD_HEIGHT_SCALE;
}

// Terrain and letters use the same perspective and pixel depth buffer. side
// and forward are the map point's offset from the camera, already turned by
// the patch's own rotation; PlotDot only has to mix in the fixed pitch.
static void PlotDot(float side, float forward, float height, unsigned char color)
{
	float vertical = height - IslandHeight;
	float depth = forward * cosf(ISLAND_PITCH) - vertical * sinf(ISLAND_PITCH);
	if (depth <= NEAR_PLANE) return;
	float up = vertical * cosf(ISLAND_PITCH) + forward * sinf(ISLAND_PITCH);
	float sx = RETRO_WIDTH * 0.5f + FOCAL_X * side / depth;
	float sy = HORIZON_Y - FOCAL_Y * up / depth;
	if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) return;
	int x = (int)sx, y = (int)sy;
	if (RETRO_DepthTest(y * RETRO_WIDTH + x, 1.0f / depth)) {
		RETRO_PutPixel(x, y, color);
	}
}

void DEMO_Render(double time, double deltatime)
{
	static float IslandZ = ISLAND_START_Z;
	static float IslandRotation = ISLAND_START_ROTATION;

	float distance = deltatime * ISLAND_MOVE_SPEED;
	float rotation = deltatime * ISLAND_TURN_SPEED;
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) IslandRotation += rotation;
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) IslandRotation -= rotation;
	if (RETRO_KeyState(SDL_SCANCODE_UP) || RETRO_KeyState(SDL_SCANCODE_W)) IslandZ -= distance;
	if (RETRO_KeyState(SDL_SCANCODE_DOWN) || RETRO_KeyState(SDL_SCANCODE_S)) IslandZ += distance;
	if (IslandZ < IslandNearestZ) IslandZ = IslandNearestZ;
	if (IslandZ > IslandFarthestZ) IslandZ = IslandFarthestZ;
	IslandRotation = fmodf(IslandRotation, (float)(2.0 * M_PI));

	RETRO_ClearDepthBuffer();
	float rotcos = cosf(IslandRotation), rotsin = sinf(IslandRotation);
	float forward0 = IslandZ - CENTER_Z;

	for (int z = 0; z < MAP_HEIGHT; z++) {
		for (int x = 0; x < MAP_WIDTH; x++) {
			float dx = x - CENTER_X, dz = z - CENTER_Z;
			PlotDot(dx * rotcos - dz * rotsin, forward0 - (dx * rotsin + dz * rotcos), TerrainSample(x, z) * WORLD_HEIGHT_SCALE, ColorSample(x, z));
		}
	}

	// The camera looks toward decreasing Z, so the font's top row uses the
	// smaller (farther) coordinate and the text reads upright on the ground.
	float scrollcycle = MAP_WIDTH + ScrollImage->width * LETTER_DOT_SPACING;
	float phase = fmod(time * SCROLL_SPEED, scrollcycle);
	for (int sy = 0; sy < ScrollImage->height; sy++) {
		float mapz = LETTER_BASE_Z + sy * LETTER_ROW_SPACING;
		for (int sx = 0; sx < ScrollImage->width; sx++) {
			if (ScrollImage->data[sy * ScrollImage->width + sx] == 0) continue;
			float mapx = MAP_WIDTH + sx * LETTER_DOT_SPACING - phase;
			if (mapx < 0 || mapx >= MAP_WIDTH) continue;
			float dx = mapx - CENTER_X, dz = mapz - CENTER_Z;
			float height = TerrainHeight(mapx, mapz) + LETTER_HEIGHT_OFFSET;
			PlotDot(dx * rotcos - dz * rotsin, forward0 - (dx * rotsin + dz * rotcos), height, LETTER_COLOR_BASE + sy);
		}
	}
}

void DEMO_Initialize(void)
{
	HeightMap = RETRO_LoadImage("assets/voxel_height_128x128.pcx");
	ColorMap = RETRO_LoadImage("assets/voxel_color_128x128.pcx", true);
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	if (ScrollImage->height > 256 - LETTER_COLOR_BASE) {
		RETRO_RageQuit("Scroller font is too tall for the letter palette\n");
	}

	RETRO_CreateGradientPalette(LETTER_COLOR_BASE, LETTER_COLOR_BASE + ScrollImage->height, RETRO_GOLD, RETRO_WHITE);
	RETRO_SetColor(0, RETRO_NIGHTSKY);

	// The near stop is the centre plus the circumradius of the patch plus the
	// near plane, not the unrotated south edge: the island turns about its
	// centre, and a stop at the south edge would let a 45 degree yaw put the
	// camera inside it looking at a corner.
	IslandNearestZ = CENTER_Z + hypotf(CENTER_X, CENTER_Z) + NEAR_PLANE;
	IslandFarthestZ = MAP_HEIGHT + 70.0f;
	IslandHeight = TerrainSample((int)CENTER_X, (int)CENTER_Z) * WORLD_HEIGHT_SCALE + tanf(ISLAND_PITCH) * (ISLAND_START_Z - CENTER_Z);
}
