//
// Dot landscape scroller: terrain samples raise a fixed text mask into
// columns of dots. The sampling window orbits the map while the patch rocks.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropalette.h"
#include "lib/retropoly.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define ORBIT_CENTER_X 80
#define ORBIT_CENTER_Y 54
#define ORBIT_RADIUS 20
#define ORBIT_SPEED 0.8

#define DOT_SPACING 3.0f
#define GROUND_PAD 6
#define EXTRUSION_MIN 4
#define EXTRUSION_MAX 22
// Index 0 stays the screen's background black. Index 1 is reserved for the
// flat ground; RETRO_LoadImage's own palette load fills the rest, so a
// letter is drawn in whatever colour the colour map actually painted
// that texel.
#define GROUND_COLOR_INDEX 1
#define YAW_AMP 0.75
#define YAW_SPEED 0.35
#define INITIAL_PITCH 1.15
#define PITCH_AMP 0.38
#define PITCH_SPEED 0.45
#define ROLL_AMP 0.22
#define ROLL_SPEED 0.55
#define OBJECT_Z 50.0f
#define PROJECTION_SCALE 1.0f

#define MAX_MAP 256

static const char *const ScrollText[] = { "RETRO", "DEMO" };
static RETRO_Image *HeightMap, *ColorMap, *TextImage;

static unsigned char HeightSample[MAX_MAP * MAX_MAP];
static unsigned char ColorSample[MAX_MAP * MAX_MAP];

// TextImage never changes after DEMO_Initialize, so which grid cell holds a
// letter is looked up directly rather than cached in a mask array.
static bool GlyphLit(int row, int col)
{
	int imagerow = row - GROUND_PAD, imagecol = col - GROUND_PAD;
	if (imagerow < 0 || imagerow >= TextImage->height) return false;
	if (imagecol < 0 || imagecol >= TextImage->width) return false;
	return TextImage->data[imagerow * TextImage->width + imagecol] != 0;
}

static void SampleLandscape(int offsetx, int offsety)
{
	int mapwidth = TextImage->width + 2 * GROUND_PAD;
	int mapheight = TextImage->height + 2 * GROUND_PAD;
	int low = 255, high = 0;
	for (int row = 0; row < mapheight; row++) {
		for (int col = 0; col < mapwidth; col++) {
			int cell = row * mapwidth + col;
			HeightSample[cell] = 0;
			ColorSample[cell] = GROUND_COLOR_INDEX;
			if (!GlyphLit(row, col)) continue;

			int x = (col + offsetx) % HeightMap->width;
			int y = (row + offsety) % HeightMap->height;
			int sample = y * HeightMap->width + x;
			HeightSample[cell] = HeightMap->data[sample];
			ColorSample[cell] = ColorMap->data[sample];
			low = MIN(low, HeightSample[cell]);
			high = MAX(high, HeightSample[cell]);
		}
	}
	// Stretch the sampled heights so the letters retain relief on flat terrain.
	for (int row = 0; row < mapheight; row++) {
		for (int col = 0; col < mapwidth; col++) {
			if (!GlyphLit(row, col)) continue;
			int cell = row * mapwidth + col;
			int sample = HeightSample[cell];
			int offset = sample - low;
			int inputrange = high - low;
			int outputrange = EXTRUSION_MAX - EXTRUSION_MIN;
			int stretched = high > low
				? EXTRUSION_MIN + offset * outputrange / inputrange
				: (EXTRUSION_MIN + EXTRUSION_MAX) / 2;
			HeightSample[cell] = stretched;
		}
	}
}

static void PlotDot(float x, float y, float z, unsigned char color, const RETRO_RotationTrig &rotation)
{
	Vertex dot = {};
	dot.pos = { x, y, z };
	RETRO_RotateVertexTrig(&dot, rotation);
	dot.rpos.z += OBJECT_Z;
	RETRO_ProjectVertex(&dot, PROJECTION_SCALE);
	if (dot.q == 0.0f) return;

	int sx = (int)lround(dot.spos.x), sy = (int)lround(dot.spos.y);
	if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) return;

	// dot.q is already 1/depth from the projection, and larger means nearer,
	// which is exactly what the shared depth test wants.
	if (RETRO_DepthTest(sy * RETRO_WIDTH + sx, dot.q)) {
		RETRO_PutPixel(sx, sy, color);
	}
}

void DEMO_Render(double time, double deltatime)
{
	int offsetx = (int)(ORBIT_CENTER_X + ORBIT_RADIUS * cos(time * ORBIT_SPEED));
	int offsety = (int)(ORBIT_CENTER_Y + ORBIT_RADIUS * sin(time * ORBIT_SPEED));
	SampleLandscape(offsetx, offsety);

	float ax = INITIAL_PITCH + PITCH_AMP * sin(time * PITCH_SPEED);
	float ay = YAW_AMP * sin(time * YAW_SPEED);
	float az = ROLL_AMP * sin(time * ROLL_SPEED);
	RETRO_RotationTrig rotation = RETRO_InitializeRotationTrig(ax, ay, az);
	RETRO_ClearDepthBuffer();

	int mapwidth = TextImage->width + 2 * GROUND_PAD;
	int mapheight = TextImage->height + 2 * GROUND_PAD;
	float originx = (mapwidth - 1) / 2.0f;
	float originz = (mapheight - 1) / 2.0f;
	for (int row = 0; row < mapheight; row++) {
		for (int col = 0; col < mapwidth; col++) {
			int cell = row * mapwidth + col;
			int height = HeightSample[cell];
			// Draw the top and the exposed drop to the lowest neighbour.
			int bottom = height;
			bottom = MIN(bottom, row > 0 ? HeightSample[cell - mapwidth] : 0);
			bottom = MIN(bottom, row + 1 < mapheight ? HeightSample[cell + mapwidth] : 0);
			bottom = MIN(bottom, col > 0 ? HeightSample[cell - 1] : 0);
			bottom = MIN(bottom, col + 1 < mapwidth ? HeightSample[cell + 1] : 0);
			float x = (col - originx) * DOT_SPACING;
			float z = (originz - row) * DOT_SPACING;
			for (int level = bottom; level <= height; level++) {
				float y = -level * DOT_SPACING;
				PlotDot(x, y, z, ColorSample[cell], rotation);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	TextImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	HeightMap = RETRO_LoadImage("assets/voxel_height_256x256.pcx");
	ColorMap = RETRO_LoadImage("assets/voxel_color_256x256.pcx", true);
	RETRO_SetColor(GROUND_COLOR_INDEX, RETRO_MIDNIGHTBLUE);
	RETRO_SetColor(0, RETRO_BLACK);
}
