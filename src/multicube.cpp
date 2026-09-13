//
// Multicube
//
// One cube, six renderers, the renderer chosen by screen y. A pixel of
// the cube that lands in [y0, y1) is always drawn with that band's style,
// so the splits are horizontal on the screen and cut across faces as the
// cube turns. Top to bottom:
//
//   dots (a 7×7-subdivided cube, so the band is a grid of vertices),
//   noise texture, Glenz orange, checker, wireframe, flat purple.
// Every filled band is Lambert-flat, so neighbouring faces keep a hard
// shade step all the way around the cube.
//
// Each band is a full cube draw, then the rows outside [y0, y1) are put
// back from a copy of the framebuffer taken before that draw. Depth is
// cleared per band so the draws do not fight. Euler angles live on 2π; all
// three tumble at their own speed rather than one spin plus a small wobble,
// so the cube never returns to the same attitude twice in a row.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define SCENE_SCALE 58
#define ROT_X 0.70f
#define ROT_Y 1.40f
#define ROT_Z 0.90f

#define TEXMAP 256
#define CHECK_CELL 64
#define PURPLE_SHADES 12

#define COL_BG 0
#define COL_BLACK 1
#define COL_WHITE 2
#define COL_TRAIL0 3 // trailing ghost colors, nearest (whitest) to oldest (deepest blue)
#define COL_TRAIL1 16
#define COL_TRAIL2 17
#define COL_PURPLE 4
#define COL_GRAY 18 // two steps short of the usual 16, giving COL_TRAIL1/2 a home
#define COL_GLENZ 32

// The wireframe band's trail: a fixed number of ghost cubes fanning out
// behind the leading one, each an earlier instant of the same spin and
// wobble, each of the trailing ones its own shade of blue.
#define WIRE_TRAIL_COUNT 4 // cubes drawn, the leading one plus WIRE_TRAIL_COUNT - 1 trailing ghosts
#define WIRE_TRAIL_STEP 0.015 // seconds each ghost trails the one ahead of it

// Six equal bands over the projected cube. BAND_START and BAND_HEIGHT
// size those slices to the cube's typical projected height; the first
// band still opens at row 0 and the last still runs to the bottom of
// the screen, so a corner that swings past the window is not clipped.
#define BAND_COUNT 6
#define BAND_HEIGHT 34
#define BAND_START 18

static int BandY(int i)
{
	return BAND_START + i * BAND_HEIGHT;
}

static Model3D *Cube;
static Model3D *Dots;
static unsigned char NoiseMap[TEXMAP * TEXMAP];
static unsigned char CheckMap[TEXMAP * TEXMAP];
static unsigned char ShadeTable[RETRO_TEXTURE_COLORS * RETRO_SHADES];
static unsigned char BandBackup[RETRO_WIDTH * RETRO_HEIGHT];

static void ScrambleNoise(void)
{
	for (int i = 0; i < TEXMAP * TEXMAP; i++) {
		NoiseMap[i] = (RANDOM(2) == 0) ? COL_BLACK : COL_WHITE;
	}
}

static void RestoreOutsideBand(int y0, int y1)
{
	unsigned char *fb = RETRO_FrameBuffer();
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		if (y >= y0 && y < y1) {
			continue;
		}
		memcpy(fb + y * RETRO_WIDTH, BandBackup + y * RETRO_WIDTH, RETRO_WIDTH);
	}
}

static void SetFaceColor(int c, int backc)
{
	for (int i = 0; i < Cube->faces; i++) {
		Cube->face[i].c = c;
		Cube->face[i].backc = backc;
	}
}

static void DrawBand(int y0, int y1, RETRO_POLY_TYPE poly, RETRO_POLY_SHADE shade, Model3D *model = Cube)
{
	memcpy(BandBackup, RETRO_FrameBuffer(), RETRO_WIDTH * RETRO_HEIGHT);
	RETRO_RenderModel(poly, shade, model);
	RestoreOutsideBand(y0, y1);
}

// The wireframe band, but WIRE_TRAIL_COUNT copies of Cube, each rotated to
// an earlier instant of the same spin and wobble and drawn oldest first, so
// the plain wireframe overwrite leaves the brightest, current-time copy on
// top instead of buried under the fainter ones. ax, ay, az are the current
// instant's angles, so Cube's rotation can be put back for the bands after
// this one, which all assume it is still at the current time.
static void DrawWireTrailBand(int y0, int y1, double time, float ax, float ay, float az)
{
	static const int colors[WIRE_TRAIL_COUNT] = { COL_WHITE, COL_TRAIL0, COL_TRAIL1, COL_TRAIL2 };

	memcpy(BandBackup, RETRO_FrameBuffer(), RETRO_WIDTH * RETRO_HEIGHT);
	for (int i = WIRE_TRAIL_COUNT - 1; i >= 0; i--) {
		double t = time - i * WIRE_TRAIL_STEP;
		float tax = fmod(t * ROT_X, 2 * M_PI);
		float tay = fmod(t * ROT_Y, 2 * M_PI);
		float taz = fmod(t * ROT_Z, 2 * M_PI);

		Cube->c = colors[i];
		RETRO_RotateModel(tax, tay, taz, Cube);
		RETRO_ProjectModel(SCENE_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Cube);
		RETRO_RenderModel(RETRO_POLY_WIREFRAME, RETRO_SHADE_NONE, Cube);
	}
	RestoreOutsideBand(y0, y1);

	RETRO_RotateModel(ax, ay, az, Cube);
	RETRO_ProjectModel(SCENE_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Cube);
}

