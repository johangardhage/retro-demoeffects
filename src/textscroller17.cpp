//
// Scroller, on a granite plaque
//
// A granite plaque, a polished metal scroller and the shadow a moving distant
// light casts from it onto the plaque.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_WIDTH 360
#define RETRO_HEIGHT 280
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"
#include "lib/retromath.h"
#include "lib/retromain.h"

#define WALL_WIDTH 198
#define WALL_HEIGHT 126
#define WALL_X ((RETRO_WIDTH - WALL_WIDTH) / 2)
#define WALL_Y ((RETRO_HEIGHT - WALL_HEIGHT) / 2)
#define TEXT_Y 124
#define TEXT_MARGIN 20
#define STONE_START 16
#define STONE_SHADES 128
#define TEXT_START 144
#define TEXT_SHADES 64
// The eye is EYE_DISTANCE in front of the text and the wall WALL_DEPTH behind
// it, so on screen the wall, and the shadow on it, appear at
// EYE_DISTANCE / (EYE_DISTANCE + WALL_DEPTH) = 0.68 of the text's scale.
#define WALL_DEPTH 80.0f
#define EYE_DISTANCE 170.0f

static unsigned char Stone[WALL_HEIGHT][WALL_WIDTH];
static vec3 StoneNormals[WALL_HEIGHT][WALL_WIDTH];
static RETRO_Image *ScrollImage;
static const char *const ScrollText[] = {
	"RETRO DEMOEFFECTS...           "
};

// Value noise, in [-1, 1]
static float Noise(float x, float y)
{
	return 2.0f * RETRO_ValueNoise(x, y) - 1.0f;
}

static bool TextPixel(int x, int y)
{
	if (x < 0 || y < 0 || y >= ScrollImage->height) return false;
	x %= ScrollImage->width;
	return ScrollImage->data[y * ScrollImage->width + x] != 0;
}

// Ink at screen column x. The text exists only between the margins, and so
// does its shadow.
static bool ShadowInk(int x, int y, int phase)
{
	return x >= TEXT_MARGIN && x < RETRO_WIDTH - TEXT_MARGIN && TextPixel(x + phase, y);
}

// Filter the binary silhouette at fractional coordinates. This smooths edge
// coverage, rather than introducing an unrelated blurred drop shadow.
static float ShadowCoverage(float x, float y, int phase)
{
	int ix = (int)floorf(x), iy = (int)floorf(y);
	float fx = x - ix, fy = y - iy;
	float top = mix(ShadowInk(ix, iy, phase), ShadowInk(ix + 1, iy, phase), fx);
	float bottom = mix(ShadowInk(ix, iy + 1, phase), ShadowInk(ix + 1, iy + 1, phase), fx);
	return mix(top, bottom, fy);
}

