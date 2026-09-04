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

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8

#define HEIGHTFIELD "assets/voxel_height_256x256.pcx"
#define COLORFIELD "assets/voxel_color_256x256.pcx"
#define ORBIT_CENTER_X 80
#define ORBIT_CENTER_Y 54
#define ORBIT_RADIUS 20
#define ORBIT_SPEED 0.8

#define DOT_SPACING 3.0f
#define GROUND_PAD 6
#define EXTRUSION_MIN 4
#define EXTRUSION_MAX 22
#define LETTER_BANDS 6
#define GROUND_BAND LETTER_BANDS
#define BAND_SHADES 32
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

static RETRO_Image *HeightField, *ColorField;
static int MapWidth, MapHeight;
static float OriginX, OriginZ, DepthNear, DepthFar;
static unsigned char LitMask[MAX_MAP * MAX_MAP];
static unsigned char HeightMap[MAX_MAP * MAX_MAP];
static unsigned char BandMap[MAX_MAP * MAX_MAP];
static float ZBuffer[RETRO_WIDTH * RETRO_HEIGHT];
static float CosAx, SinAx, CosAy, SinAy, CosAz, SinAz;

// The terrain palette is muted, so push each channel away from the colour's
// own grey level to make the hue vivid before shading it dark and light.
static RETRO_Palette Saturate(RETRO_Palette c, float s)
{
	float gray = (c.r + c.g + c.b) / 3.0f;
	return { (unsigned char)CLAMP256(gray + (c.r - gray) * s), (unsigned char)CLAMP256(gray + (c.g - gray) * s), (unsigned char)CLAMP256(gray + (c.b - gray) * s) };
}

// Each band's shade ramp runs dark to light from one representative colour
// sampled straight out of the colour map's own palette.
static void InitializePalette()
{
	for (int band = 0; band < LETTER_BANDS; band++) {
		RETRO_Palette c = Saturate(ColorField->palette[band * RETRO_COLORS / LETTER_BANDS], 2.2f);
		RETRO_Palette dark = { (unsigned char)(c.r * 0.6f), (unsigned char)(c.g * 0.6f), (unsigned char)(c.b * 0.6f) };
		RETRO_Palette light = { (unsigned char)CLAMP256(c.r * 1.3f), (unsigned char)CLAMP256(c.g * 1.3f), (unsigned char)CLAMP256(c.b * 1.3f) };
		RETRO_CreateGradientPalette(1 + band * BAND_SHADES, 1 + (band + 1) * BAND_SHADES, dark, light);
	}
	RETRO_CreateGradientPalette(1 + GROUND_BAND * BAND_SHADES, 1 + (GROUND_BAND + 1) * BAND_SHADES, RETRO_Palette{ 5, 5, 30 }, RETRO_MIDNIGHTBLUE);
}

static void SampleLandscape(int offsetx, int offsety)
{
	int low = 255, high = 0;
	for (int row = 0; row < MapHeight; row++) {
		for (int col = 0; col < MapWidth; col++) {
			int cell = row * MapWidth + col;
			HeightMap[cell] = 0;
			BandMap[cell] = GROUND_BAND;
			if (!LitMask[cell]) continue;

			int x = (col + offsetx) % HeightField->width;
			int y = (row + offsety) % HeightField->height;
			int sample = y * HeightField->width + x;
			HeightMap[cell] = HeightField->data[sample];
			BandMap[cell] = ColorField->data[sample] * LETTER_BANDS / RETRO_COLORS;
			low = MIN(low, HeightMap[cell]);
			high = MAX(high, HeightMap[cell]);
		}
	}
	// Stretch the sampled heights so the letters retain relief on flat terrain.
	for (int cell = 0; cell < MapWidth * MapHeight; cell++) {
		if (!LitMask[cell]) continue;
		HeightMap[cell] = high > low
			? EXTRUSION_MIN + (HeightMap[cell] - low) * (EXTRUSION_MAX - EXTRUSION_MIN) / (high - low)
			: (EXTRUSION_MIN + EXTRUSION_MAX) / 2;
	}
}