void DEMO_Render(double time, double deltatime)
{
	float ax = fmod(time * ROT_X, 2 * M_PI);
	float ay = fmod(time * ROT_Y, 2 * M_PI);
	float az = fmod(time * ROT_Z, 2 * M_PI);

	ScrambleNoise();

	RETRO_RotateModel(ax, ay, az, Cube);
	RETRO_ProjectModel(SCENE_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Cube);
	RETRO_RotateModel(ax, ay, az, Dots);
	RETRO_ProjectModel(SCENE_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Dots);

	SetFaceColor(0, 0);
	DrawBand(0, BandY(1), RETRO_POLY_DOT, RETRO_SHADE_FLAT, Dots);

	Cube->c = 0;
	Cube->shades = 0;
	Cube->texmap = NoiseMap;
	Cube->shadetable = ShadeTable;
	DrawBand(BandY(1), BandY(2), RETRO_POLY_TEXTURE, RETRO_SHADE_FLAT);

	Cube->c = COL_GLENZ;
	Cube->shades = 64;
	SetFaceColor(24, 24);
	DrawBand(BandY(2), BandY(3), RETRO_POLY_GLENZ, RETRO_SHADE_FLAT);

	SetFaceColor(0, 0);
	Cube->c = 0;
	Cube->shades = 0;
	Cube->texmap = CheckMap;
	Cube->shadetable = ShadeTable;
	DrawBand(BandY(3), BandY(4), RETRO_POLY_TEXTURE, RETRO_SHADE_FLAT);

	Cube->shades = 0;
	DrawWireTrailBand(BandY(4), BandY(5), time, ax, ay, az);

	Cube->c = COL_PURPLE;
	Cube->shades = PURPLE_SHADES;
	DrawBand(BandY(5), RETRO_HEIGHT, RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(COL_BG, RETRO_BLACK);
	RETRO_SetColor(COL_BLACK, RETRO_BLACK);
	RETRO_SetColor(COL_WHITE, RETRO_WHITE);
	RETRO_SetColor(COL_TRAIL0, RETRO_RGB(0xd0e0ff));
	RETRO_SetColor(COL_TRAIL1, RETRO_RGB(0x80a0ff));
	RETRO_SetColor(COL_TRAIL2, RETRO_RGB(0x3050c0));
	RETRO_CreateGradientPalette(COL_PURPLE, COL_PURPLE + PURPLE_SHADES, RETRO_RGB(0x4020a0), RETRO_RGB(0xb090ff));
	RETRO_CreateGradientPalette(COL_GRAY, COL_GLENZ, RETRO_BLACK, RETRO_WHITE);
	// Glenz adds face shades, so a pixel covered twice walks up this ramp
	// toward white and the back face shows through as a darker overlay.
	RETRO_CreateGradientPalette(COL_GLENZ, 180, RETRO_RGB(0x501800), RETRO_RGB(0xffc028));
	RETRO_CreateGradientPalette(180, RETRO_COLORS, RETRO_RGB(0xffc028), RETRO_WHITE);

	RETRO_Palette texpal[RETRO_TEXTURE_COLORS];
	for (int i = 0; i < RETRO_TEXTURE_COLORS; i++) {
		texpal[i] = RETRO_GetColor(i);
	}
	RETRO_CreatePaletteShadeTable(texpal, RETRO_TEXTURE_COLORS, RETRO_SHADES, ShadeTable, 0.20f);

	for (int y = 0; y < TEXMAP; y++) {
		for (int x = 0; x < TEXMAP; x++) {
			int cell = (x / CHECK_CELL) + (y / CHECK_CELL);
			CheckMap[y * TEXMAP + x] = (cell & 1) ? COL_WHITE : COL_BLACK;
		}
	}

	Cube = RETRO_Load3DModel("assets/cubequads.obj");
	Cube->texmap = NoiseMap;
	Cube->texmapwidth = TEXMAP;
	Cube->texmapheight = TEXMAP;
	// cubequads has no UVs; give each face the whole map so the noise and
	// checker bands still cover it.
	Cube->uvs = 0;
	for (int i = 0; i < Cube->faces; i++) {
		Face *face = &Cube->face[i];
		float u1 = TEXMAP - 0.51f;
		float v1 = TEXMAP - 0.51f;
		int base = Cube->uvs;
		Cube->uv[Cube->uvs++] = { 0.51f, 0.51f };
		Cube->uv[Cube->uvs++] = { u1, 0.51f };
		Cube->uv[Cube->uvs++] = { u1, v1 };
		Cube->uv[Cube->uvs++] = { 0.51f, v1 };
		for (int j = 0; j < face->vertices; j++) {
			face->uv[j] = base + j;
		}
	}

	Dots = RETRO_Load3DModel("assets/rubbercubequads.obj");
	// Reads the same ramp the noise and checker bands' shading already uses,
	// rather than reserving a new one: RETRO_RenderDotModel needs a genuine
	// brightness gradient to shade into, and COL_GRAY..COL_GLENZ already is one.
	// A dot has no occlusion of its own - every vertex is drawn, lit or not -
	// so the ramp starts a few steps shy of the ramp's near-black end: a
	// vertex facing away from the light still shows as a dim dot rather
	// than disappearing into the background.
	Dots->c = COL_GRAY + 6;
	Dots->shades = COL_GLENZ - Dots->c;

	RETRO_InitializeLightSource(0.4f, -0.2f, -1.0f);
}
