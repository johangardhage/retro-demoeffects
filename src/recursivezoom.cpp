//
// Recursive picture zoom
//
// The stone doorway's transparent opening reveals the next smaller copy.
// Palette index zero is the aperture; all masonry uses nonzero indices, a
// border fully included, since Bilinear clamps off-edge samples to it.
// Each child is 60% of its parent's size, cropped by the arch silhouette.
// The artwork must match the screen size.
//
// Trace from large ancestors down through transparent pixels until stone is
// hit. Starting ANCESTOR_LEVELS above the viewport includes the curved
// parent arch overlapping a child's corners, even across the zoom-period
// boundary. That starting level lies entirely inside the aperture, so
// wrapping time only renames the levels and produces the same view - and
// contributes no color of its own, which a deterministic sweep of every
// phase in the cycle confirmed for both the outermost level and the one
// below it. Always resample the original artwork, never the previous
// frame, to avoid cumulative blur.
//
// Mip levels filter distant arches as well as magnified ones: each level
// averages 2x2 blocks of its parent, ordinary mipmap generation, so a
// bilinear tap into a small level already blends many source pixels rather
// than sampling one of them sparsely.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define CHILD_SCALE 0.6 // child image size before clipping to the arch
#define ZOOM_PERIOD 4.0 // seconds for one child to grow to its parent's size
#define MAX_DEPTH 32 // bounds the singular center pixel at odd resolutions
#define ANCESTOR_LEVELS 2 // trace starts this many levels above the viewport
#define INV_CHILD_SCALE (1.0 / CHILD_SCALE) // the trace's per-level growth is a multiply, not a divide

#define MAX_LEVELS 16 // generous bound for a mip pyramid down from RETRO_WIDTH x RETRO_HEIGHT to 1x1
#define MAX_PIXELS (RETRO_WIDTH * RETRO_HEIGHT * 4 / 3 + 64) // geometric series bound, each level near a quarter of its parent

// Premultiplied color keeps the transparent doorway from bleeding black
// into the stone.
struct Sample { float r, g, b, a; };
struct Level { int width, height; Sample *pixels; };
static Level Levels[MAX_LEVELS];
static int LevelCount;
static Sample PixelPool[MAX_PIXELS]; // every level's pixels, packed back to back
static unsigned char ColorLUT[64][64][64];

static Sample Mix(Sample a, Sample b, float t)
{
	return { mix(a.r, b.r, t), mix(a.g, b.g, t),
		mix(a.b, b.b, t), mix(a.a, b.a, t) };
}

// Sampling within a level, for the recursive trace: continuous, so a
// non-integer (u, v) reads a smooth blend of its four nearest texels.
static Sample Bilinear(int level, double u, double v)
{
	const Level &map = Levels[level];
	double x = (u + 0.5) * map.width - 0.5;
	double y = (v + 0.5) * map.height - 0.5;
	int ix = (int)floor(x), iy = (int)floor(y);
	int x0 = CLAMP(ix, 0, map.width), x1 = CLAMP(ix + 1, 0, map.width);
	int y0 = CLAMP(iy, 0, map.height), y1 = CLAMP(iy + 1, 0, map.height);
	return Mix(Mix(map.pixels[y0 * map.width + x0], map.pixels[y0 * map.width + x1], x - ix),
		Mix(map.pixels[y1 * map.width + x0], map.pixels[y1 * map.width + x1], x - ix), y - iy);
}

// Building the mip pyramid: one output texel is the average of the 2x2
// parent block below it. An odd parent dimension leaves that block short a
// row or column, so the last valid texel stands in for the missing one
// rather than the average dropping to fewer samples.
static Sample DownsampleTexel(const Level &parent, int x, int y)
{
	int x0 = x * 2, x1 = MIN(x0 + 1, parent.width - 1);
	int y0 = y * 2, y1 = MIN(y0 + 1, parent.height - 1);
	Sample a = parent.pixels[y0 * parent.width + x0];
	Sample b = parent.pixels[y0 * parent.width + x1];
	Sample c = parent.pixels[y1 * parent.width + x0];
	Sample d = parent.pixels[y1 * parent.width + x1];
	return { (a.r + b.r + c.r + d.r) / 4, (a.g + b.g + c.g + d.g) / 4,
		(a.b + b.b + c.b + d.b) / 4, (a.a + b.a + c.a + d.a) / 4 };
}

