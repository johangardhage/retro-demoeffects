//
// Mirror ball behind mint lettering, based on mirror.mp4.
// Reflected rays reach a virtual logo panel in front of the sphere through a
// stereographic projection, so the reflection runs all the way to the rim.
// The ball is shaded afresh each frame at its exact, fractional position, so
// it glides instead of stepping from pixel to pixel.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

static const float Radius = 29.0f;
static const float PanelDistance = 80.0f;
static const vec3 Background = {0, 0, 45};
static const vec3 ViewRay = {0, 0, -1}; // Orthographic, z toward the viewer.

//
// A glint is placed where it shows on the ball, as the normal (nx, ny) seen
// head on, and lights the disc of reflected directions within acos(edge) of
// the one reflected there.
// Positions are measured from the reference and must lie inside the unit disc.
// Next to the silhouette a cone of directions stretches into an arc along the
// rim, so the rim glints sit a little inside it.
//
struct Glint {
	float nx, ny, edge;
};

static const Glint Glints[] = {
	{0.40f, -0.18f, 0.980f},
	{-0.47f, 0.05f, 0.985f},
	{-0.40f, 0.33f, 0.997f},
	{0.06f, -0.84f, 0.988f},
	{0.87f, -0.25f, 0.996f},
	{-0.90f, 0.05f, 0.996f},
	{0.45f, 0.50f, 0.998f},
	{0.70f, 0.45f, 0.998f}
};

static RETRO_Image *Logo;
static const int GlintCount = sizeof(Glints) / sizeof(Glints[0]);
static vec3 GlintLights[GlintCount]; // The light direction each glint reflects.
static float GlintSizes[GlintCount];  // Its radius, as a chord between unit vectors.
static float BallX, BallY;
static unsigned char ColorLookup[32 * 32 * 32];

//
// Bilinear logo lookup in screen coordinates, where pixel i covers [i, i + 1)
// and so has its center at i + 0.5
//
static float SampleLogo(float x, float y)
{
	x -= 0.5f;
	y -= 0.5f;
	if (x < 0 || y < 0 || x >= RETRO_WIDTH - 1 || y >= RETRO_HEIGHT - 1) return 0;
	int ix = (int)x, iy = (int)y;
	float fx = x - ix, fy = y - iy;
	const unsigned char *p = Logo->data + iy * RETRO_WIDTH + ix;
	return ((p[0] * (1 - fx) + p[1] * fx) * (1 - fy)
		+ (p[RETRO_WIDTH] * (1 - fx) + p[RETRO_WIDTH + 1] * fx) * fy) / 255.0f;
}

//
// Shade one sample of the screen at (sx, sy)
//
static vec3 ShadeBall(float sx, float sy)
{
	vec3 n = {(sx - BallX) / Radius, (BallY - sy) / Radius, 0};
	float radius2 = n.x * n.x + n.y * n.y;
	if (radius2 >= 1) return Background;
	n.z = sqrtf(1 - radius2);
	vec3 reflected = reflect(ViewRay, n);

	// The logo panel sits PanelDistance in front of the sphere. Intersecting
	// that plane would leave it out of reach wherever incidence passes 45
	// degrees (beyond R / sqrt(2) from the center), so the reflected direction
	// is projected stereographically onto it instead: the same as the plane
	// hit at the center, but the panel reaches the silhouette and is squeezed
	// there, as it is in a mirror ball. The offset from the surface point,
	// 2 r.xy (PanelDistance - R n.z) / (1 + r.z), reduces to n.xy spread,
	// and the surface point itself is n.xy R from the center.
	float spread = Radius + 2 * (PanelDistance - Radius * n.z) / n.z;
	float ink = SampleLogo(BallX + n.x * spread, BallY - n.y * spread);

	// The navy surroundings reflect more strongly toward the silhouette, which
	// all but disappears into the background as in the reference, and a cobalt
	// crescent lies below the equator. Reflected lettering covers both.
	float band = (n.y + 0.77f) / 0.18f;
	float crescent = expf(-band * band) * (1 - n.x * n.x) * (1 - n.x * n.x);
	vec3 surroundings = {0, 0, 40 * radius2 + 125 * crescent};
	vec3 lettering = vec3{158, 255, 190} * (0.65f + 0.15f * n.z);

	// Finite light discs, not Gaussian dots. A glint is measured by the chord
	// |reflected - light|, which grows linearly with the angle, and its border
	// is ramped over one screen pixel: the chord's change per pixel along x
	// plus along y. With dn.z = -(n.x dn.x + n.y dn.y) / n.z, dn.x = 1 / R
	// along x and dn.y = -1 / R along y, differentiating the reflection gives:
	float dzx = -n.x / n.z, dzy = n.y / n.z;
	vec3 alongx = vec3{n.z + n.x * dzx, n.y * dzx, 2 * n.z * dzx} * (2 / Radius);
	vec3 alongy = vec3{n.x * dzy, -n.z + n.y * dzy, 2 * n.z * dzy} * (2 / Radius);
	float glint = 0;
	for (int i = 0; i < GlintCount; i++) {
		vec3 toward = reflected - GlintLights[i];
		float chord = MAX(length(toward), 1e-6f);
		float perpixel = (fabsf(dot(toward, alongx)) + fabsf(dot(toward, alongy))) / chord;
		// A disc smaller than a pixel would light a whole pixel when centered
		// on one and four dim ones when straddling them, pulsing as the ball
		// glides. So it is drawn at least a pixel wide and dimmed by the width
		// it gained. Dimming by the area would keep its light exactly constant
		// but leaves the small glints visibly fainter than they were meant.
		float size = GlintSizes[i], drawn = MAX(size, perpixel);
		float disc = CLAMP01((drawn - chord) / MAX(perpixel, 1e-6f) + 0.5f);
		glint += disc * size / drawn;
	}

	vec3 white = {255, 255, 255};
	return min(surroundings * (1 - ink) + lettering * ink + white * glint, white);
}

