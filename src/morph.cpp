//
// Morph
//
// A dark rose with curling, blackened petals fades in from black, and after
// a second keeps turning into a bright red rose and back. Both are
// photographed from the same angle against black. Each picture is warped so
// that its features land where they lie partway between both pictures, and
// the two warped pictures are crossfaded, so a petal always meets a petal
// rather than fading out where the other picture has background.
//
// The features are pairs of line segments, one in each picture, drawn by hand
// over the heart and the outline of the flower (see Features below), and the
// screen edges, which stay where they are so the warp never reaches past the
// pictures. This is the field morph of Beier and Neely (1992). At morph
// amount t a feature lies at the mix of its two lines, PQ. A screen point X
// is placed relative to it by how far along it lies and how far to one side:
//
//   u = (X − P) · (Q − P) / |Q − P|²      v = (X − P) · ⊥(Q − P) / |Q − P|
//
// and the same u and v off the feature's line P'Q' in a picture give the
// point X is taken from there:
//
//   X' = P' + u (Q' − P') + v ⊥(Q' − P') / |Q' − P'|
//
// Every feature has its say, weighted by
//
//   w = (|Q − P|^WARP_P / (WARP_A + d))²
//
// where d is the distance from X to the segment, so the nearest lines rule
// and a longer line reaches further. X is taken from X plus the weighted
// mean of the features' X' − X, in each picture. The two pixels are blended
// in RGB, crossfading evenly from the dark rose to the red one over the
// whole morph, and drawn through an inverse color LUT on a palette fitted to
// both pictures, with a 4x4 ordered dither.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TIME_FADEIN 1.5 // seconds to fade in the dark rose from black at startup
#define TIME_WAIT 1.0 // seconds the dark rose is held after the fade-in
#define TIME_MORPH 6.0 // seconds a morph takes
#define TIME_CYCLE (2 * TIME_MORPH)
#define WARP_A 1.0f // pixels, so the weight stays finite on a line
#define WARP_P 0.5f // how much more a longer line weighs
#define FEATURES 10

// A line segment, start to end, in picture coordinates
struct Line {
	vec2 from, to;
};

// The same feature in either picture
struct Feature {
	Line dark, red;
};

static const Feature Features[FEATURES] = {
	{ { { 150, 40 }, { 150, 160 } }, { { 140, 25 }, { 155, 160 } } }, // top to bottom of the heart
	{ { { 88, 100 }, { 205, 90 } }, { { 85, 85 }, { 220, 95 } } }, // left to right of the heart
	{ { { 110, 35 }, { 185, 15 } }, { { 100, 42 }, { 195, 25 } } }, // top of the flower
	{ { { 60, 125 }, { 62, 163 } }, { { 50, 100 }, { 75, 190 } } }, // left of the flower
	{ { { 225, 55 }, { 255, 145 } }, { { 265, 65 }, { 255, 165 } } }, // right of the flower
	{ { { 135, 222 }, { 195, 225 } }, { { 150, 215 }, { 220, 225 } } }, // bottom of the flower
	{ { { 0, 0 }, { 319, 0 } }, { { 0, 0 }, { 319, 0 } } }, // screen edges, held in place
	{ { { 319, 0 }, { 319, 239 } }, { { 319, 0 }, { 319, 239 } } },
	{ { { 319, 239 }, { 0, 239 } }, { { 319, 239 }, { 0, 239 } } },
	{ { { 0, 239 }, { 0, 0 } }, { { 0, 239 }, { 0, 0 } } },
};

static RETRO_Image *PictureA;
static RETRO_Image *PictureB;
static RETRO_ColorHistogram Histogram;
static RETRO_Palette Palette[RETRO_COLORS];
static unsigned char ColorLUT[32][32][32];

static vec2 Perpendicular(vec2 v)
{
	return { -v.y, v.x };
}

