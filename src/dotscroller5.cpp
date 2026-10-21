//
// Dot scroller, raised by the terrain
//
// Terrain samples raise a fixed text mask into columns of dots. The sampling
// window orbits the map while the patch rocks. The window moves smoothly,
// but a sample is a whole map cell, so each dot crosses to the next cell
// when the window's fraction passes its ordered-dither threshold: the
// letters change a few dots at a time instead of all at once.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retropalette.h"
#include "lib/retropoly.h"

constexpr RETRO_FontAsset FONT = { "assets/font_16x16.pcx", 16, 16 };
//static const RETRO_FontData &FONT = RETRO_FONT_MINECRAFT_8X8;

constexpr int ORBIT_CENTER_X = 80;
constexpr int ORBIT_CENTER_Y = 54;
constexpr int ORBIT_RADIUS = 20;
constexpr double ORBIT_SPEED = 0.8;

constexpr float DOT_SPACING = 3.0f;
constexpr int GROUND_PAD = 6;
constexpr int EXTRUSION_MIN = 4;
constexpr int EXTRUSION_MAX = 22;
// Index 0 stays the screen's background black. Index 1 is reserved for the
// flat ground; RETRO_LoadImage's own palette load fills the rest, so a
// letter is drawn in whatever color the color map actually painted
// that texel.
constexpr int GROUND_COLOR_INDEX = 1;
constexpr double YAW_AMP = 0.75;
constexpr double YAW_SPEED = 0.35;
constexpr double INITIAL_PITCH = 1.15;
constexpr double PITCH_AMP = 0.38;
constexpr double PITCH_SPEED = 0.45;
constexpr double ROLL_AMP = 0.22;
constexpr double ROLL_SPEED = 0.55;
constexpr float OBJECT_Z = 50.0f;
constexpr float PROJECTION_SCALE = 1.0f;

constexpr int MAX_MAP = 256;

static const char *const ScrollText[] = { "RETRO", "DEMO" };

constexpr int LINES = (int)(sizeof(ScrollText) / sizeof(ScrollText[0]));

static RETRO_Image *HeightMap, *ColorMap, *TextImage;

static unsigned char HeightSample[MAX_MAP * MAX_MAP];
static unsigned char ColorSample[MAX_MAP * MAX_MAP];

// TextImage never changes after DEMO_Initialize, so which grid cell holds a
// letter is looked up directly rather than cached in a mask array.
static bool GlyphLit(int row, int col)
{
	int imagerow = row - GROUND_PAD;
	int imagecol = col - GROUND_PAD;
	if (imagerow < 0 || imagerow >= TextImage->height) return false;
	if (imagecol < 0 || imagecol >= TextImage->width) return false;
	return TextImage->data[imagerow * TextImage->width + imagecol] != 0;
}

static void SampleLandscape(double offsetx, double offsety)
{
	int basex = (int)floor(offsetx);
	int basey = (int)floor(offsety);
	float fx = offsetx - basex;
	float fy = offsety - basey;
	int mapwidth = TextImage->width + 2 * GROUND_PAD;
	int mapheight = TextImage->height + 2 * GROUND_PAD;
	int low = 255;
	int high = 0;
	for (int row = 0; row < mapheight; row++) {
		for (int col = 0; col < mapwidth; col++) {
			int cell = row * mapwidth + col;
			HeightSample[cell] = 0;
			ColorSample[cell] = GROUND_COLOR_INDEX;
			if (!GlyphLit(row, col)) continue;

			float threshold = RETRO_DitherThreshold(col, row);
			int x = (col + basex + (fx > threshold)) % HeightMap->width;
			int y = (row + basey + (fy > threshold)) % HeightMap->height;
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

static void PlotDot(float x, float y, float z, unsigned char color, const mat3 &matrix)
{
	Vertex dot = RETRO_ProjectPoint(matrix * vec3{ x, y, z } + vec3{ 0, 0, OBJECT_Z }, PROJECTION_SCALE);
	if (dot.q == 0.0f) return;

	int sx = (int)lround(dot.spos.x);
	int sy = (int)lround(dot.spos.y);
	if (!RETRO_OnScreen(sx, sy)) return;

	// dot.q is already 1/depth from the projection, and larger means nearer,
	// which is exactly what the shared depth test wants.
	if (RETRO_DepthTest(sy * RETRO_WIDTH + sx, dot.q)) {
		RETRO_PutPixel(sx, sy, color);
	}
}

void DEMO_Render(RETRO_Time time)
{
	double offsetx = ORBIT_CENTER_X + ORBIT_RADIUS * cos(time.total * ORBIT_SPEED);
	double offsety = ORBIT_CENTER_Y + ORBIT_RADIUS * sin(time.total * ORBIT_SPEED);
	SampleLandscape(offsetx, offsety);

	float ax = INITIAL_PITCH + PITCH_AMP * sin(time.total * PITCH_SPEED);
	float ay = YAW_AMP * sin(time.total * YAW_SPEED);
	float az = ROLL_AMP * sin(time.total * ROLL_SPEED);
	mat3 matrix = rotate(ax, ay, az);
	RETRO_ClearDepthBuffer();

	int mapwidth = TextImage->width + 2 * GROUND_PAD;
	int mapheight = TextImage->height + 2 * GROUND_PAD;
	float originx = (mapwidth - 1) / 2.0f;
	float originz = (mapheight - 1) / 2.0f;
	for (int row = 0; row < mapheight; row++) {
		for (int col = 0; col < mapwidth; col++) {
			int cell = row * mapwidth + col;
			int height = HeightSample[cell];
			// Draw the top and the exposed drop to the lowest neighbor.
			int bottom = height;
			bottom = MIN(bottom, row > 0 ? HeightSample[cell - mapwidth] : 0);
			bottom = MIN(bottom, row + 1 < mapheight ? HeightSample[cell + mapwidth] : 0);
			bottom = MIN(bottom, col > 0 ? HeightSample[cell - 1] : 0);
			bottom = MIN(bottom, col + 1 < mapwidth ? HeightSample[cell + 1] : 0);
			float x = (col - originx) * DOT_SPACING;
			float z = (originz - row) * DOT_SPACING;
			for (int level = bottom; level <= height; level++) {
				float y = -level * DOT_SPACING;
				PlotDot(x, y, z, ColorSample[cell], matrix);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	TextImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, LINES);
	HeightMap = RETRO_LoadImage("assets/voxel_height_256x256.pcx");
	ColorMap = RETRO_LoadImage("assets/voxel_color_256x256.pcx", true);
	RETRO_SetColor(GROUND_COLOR_INDEX, RETRO_MIDNIGHTBLUE);
	RETRO_SetColor(0, RETRO_BLACK);
}