void DEMO_Render(double time, double deltatime)
{
	unsigned char *dest = RETRO_FrameBuffer();

	// The text enters at the right margin. Until it has scrolled in, phase is
	// negative and the columns left of the text's start stay empty.
	double scroll = time * 76.0 - (RETRO_WIDTH - TEXT_MARGIN);
	int phase = scroll < 0 ? (int)floor(scroll) : (int)fmod(scroll, (double)ScrollImage->width);
	// A distant light, so every ray is parallel. slopex and slopey are how far
	// it leans per unit of depth. A wall point is shaded when the ray from it
	// toward the light meets ink at the text: its point in space, back from
	// the screen through the eye, moved by the lean over the gap to the text.
	float ax = fmod(time * 1.15, 2 * M_PI);
	float ay = fmod(time * 1.63, 2 * M_PI);
	float slopex = 0.88f * sinf(ax);
	float slopey = 1.03f * sinf(ay + 0.5f);
	vec3 light = normalize(vec3{ slopex, slopey, 1 });
	float wallscale = (EYE_DISTANCE + WALL_DEPTH) / EYE_DISTANCE;
	float centerx = RETRO_WIDTH * 0.5f, centery = RETRO_HEIGHT * 0.5f;

	// The plaque is bright stone that the shadow darkens to roughly half.
	// The rim is a bevel in the wall plane: the same light and the same shadow.
	float stoneambient = 0.5f, stonediffuse = 0.62f;
	for (int y = -2; y < WALL_HEIGHT + 2; y++) {
		for (int x = -2; x < WALL_WIDTH + 2; x++) {
			float shadow = ShadowCoverage(centerx + (WALL_X + x - centerx) * wallscale + WALL_DEPTH * slopex,
				centery + (WALL_Y + y - centery) * wallscale + WALL_DEPTH * slopey - TEXT_Y, phase);
			float albedo, ambient, diffuse;
			if (x < 0 || x >= WALL_WIDTH || y < 0 || y >= WALL_HEIGHT) {
				// Steeper than the face, so it catches more light, and the sides
				// turned away go nearly black.
				float nx = x < 0 ? -0.7f : (x >= WALL_WIDTH ? 0.7f : 0);
				float ny = y < 0 ? -0.7f : (y >= WALL_HEIGHT ? 0.7f : 0);
				vec3 normal = normalize(vec3{ nx, ny, 0.4f });
				albedo = 1;
				ambient = 0.1f;
				diffuse = 1.75f * MAX(0.0f, dot(normal, light));
			} else {
				albedo = Stone[y][x] / 63.0f;
				ambient = stoneambient;
				diffuse = MAX(0.0f, dot(StoneNormals[y][x], light));
			}
			float intensity = albedo * (ambient + stonediffuse * diffuse * (1 - shadow));
			dest[(WALL_Y + y) * RETRO_WIDTH + WALL_X + x] =
				STONE_START + (int)(CLAMP01(intensity) * (STONE_SHADES - 1));
		}
	}

	// Polished metal letters: mostly a Blinn-Phong glint, with little diffuse,
	// so letters away from it go nearly black. The glint needs a viewer at a
	// finite distance; with parallel view rays a flat face would shade evenly.
	// A one-pixel bevel tilts outward at the glyph boundary to catch it.
	float textdiffuse = 0.1f, textspecular = 1.25f, shininess = 8.0f;
	for (int y = 0; y < ScrollImage->height; y++) {
		for (int x = TEXT_MARGIN; x < RETRO_WIDTH - TEXT_MARGIN; x++) {
			int sourcex = x + phase;
			if (!TextPixel(sourcex, y)) continue;
			float nx = 0.5f * ((int)TextPixel(sourcex - 1, y) - (int)TextPixel(sourcex + 1, y));
			float ny = 0.5f * ((int)TextPixel(sourcex, y - 1) - (int)TextPixel(sourcex, y + 1));
			vec3 normal = normalize(vec3{ nx, ny, 1 });
			vec3 view = normalize(vec3{ centerx - x, centery - (TEXT_Y + y), EYE_DISTANCE });
			vec3 halfway = normalize(light + view);
			float diffuse = MAX(0.0f, dot(normal, light));
			float specular = powf(MAX(0.0f, dot(normal, halfway)), shininess);
			float intensity = textdiffuse * diffuse + textspecular * specular;
			dest[(TEXT_Y + y) * RETRO_WIDTH + x] = TEXT_START + (int)(CLAMP01(intensity) * (TEXT_SHADES - 1));
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_RGB(0x203363));
	for (int i = 0; i < STONE_SHADES; i++) {
		int c = i * 255 / (STONE_SHADES - 1);
		RETRO_SetColor(STONE_START + i, c, c * 0.88f, c * 0.87f);
	}
	// Metal ramps from black up to a cool, blue-white highlight.
	for (int i = 0; i < TEXT_SHADES; i++) {
		float t = i / (float)(TEXT_SHADES - 1);
		RETRO_SetColor(TEXT_START + i, 236 * t, 244 * t, 255 * t);
	}
	// Speckled granite: soft mottled patches under fine grain, with sparse
	// dark crystals and pale flecks.
	for (int y = 0; y < WALL_HEIGHT; y++) {
		for (int x = 0; x < WALL_WIDTH; x++) {
			float mottle = Noise(x * 0.045f, y * 0.045f) + 0.5f * Noise(x * 0.11f + 40, y * 0.11f);
			float grain = Noise(x * 1.1f, y * 1.1f) + 0.7f * Noise(x * 2.3f + 17, y * 2.3f);
			float crystals = Noise(x * 0.55f + 90, y * 0.55f);
			float flecks = Noise(x * 0.8f + 150, y * 0.8f);
			float shade = 0.66f + 0.21f * mottle + 0.07f * grain;
			if (crystals < -0.6f) shade -= 0.22f;
			if (flecks > 0.65f) shade += 0.2f;
			Stone[y][x] = (unsigned char)(CLAMP01(shade) * 63);
		}
	}
	// Smooth the texture-derived height before taking slopes, so the fine
	// albedo grain does not become harsh, sparkling bumps. Geometry stays flat.
	static float height[WALL_HEIGHT][WALL_WIDTH];
	for (int y = 0; y < WALL_HEIGHT; y++) {
		for (int x = 0; x < WALL_WIDTH; x++) {
			float sum = 0;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					sum += Stone[CLAMP(y + dy, 0, WALL_HEIGHT)][CLAMP(x + dx, 0, WALL_WIDTH)];
				}
			}
			height[y][x] = sum / (9.0f * 63.0f);
		}
	}
	for (int y = 0; y < WALL_HEIGHT; y++) {
		for (int x = 0; x < WALL_WIDTH; x++) {
			int left = CLAMP(x - 1, 0, WALL_WIDTH), right = CLAMP(x + 1, 0, WALL_WIDTH);
			int up = CLAMP(y - 1, 0, WALL_HEIGHT), down = CLAMP(y + 1, 0, WALL_HEIGHT);
			float nx = (height[y][left] - height[y][right]) * 1.5f;
			float ny = (height[up][x] - height[down][x]) * 1.5f;
			StoneNormals[y][x] = normalize(vec3{ nx, ny, 1.0f });
		}
	}
	RETRO_Font font = RETRO_LoadFont(RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 });
	ScrollImage = RETRO_GenerateTextImage(font, ScrollText, 1, 2);
}