void DEMO_Render(RETRO_Time time)
{
	// Hardware DAC palette fade-in of the dark rose at startup
	RETRO_Fade(time.total / TIME_FADEIN, Palette);

	// Calculate phase: morphing to the red rose and back, without a pause,
	// once the fade-in and the wait after it are over
	double phase = fmod(fmax(time.total - TIME_FADEIN - TIME_WAIT, 0.0), TIME_CYCLE);
	float t = smoothstep(0.0, TIME_MORPH, phase) - smoothstep(TIME_MORPH, TIME_CYCLE, phase);

	// The crossfade runs evenly over the whole warp, not eased like it
	float fade = (float)(1 - fabs(phase / TIME_MORPH - 1));

	// Where each feature lies this frame, and its line in either picture
	vec2 from[FEATURES], to[FEATURES], along[FEATURES], across[FEATURES];
	vec2 froma[FEATURES], alonga[FEATURES], acrossa[FEATURES];
	vec2 fromb[FEATURES], alongb[FEATURES], acrossb[FEATURES];
	float strength[FEATURES];
	for (int i = 0; i < FEATURES; i++) {
		Line a = Features[i].dark;
		Line b = Features[i].red;
		from[i] = mix(a.from, b.from, t);
		to[i] = mix(a.to, b.to, t);
		vec2 d = to[i] - from[i];
		float length2 = dot(d, d);
		float linelength = sqrtf(length2);
		along[i] = d / length2;
		across[i] = Perpendicular(d) / linelength;
		strength[i] = powf(linelength, WARP_P);

		froma[i] = a.from;
		alonga[i] = a.to - a.from;
		acrossa[i] = Perpendicular(normalize(alonga[i]));
		fromb[i] = b.from;
		alongb[i] = b.to - b.from;
		acrossb[i] = Perpendicular(normalize(alongb[i]));
	}

	unsigned char *a = PictureA->data;
	unsigned char *b = PictureB->data;
	RETRO_Palette *pa = PictureA->palette;
	RETRO_Palette *pb = PictureB->palette;
	unsigned char *buffer = RETRO_FrameBuffer();

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			vec2 p = { (float)x, (float)y };
			vec2 suma = {}, sumb = {};
			float weights = 0;
			for (int i = 0; i < FEATURES; i++) {
				vec2 offset = p - from[i];
				float u = dot(offset, along[i]);
				float v = dot(offset, across[i]);

				// Distance to the segment
				float d = u < 0 ? length(offset) : u > 1 ? distance(p, to[i]) : fabsf(v);
				float w = strength[i] / (WARP_A + d);
				w *= w;

				suma += (froma[i] + alonga[i] * u + acrossa[i] * v - p) * w;
				sumb += (fromb[i] + alongb[i] * u + acrossb[i] * v - p) * w;
				weights += w;
			}
			vec2 qa = p + suma / weights;
			vec2 qb = p + sumb / weights;

			int ia = CLAMP((int)(qa.y + 0.5f), 0, RETRO_HEIGHT) * RETRO_WIDTH + CLAMP((int)(qa.x + 0.5f), 0, RETRO_WIDTH);
			int ib = CLAMP((int)(qb.y + 0.5f), 0, RETRO_HEIGHT) * RETRO_WIDTH + CLAMP((int)(qb.x + 0.5f), 0, RETRO_WIDTH);
			RETRO_Palette color = mix(pa[a[ia]], pb[b[ib]], fade);
			int dither = RETRO_DitherLevel(x, y) - 7; // -7..8, about zero
			int r = CLAMP256(color.r + dither);
			int g = CLAMP256(color.g + dither);
			int bch = CLAMP256(color.b + dither);

			buffer[y * RETRO_WIDTH + x] = ColorLUT[r >> 3][g >> 3][bch >> 3];
		}
	}
}

void DEMO_Initialize(void)
{
	PictureA = RETRO_LoadImage("assets/rose_dark_320x240.pcx");
	PictureB = RETRO_LoadImage("assets/rose_red_320x240.pcx");

	// Init palette, fitted to the colors of both pictures, and the inverse
	// color LUT onto it
	for (int i = 0; i < RETRO_WIDTH * RETRO_HEIGHT; i++) {
		RETRO_AddHistogramColor(&Histogram, PictureA->palette[PictureA->data[i]], 1);
		RETRO_AddHistogramColor(&Histogram, PictureB->palette[PictureB->data[i]], 1);
	}
	RETRO_CreateHistogramPalette(&Histogram, Palette);
	RETRO_CreateColorLUT(Palette, 32, &ColorLUT[0][0][0]);
}