void DEMO_Render(double time, double deltatime)
{
	// Fit to the largest dark component in reference frames sampled at 5 Hz.
	// Fit RMS error: x 2.3 pixels, y 5.4 pixels (occlusion affects y).
	// Start at reference second 10.2, after its introductory title.
	double phase = time + 10.2;
	BallX = 156.9f + 162.2f * sin(phase * 1.045 + 0.6672);
	BallY = 120.5f + 84.9f * sin(phase * 1.566 - 0.7947);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int ink = Logo->data[y * RETRO_WIDTH + x];
			unsigned char color = ink > 24 ? 1 + ink * 63 / 255 : 0;
			// The ball passes behind the lettering. A pixel whose center is
			// more than R + 1 from the ball's has no sample on the disc.
			float dx = (x + 0.5f - BallX) / Radius;
			float dy = (y + 0.5f - BallY) / Radius;
			float distance2 = dx * dx + dy * dy;
			if (ink <= 24 && distance2 < (1 + 1 / Radius) * (1 + 1 / Radius)) {
				// Average actual RGB, not palette indices: this preserves green
				// lettering mixed with blue light and smooths the silhouette
				// too. The squeezed lettering and the lights near the rim get a
				// finer grid.
				int samples = distance2 > 0.45f ? 6 : 5;
				vec3 sum = {0, 0, 0};
				for (int j = 0; j < samples; j++)
					for (int i = 0; i < samples; i++)
						sum += ShadeBall(x + (i + 0.5f) / samples, y + (j + 0.5f) / samples);
				sum = sum / (float)(samples * samples);
				color = ColorLookup[(((int)sum.x >> 3) * 32 + ((int)sum.y >> 3)) * 32 + ((int)sum.z >> 3)];
			}
			RETRO_PutPixel(x, y, color);
		}
	}
}

void DEMO_Initialize(void)
{
	// Blobby lettering in the style of the reference's logo; the palette index
	// is the ink, 255 on the face and 178 on its shade.
	Logo = RETRO_LoadImage("assets/retrologo_320x240.pcx");
	if (Logo->width != RETRO_WIDTH || Logo->height != RETRO_HEIGHT)
		RETRO_RageQuit("Crystal ball logo must match the framebuffer dimensions\n");

	for (int i = 0; i < GlintCount; i++) {
		const Glint &g = Glints[i];
		GlintLights[i] = reflect(ViewRay, vec3{g.nx, g.ny, sqrtf(1 - g.nx * g.nx - g.ny * g.ny)});
		GlintSizes[i] = sqrtf(2 * (1 - g.edge));
	}

	// The palette, in order: the background; 64 mint shades, which the logo
	// indexes directly; 32 blues for the body and the crescent; 33 grays for
	// glints over the black body; and 7 crescent blues each mixed with 18
	// amounts of white for glints over the crescent. Without that last group a
	// glint's border on blue has no near color, and coarse steps there make
	// the small glints flicker as the ball glides.
	RETRO_Palette palette[256];
	int count = 0;
	palette[count++] = {0, 0, 45};
	for (int i = 0; i < 64; i++) {
		float shade = i / 63.0f;
		palette[count++] = {(unsigned char)(158 * shade), (unsigned char)(255 * shade), (unsigned char)(190 * shade)};
	}
	for (int i = 0; i < 32; i++)
		palette[count++] = {0, 0, (unsigned char)(3 + 180 * i / 31)};
	for (int i = 0; i < 33; i++) {
		unsigned char gray = i * 255 / 32;
		palette[count++] = {gray, gray, gray};
	}
	for (int row = 0; row < 7; row++) {
		for (int column = 0; column < 18; column++) {
			float blue = 20 + 140 * row / 6.0f, white = (column + 1) / 19.0f;
			unsigned char gray = (unsigned char)(255 * white);
			palette[count++] = {gray, gray, (unsigned char)(blue + (255 - blue) * white)};
		}
	}
	if (count != 256) RETRO_RageQuit("Crystal ball palette must fill all 256 colors\n");
	RETRO_SetPalette(palette);
	// Prequantize RGB to the shared 8-bit palette; no color searches per frame.
	for (int r = 0; r < 32; r++) for (int g = 0; g < 32; g++) for (int b = 0; b < 32; b++) {
		RETRO_Palette color = {(unsigned char)(r * 8 + 4), (unsigned char)(g * 8 + 4), (unsigned char)(b * 8 + 4)};
		ColorLookup[(r * 32 + g) * 32 + b] = RETRO_NearestPaletteIndex(color, palette);
	}
}
