//
// Dot landscape scroller
//
// A finite grid of dots is the landscape, a sine in x and z so the platform
// is not a plane. The font (see FONT below) is packed into one strip at
// startup. Every lit texel is a column standing on that surface,
//
//   x     = (column − phase) · SPACING
//   z     = (row0 + row − depth/2) · SPACING
//   base  = WAVE · sin(kx x + ω t) · sin(kz z) + LETTER_GAP · SPACING
//   y     = −(base + level · SPACING)
//
// for level in 0..EXTRUSION on an outline texel, and only the top of an
// interior one, so a letter is a box of dots rather than a filled solid.
// The landscape grid cell a texel maps onto is skipped, so the flat ground
// dot never shows through below a letter. phase lives on stripwidth, in
// cells per second. A letter that leaves on the left is wrapped by one
// stripwidth and plotted again if that copy still sits on the map, so the
// strip loops. A zero texel is a gap. The
// patch nods about y and is then pushed back OBJECT_Z, viewed at a fixed
// pitch. Shade falls with rz, so keeping the brighter of two dots on one
// pixel is an exact depth test.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropalette.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define MAP_WIDTH 80 // cells across the platform
#define MAP_DEPTH 40 // and along it
#define DOT_SPACING 3.0f // pixels between neighbouring dots, the same in x, y and z
#define EXTRUSION 8 // levels a lit texel stands above the landscape
#define LETTER_GAP 3 // levels of clearance between the floor and a letter's underside
#define WAVE_AMP 4.0f // pixels the sine lifts the platform
#define WAVE_KX 0.045f // radians per pixel of that sine in x
#define WAVE_KZ 0.070f // and in z
#define WAVE_SPEED 0.55 // radians per second the wave travels
#define SCROLL_SPEED 10.0f // cells per second the strip travels left
#define SHADES 64 // palette entries the depth shading ramps over
#define YAW_AMP 0.6 // radians either side of centre the patch turns about y
#define YAW_SPEED 0.5 // radians per second of that turn
#define PITCH 1.15 // radians of x the view holds, looking down
#define OBJECT_Z 55.0f // model units the patch is pushed back after the turn
#define PROJECTION_SCALE 1.0f // the dots are built in pixels, so the projection adds no scale

static const char *const ScrollText[] = { "RETRO DEMOEFFECTS...        " };

RETRO_Image *ScrollImage;

float OriginX;
float OriginZ;
int TextRow0;
float DepthNear;
float DepthFar;

static float CosAx, SinAx, CosAy, SinAy;

static float LandscapeY(float x, float z, float time)
{
	return WAVE_AMP * sinf(x * WAVE_KX + time * WAVE_SPEED) * sinf(z * WAVE_KZ);
}

static bool GlyphInk(int x, int y)
{
	if (x < 0 || y < 0 || x >= ScrollImage->width || y >= ScrollImage->height) {
		return false;
	}
	return ScrollImage->data[y * ScrollImage->width + x] != 0;
}

static void PlotDot(float x, float y, float z)
{
	float ry = y * CosAx - z * SinAx;
	float rz = y * SinAx + z * CosAx;
	float rx = x * CosAy + rz * SinAy;
	rz = x * -SinAy + rz * CosAy;

	Vertex vertex;
	vertex.rx = rx;
	vertex.ry = ry;
	vertex.rz = rz + OBJECT_Z;
	RETRO_ProjectVertex(&vertex, PROJECTION_SCALE);

	if (vertex.q == 0.0f) {
		return;
	}

	int sx = (int)lround(vertex.sx);
	int sy = (int)lround(vertex.sy);
	if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) {
		return;
	}

	int color = (int)((DepthFar - vertex.rz) / (DepthFar - DepthNear) * (SHADES - 1));
	if (color < 1) {
		return;
	}
	if (color > SHADES - 1) {
		color = SHADES - 1;
	}

	if (color > RETRO_GetPixel(sx, sy)) {
		RETRO_PutPixel(sx, sy, color);
	}
}

static void PlotLetterColumn(float x, float z, float time, bool wall)
{
	float base = LandscapeY(x, z, time) + LETTER_GAP * DOT_SPACING;
	PlotDot(x, -(base + EXTRUSION * DOT_SPACING), z);
	if (!wall) {
		return;
	}
	for (int level = 0; level < EXTRUSION; level++) {
		PlotDot(x, -(base + level * DOT_SPACING), z);
	}
}

void DEMO_Render(double time, double deltatime)
{
	float ay = YAW_AMP * sin(time * YAW_SPEED);

	CosAx = cos(PITCH);
	SinAx = sin(PITCH);
	CosAy = cos(ay);
	SinAy = sin(ay);

	float phase = fmod(time * SCROLL_SPEED, (double)ScrollImage->width);

	bool occupied[MAP_DEPTH][MAP_WIDTH] = {};

	for (int sy = 0; sy < ScrollImage->height; sy++) {
		for (int sx = 0; sx < ScrollImage->width; sx++) {
			if (!GlyphInk(sx, sy)) {
				continue;
			}

			bool wall = !GlyphInk(sx - 1, sy) || !GlyphInk(sx + 1, sy) ||
				!GlyphInk(sx, sy - 1) || !GlyphInk(sx, sy + 1);

			for (int copy = 0; copy < 2; copy++) {
				float col = sx - phase + copy * ScrollImage->width;
				if (col < 0 || col >= MAP_WIDTH) {
					continue;
				}

				occupied[TextRow0 + sy][(int)lround(col)] = true;

				float x = (col - OriginX) * DOT_SPACING;
				float z = (OriginZ - (TextRow0 + sy)) * DOT_SPACING;
				PlotLetterColumn(x, z, (float)time, wall);
			}
		}
	}

	for (int row = 0; row < MAP_DEPTH; row++) {
		for (int col = 0; col < MAP_WIDTH; col++) {
			if (occupied[row][col]) {
				continue;
			}
			float x = (col - OriginX) * DOT_SPACING;
			float z = (OriginZ - row) * DOT_SPACING;
			PlotDot(x, -LandscapeY(x, z, (float)time), z);
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, SHADES, RETRO_BLACK, RETRO_WHITE);

	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	if (ScrollImage->height > MAP_DEPTH) {
		RETRO_RageQuit("Scroll strip is taller than the landscape\n");
	}

	OriginX = (MAP_WIDTH - 1) / 2.0f;
	OriginZ = (MAP_DEPTH - 1) / 2.0f;
	TextRow0 = (MAP_DEPTH - ScrollImage->height) / 2;

	float halfw = OriginX * DOT_SPACING;
	float halfd = OriginZ * DOT_SPACING;
	float halfh = WAVE_AMP + (LETTER_GAP + EXTRUSION) * DOT_SPACING;
	float radius = sqrtf(halfw * halfw + halfd * halfd + halfh * halfh);
	DepthNear = OBJECT_Z - radius;
	DepthFar = OBJECT_Z + radius;
}
