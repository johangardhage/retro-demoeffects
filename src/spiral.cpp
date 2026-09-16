//
// Spiral image distortion
//
// Rotate each source sample about the picture centre by an angle depending
// on radius. The centre twists most; angle and slope fall to zero at the
// outer circle, joining the untouched picture without a hard rim:
//
//   angle = TWIST_AMOUNT sin(2pi t / TWIST_PERIOD) (1 - r²/R²)²
//
// Inverse mapping covers every destination pixel. Radius is preserved, so
// samples stay inside the picture. Bilinear RGB sampling smooths subpixel
// movement before mapping back to the image's palette; palette indices
// themselves must never be interpolated. The twist winds, unwinds, then
// reverses direction, returning to the original picture twice per cycle.
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retroshadetable.h"

#define TWIST_AMOUNT (3.0 * M_PI) // maximum rotation at the centre, radians
#define TWIST_PERIOD 12.0 // seconds for a clockwise/counterclockwise cycle

static RETRO_Image *Picture;
static double Falloff[RETRO_HEIGHT][RETRO_WIDTH];
static unsigned char ColorLUT[64][64][64];

static unsigned char SamplePicture(double x, double y)
{
	int ix = (int)floor(x), iy = (int)floor(y);
	double fx = x - ix, fy = y - iy;
	int x0 = CLAMP(ix, 0, Picture->width), x1 = CLAMP(ix + 1, 0, Picture->width);
	int y0 = CLAMP(iy, 0, Picture->height), y1 = CLAMP(iy + 1, 0, Picture->height);
	RETRO_Palette a = Picture->palette[Picture->data[y0 * Picture->width + x0]];
	RETRO_Palette b = Picture->palette[Picture->data[y0 * Picture->width + x1]];
	RETRO_Palette c = Picture->palette[Picture->data[y1 * Picture->width + x0]];
	RETRO_Palette d = Picture->palette[Picture->data[y1 * Picture->width + x1]];
	double wa = (1 - fx) * (1 - fy), wb = fx * (1 - fy);
	double wc = (1 - fx) * fy, wd = fx * fy;
	int r = CLAMP((wa * a.r + wb * b.r + wc * c.r + wd * d.r) * 63 / 255 + 0.5, 0, 64);
	int g = CLAMP((wa * a.g + wb * b.g + wc * c.g + wd * d.g) * 63 / 255 + 0.5, 0, 64);
	int blue = CLAMP((wa * a.b + wb * b.b + wc * c.b + wd * d.b) * 63 / 255 + 0.5, 0, 64);
	return ColorLUT[r][g][blue];
}

void DEMO_Render(double time, double deltatime)
{
	double phase = fmod(time, TWIST_PERIOD) / TWIST_PERIOD;
	double twist = TWIST_AMOUNT * sin(2 * M_PI * phase);
	double cx = (RETRO_WIDTH - 1) / 2.0, cy = (RETRO_HEIGHT - 1) / 2.0;
	unsigned char *buffer = RETRO_FrameBuffer();

	// Preserve the exact source palette indices at the untwisted moments.
	if (fabs(twist) < 1e-12) {
		memcpy(buffer, Picture->data, RETRO_WIDTH * RETRO_HEIGHT);
		return;
	}
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;
			if (Falloff[y][x] == 0) {
				buffer[offset] = Picture->data[offset];
				continue;
			}
			double angle = twist * Falloff[y][x];
			double c = cos(angle), s = sin(angle);
			double dx = x - cx, dy = y - cy;
			buffer[offset] = SamplePicture(cx + dx * c - dy * s, cy + dx * s + dy * c);
		}
	}
}

void DEMO_Initialize(void)
{
	Picture = RETRO_LoadImage("assets/monkey_320x240.pcx", true);
	if (Picture->width != RETRO_WIDTH || Picture->height != RETRO_HEIGHT) {
		RETRO_RageQuit("Spiral picture must match the screen size\n");
	}
	double cx = (RETRO_WIDTH - 1) / 2.0, cy = (RETRO_HEIGHT - 1) / 2.0;
	double radius = MIN(cx, cy);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double dx = x - cx, dy = y - cy;
			double f = MAX(0.0, 1 - (dx * dx + dy * dy) / (radius * radius));
			Falloff[y][x] = f * f;
		}
	}
	RETRO_CreateColorLUT(Picture->palette, RETRO_COLORS, 64, &ColorLUT[0][0][0]);
}