static void PlotDot(float x, float y, float z, int band)
{
	float ry = y * CosAx - z * SinAx;
	float rz = y * SinAx + z * CosAx;
	float rx = x * CosAy + rz * SinAy;
	rz = -x * SinAy + rz * CosAy;
	Vertex dot = {};
	dot.rx = rx * CosAz - ry * SinAz;
	dot.ry = rx * SinAz + ry * CosAz;
	dot.rz = rz + OBJECT_Z;
	RETRO_ProjectVertex(&dot, PROJECTION_SCALE);
	if (dot.q == 0.0f) return;

	int sx = (int)lround(dot.sx), sy = (int)lround(dot.sy);
	if (sx < 0 || sx >= RETRO_WIDTH || sy < 0 || sy >= RETRO_HEIGHT) return;
	int pixel = sy * RETRO_WIDTH + sx;
	if (dot.rz >= ZBuffer[pixel]) return;

	int shade = (DepthFar - dot.rz) / (DepthFar - DepthNear) * (BAND_SHADES - 1);
	shade = MAX(1, MIN(BAND_SHADES - 1, shade));
	ZBuffer[pixel] = dot.rz;
	RETRO_PutPixel(sx, sy, 1 + band * BAND_SHADES + shade);
}

void DEMO_Render(double time, double deltatime)
{
	SampleLandscape((int)(ORBIT_CENTER_X + ORBIT_RADIUS * cos(time * ORBIT_SPEED)),
		(int)(ORBIT_CENTER_Y + ORBIT_RADIUS * sin(time * ORBIT_SPEED)));

	float ax = INITIAL_PITCH + PITCH_AMP * sin(time * PITCH_SPEED);
	float ay = YAW_AMP * sin(time * YAW_SPEED);
	float az = ROLL_AMP * sin(time * ROLL_SPEED);
	CosAx = cos(ax); SinAx = sin(ax);
	CosAy = cos(ay); SinAy = sin(ay);
	CosAz = cos(az); SinAz = sin(az);
	for (int pixel = 0; pixel < RETRO_WIDTH * RETRO_HEIGHT; pixel++) ZBuffer[pixel] = DepthFar + 1;

	for (int row = 0; row < MapHeight; row++) {
		for (int col = 0; col < MapWidth; col++) {
			int cell = row * MapWidth + col;
			int height = HeightMap[cell];
			// Draw the top and the exposed drop to the lowest neighbour.
			int bottom = height;
			bottom = MIN(bottom, row > 0 ? HeightMap[cell - MapWidth] : 0);
			bottom = MIN(bottom, row + 1 < MapHeight ? HeightMap[cell + MapWidth] : 0);
			bottom = MIN(bottom, col > 0 ? HeightMap[cell - 1] : 0);
			bottom = MIN(bottom, col + 1 < MapWidth ? HeightMap[cell + 1] : 0);
			for (int level = bottom; level <= height; level++) {
				PlotDot((col - OriginX) * DOT_SPACING, -level * DOT_SPACING,
					(OriginZ - row) * DOT_SPACING, BandMap[cell]);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *image = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]));
	HeightField = RETRO_LoadImage(HEIGHTFIELD);
	ColorField = RETRO_LoadImage(COLORFIELD);

	if (HeightField->width != ColorField->width || HeightField->height != ColorField->height) {
		RETRO_RageQuit("Terrain height and colour maps must have matching dimensions\n");
	}
	InitializePalette();

	MapWidth = image->width + 2 * GROUND_PAD;
	MapHeight = image->height + 2 * GROUND_PAD;
	if (MapWidth > MAX_MAP || MapHeight > MAX_MAP) {
		RETRO_RageQuit("Dot landscape is larger than the height map\n");
	}
	OriginX = (MapWidth - 1) / 2.0f;
	OriginZ = (MapHeight - 1) / 2.0f;

	for (int row = 0; row < MapHeight; row++) {
		int imagerow = row - GROUND_PAD;
		for (int col = 0; col < MapWidth; col++) {
			int imagecol = col - GROUND_PAD;
			LitMask[row * MapWidth + col] = imagerow >= 0 && imagerow < image->height &&
				imagecol >= 0 && imagecol < image->width &&
				image->data[imagerow * image->width + imagecol] != 0;
		}
	}

	float radius = DOT_SPACING * sqrtf(OriginX * OriginX + OriginZ * OriginZ + EXTRUSION_MAX * EXTRUSION_MAX);
	DepthNear = OBJECT_Z - radius;
	DepthFar = OBJECT_Z + radius;
}
