//
// Fixed-bend tunnel using only a 2D texture-coordinate lookup.
// Screen-space circles shrink and drift left. Each pixel uses the first
// contour that leaves it behind, so the inside wall hides the distant bend.
// No vertices, camera, polygons or depth buffer are used, even at startup.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropalette.h"

#define TEXTURE_SIZE 64
#define WALL_TILES 32
#define SCROLL_SPEED 576.0f

struct TunnelPixel {
	unsigned short u, v;
	unsigned char shade;
};

static TunnelPixel Mapping[RETRO_WIDTH * RETRO_HEIGHT];
static unsigned char Texture[TEXTURE_SIZE * TEXTURE_SIZE];
static unsigned char Shading[256][256];

// Parameter s controls a family of circles entirely in screen coordinates.
static float ContourDistance(float x, float y, float s)
{
	float dx = x - (RETRO_WIDTH * 0.70f - 0.45f * s);
	float dy = y - RETRO_HEIGHT * 0.5f;
	float radius = 8640.0f / s;
	return dx * dx + dy * dy - radius * radius;
}

void DEMO_Render(double time, double deltatime)
{
	int scroll = (int)fmod(time * SCROLL_SPEED, TEXTURE_SIZE);
	static const int threshold[2][2] = { { 0, 2 }, { 3, 1 } };
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			int offset = y * RETRO_WIDTH + x;
			const TunnelPixel &p = Mapping[offset];
			int v = (p.v + scroll) & (TEXTURE_SIZE - 1);
			int value = Shading[p.shade][Texture[v * TEXTURE_SIZE + p.u]];
			int level = value / 17;
			if ((value % 17) * 4 > threshold[y & 1][x & 1] * 17 + 8) level++;
			RETRO.framebuffer[offset] = MIN(level, 15) * 17;
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_Palette palette[RETRO_COLORS];
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK,
		RETRO_Palette{ 255, 72, 0 }, palette);
	RETRO_SetPalette(palette);

	for (int y = 0; y < TEXTURE_SIZE; y++) {
		for (int x = 0; x < TEXTURE_SIZE; x++) {
			float u = (x + 0.5f) / TEXTURE_SIZE;
			float v = (y + 0.5f) / TEXTURE_SIZE;
			Texture[y * TEXTURE_SIZE + x] =
				(u > v * 0.5f && u < 1.0f - v * 0.5f) ? 128 : 248;
		}
	}
	for (int shade = 0; shade < 256; shade++) {
		for (int color = 0; color < 256; color++) {
			Shading[shade][color] = color * shade / 255;
		}
	}

	for (int y = 0; y < RETRO_HEIGHT; y++) {
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// Find the FIRST crossing: distant circles can overlap again on
			// the inside of the bend. A single global bisection misses this.
			float low = 1.0f, high = 1.0f;
			while (high < 2048.0f && ContourDistance(x + 0.5f, y + 0.5f, high) < 0.0f) {
				low = high;
				high += 0.25f;
			}
			for (int step = 0; step < 16; step++) {
				float mid = (low + high) * 0.5f;
				if (ContourDistance(x + 0.5f, y + 0.5f, mid) < 0.0f) low = mid;
				else high = mid;
			}
			float s = (low + high) * 0.5f;
			float dx = x + 0.5f - (RETRO_WIDTH * 0.70f - 0.45f * s);
			float dy = y + 0.5f - RETRO_HEIGHT * 0.5f;
			float angle = atan2f(dy, dx);
			float light = 0.40f + 0.60f * MAX(0.0f, cosf(angle));
			float fog = 1.0f / (1.0f + 0.000002f * s * s);
			TunnelPixel &p = Mapping[y * RETRO_WIDTH + x];
			p.u = (int)((angle + M_PI) * WALL_TILES * TEXTURE_SIZE / (2.0f * M_PI)) & (TEXTURE_SIZE - 1);
			p.v = (int)(s * TEXTURE_SIZE / 8.0f) & (TEXTURE_SIZE - 1);
			p.shade = (unsigned char)(255.0f * light * fog);
		}
	}
}
