//
// Dot scroller, over a cruising landscape
//
// A dot scroller over the 128x128 voxel_height_128x128.pcx height field,
// tiled without limit and painted with its paired voxel_color_128x128.pcx
// colormap. The camera cruises and turns automatically. The text strip
// travels with the camera, scrolling sideways while each letter dot follows
// the terrain beneath it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retropoly.h"
#include "lib/retroterrain.h"

constexpr RETRO_FontAsset FONT = { "assets/font_16x16.pcx", 16, 16 };
//const RETRO_FontData &FONT = RETRO_FONT_MINECRAFT_8X8;

// voxel_color_128x128.pcx never uses palette indices 224 upward, so the
// letter gradient can occupy them without stealing a terrain color.
constexpr int LETTER_COLOR_BASE = 224;
constexpr float LETTER_DOT_SPACING = 0.8f;
constexpr float LETTER_ROW_SPACING = 1.0f;
constexpr float LETTER_HEIGHT_OFFSET = 1.5f;
constexpr float SCROLL_SPEED = 17.0f;
constexpr float CAMERA_HEIGHT = 20.0f;
constexpr float PITCH = -0.5f;
constexpr float NEAR_PLANE = 1.0f;
constexpr float VIEW_DISTANCE = 100.0f;
constexpr float WORLD_HEIGHT_SCALE = 1.0f / 27.0f;
constexpr float LETTER_NEAR_FORWARD = 24.0f;
constexpr float TURN_SPEED = 0.3f;
constexpr float FORWARD_SPEED = 30.0f;

// Looking along +z with x to the right and y up, tipped down by -PITCH: the
// frame the dots were always seen in. Half the screen's width and height
// across a unit of depth. It turns right by the heading as it cruises.
static RETRO_Camera View;

static const char ScrollText[] = " RETRO DEMOEFFECTS...";
static RETRO_Image *ScrollImage;

// Terrain and letters use the same perspective and pixel depth buffer.
static void PlotDot(vec3 p, unsigned char color)
{
	vec3 eye = RETRO_ViewPoint(&View, p);
	if (eye.z > VIEW_DISTANCE || eye.z < View.lens.nearplane) return;
	PolygonPoint point = RETRO_ProjectViewPoint(View.lens, eye);
	int x = (int)floorf(point.pos.x);
	int y = (int)floorf(point.pos.y);
	if (!RETRO_LensViewContains(View.lens, x, y)) return;
	if (RETRO_DepthTest(y * RETRO_WIDTH + x, point.q)) RETRO_PutPixel(x, y, color);
}

// Scan the view radius around the camera and plot every terrain cell in it.
static void DrawTerrainDots(float camerax, float cameraz)
{
	int minx = (int)floorf(camerax - VIEW_DISTANCE);
	int maxx = (int)ceilf(camerax + VIEW_DISTANCE);
	int minz = (int)floorf(cameraz - VIEW_DISTANCE);
	int maxz = (int)ceilf(cameraz + VIEW_DISTANCE);
	for (int z = minz; z <= maxz; z++) {
		for (int x = minx; x <= maxx; x++) {
			PlotDot({ (float)x, RETRO_TerrainHeight(x, z), (float)z }, RETRO_TerrainColor(x, z));
		}
	}
}

// Scroll the text strip sideways with the camera, following the terrain
// beneath each letter dot.
static void DrawScrollerDots(double time, float camerax, float cameraz, float cs, float sn)
{
	// The farthest row's own depth: at that depth the true screen edge sits at
	// side = +-farforward, wider than any nearer row's. Timing entry/exit to
	// this bound, rather than a smaller guess, means no row is ever still
	// waiting on a phantom margin PlotDot would already have put on screen.
	float farforward = LETTER_NEAR_FORWARD + (ScrollImage->height - 1) * LETTER_ROW_SPACING;
	float cycle = 2 * farforward + ScrollImage->width * LETTER_DOT_SPACING;
	float phase = fmod(time * SCROLL_SPEED, cycle);
	for (int row = 0; row < ScrollImage->height; row++) {
		// Top rows lie farther away, keeping the font upright in perspective.
		float forward = LETTER_NEAR_FORWARD + (ScrollImage->height - 1 - row) * LETTER_ROW_SPACING;
		for (int column = 0; column < ScrollImage->width; column++) {
			if (!ScrollImage->data[row * ScrollImage->width + column]) continue;
			float side = farforward + column * LETTER_DOT_SPACING - phase;
			if (side < -farforward || side > farforward) continue;
			float x = camerax + side * cs + forward * sn;
			float z = cameraz - side * sn + forward * cs;
			PlotDot({ x, RETRO_TerrainHeightLinear(x, z) + LETTER_HEIGHT_OFFSET, z }, LETTER_COLOR_BASE + row);
		}
	}
}

void DEMO_Render(RETRO_Time time)
{
	static float camerax = 64;
	static float cameraz = 118;
	static float heading = 0;
	heading = fmodf(heading + TURN_SPEED * time.delta, 2.0f * M_PI);
	float cs = cosf(heading);
	float sn = sinf(heading);
	camerax = fmodf(camerax + sn * FORWARD_SPEED * time.delta + RETRO_Terrain.width, RETRO_Terrain.width);
	cameraz = fmodf(cameraz + cs * FORWARD_SPEED * time.delta + RETRO_Terrain.height, RETRO_Terrain.height);

	View.pos = { camerax, CAMERA_HEIGHT, cameraz };
	RETRO_AimCamera(&View, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, heading, -PITCH);
	RETRO_ClearDepthBuffer();
	DrawTerrainDots(camerax, cameraz);
	DrawScrollerDots(time.total, camerax, cameraz, cs, sn);
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_128x128.pcx", "assets/voxel_height_128x128.pcx", WORLD_HEIGHT_SCALE);
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText);
	if (ScrollImage->height > RETRO_COLORS - LETTER_COLOR_BASE) {
		RETRO_RageQuit("Scroller font is too tall for the letter palette\n");
	}

	RETRO_CreateGradientPalette(LETTER_COLOR_BASE, LETTER_COLOR_BASE + ScrollImage->height, RETRO_GOLD, RETRO_WHITE);
	RETRO_SetColor(0, RETRO_NIGHTSKY);

	View.lens.focalx = RETRO_WIDTH * 0.5f;
	View.lens.focaly = RETRO_HEIGHT * 0.5f;
	View.lens.nearplane = NEAR_PLANE;
}