void DEMO_Render(double time, double deltatime)
{
	double phase = fmod(time, ZOOM_PERIOD) / ZOOM_PERIOD;
	double zoom = pow(1.0 / CHILD_SCALE, phase);
	double ancestor = pow(CHILD_SCALE, ANCESTOR_LEVELS);
	double scale = ancestor / zoom; // constant for the whole frame; only depth steps it further
	double footprint = scale; // the artwork is always exactly screen sized, so no ratio to fold in
	int mip[MAX_DEPTH];
	float blend[MAX_DEPTH];
	for (int depth = 0; depth < MAX_DEPTH; depth++) {
		double lod = clamp(log2(footprint), 0.0, LevelCount - 1.0);
		mip[depth] = (int)lod;
		blend[depth] = lod - mip[depth];
		footprint /= CHILD_SCALE;
	}
	unsigned char *buffer = RETRO_FrameBuffer();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double v0 = ((y + 0.5) / RETRO_HEIGHT - 0.5) * scale;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double u = ((x + 0.5) / RETRO_WIDTH - 0.5) * scale;
			double v = v0;
			Sample color = { 0, 0, 0, 0 };
			float remaining = 1;
			for (int depth = 0; depth < MAX_DEPTH; depth++) {
				Sample stone = Bilinear(mip[depth], u, v);
				if (blend[depth] > 0) stone = Mix(stone, Bilinear(mip[depth] + 1, u, v), blend[depth]);
				color.r += remaining * stone.r;
				color.g += remaining * stone.g;
				color.b += remaining * stone.b;
				remaining *= 1 - stone.a;
				if (remaining < 1.0f / 255) break;
				u *= INV_CHILD_SCALE;
				v *= INV_CHILD_SCALE;
			}
			buffer[y * RETRO_WIDTH + x] = ColorLUT[CLAMP256(color.r) >> 2]
				[CLAMP256(color.g) >> 2][CLAMP256(color.b) >> 2];
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Image *picture = RETRO_LoadImage("assets/recursivezoom_320x240.pcx", true);
	LevelCount = 0;
	int pixelpoolused = 0;

	Level &base = Levels[LevelCount++];
	base.width = picture->width;
	base.height = picture->height;
	base.pixels = &PixelPool[pixelpoolused];
	pixelpoolused += base.width * base.height;
	for (int i = 0; i < picture->width * picture->height; i++) {
		int index = picture->data[i];
		RETRO_Palette c = picture->palette[index];
		base.pixels[i] = index ? Sample{ (float)c.r, (float)c.g, (float)c.b, 1 } : Sample{ 0, 0, 0, 0 };
	}
	// Bilinear clamps an out-of-range sample to the border, so the ancestor
	// levels sampled outside [-0.5, 0.5] read as stone rather than as more
	// aperture only if that border is fully opaque.
	for (int x = 0; x < picture->width; x++) {
		if (picture->data[x] == 0 || picture->data[(picture->height - 1) * picture->width + x] == 0) {
			RETRO_RageQuit("Recursivezoom picture's border must be fully opaque\n");
		}
	}
	for (int y = 0; y < picture->height; y++) {
		if (picture->data[y * picture->width] == 0 || picture->data[y * picture->width + picture->width - 1] == 0) {
			RETRO_RageQuit("Recursivezoom picture's border must be fully opaque\n");
		}
	}
	while (Levels[LevelCount - 1].width > 1 || Levels[LevelCount - 1].height > 1) {
		const Level &parent = Levels[LevelCount - 1];
		Level &next = Levels[LevelCount++];
		next.width = MAX(1, parent.width / 2);
		next.height = MAX(1, parent.height / 2);
		next.pixels = &PixelPool[pixelpoolused];
		pixelpoolused += next.width * next.height;
		for (int y = 0; y < next.height; y++) {
			for (int x = 0; x < next.width; x++) {
				next.pixels[y * next.width + x] = DownsampleTexel(parent, x, y);
			}
		}
	}
	RETRO_CreateColorLUT(picture->palette, 64, &ColorLUT[0][0][0]);
}
