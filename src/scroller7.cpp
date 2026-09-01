//
// Scroller cube, using the shared 16x16 font
//
// Text from font_16x16.pcx is assembled into a horizontal strip and copied
// into the middle of a 256x256 texture. The texture is mapped in full onto
// every face of a rotating cube, so the same scroller wraps around it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define FONT_SCALE 4
#define GLYPH_WIDTH (FONT_WIDTH * FONT_SCALE)
#define GLYPH_HEIGHT (FONT_HEIGHT * FONT_SCALE)
#define GLYPH_ADVANCE (GLYPH_WIDTH + 2 * FONT_SCALE) // proportional inter-letter space
#define FONT_IMAGE_WIDTH 944
#define IMAGE_WIDTH 256
#define IMAGE_HEIGHT 256
#define SCROLL_TEXT "       RETRO DEMOEFFECTS...   "
#define SCROLL_LENGTH (sizeof(SCROLL_TEXT) - 1)
#define SCROLL_WIDTH (GLYPH_ADVANCE * SCROLL_LENGTH)
#define SCROLL_SPEED 200.0 // strip columns per second
#define SCROLL_Y ((IMAGE_HEIGHT - GLYPH_HEIGHT) / 2)
#define ROTATION_SPEED 2.0 // radians per second, about each axis

unsigned char image[IMAGE_WIDTH * IMAGE_HEIGHT];
unsigned char scroll_bitmap[GLYPH_HEIGHT * SCROLL_WIDTH];
int font_dim = 255;
int font_lit = 0;

void DEMO_Render(double deltatime)
{
	static double phase = 0;
	phase = fmod(phase + deltatime * SCROLL_SPEED, SCROLL_WIDTH);
	int iphase = (int)phase;

	memset(image, 0, sizeof(image));
	for (int y = 0; y < GLYPH_HEIGHT; y++) {
		for (int x = 0; x < IMAGE_WIDTH; x++) {
			unsigned char color = scroll_bitmap[y * SCROLL_WIDTH + WRAP(x + iphase, SCROLL_WIDTH)];
			if (color != 0) {
				// Spread the shades used by the font across the cube's colored
				// palette instead of driving them all into white.
				unsigned char texel = 40;
				if (font_lit > font_dim) {
					texel += (color - font_dim) * 200 / (font_lit - font_dim);
				}
				image[(SCROLL_Y + y) * IMAGE_WIDTH + x] = texel;
			}
		}
	}

	static float ax, ay, az;
	ax = fmodf(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmodf(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmodf(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_TEXTURE);

	// Overstroke model-space cube edges so its silhouette stays visible
	// when a textured face becomes nearly edge-on.
	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *v1 = &model->vertex[face->vertex[j]];
			Vertex *v2 = &model->vertex[face->vertex[(j + 1) % face->vertices]];
			int changed = (v1->x != v2->x) + (v1->y != v2->y) + (v1->z != v2->z);
			if (changed != 1) {
				continue;
			}

			float dx = v2->sx - v1->sx;
			float dy = v2->sy - v1->sy;
			float length = sqrtf(dx * dx + dy * dy);
			if (length == 0.0f) {
				continue;
			}
			float ox = -dy / length;
			float oy = dx / length;
			for (int offset = -1; offset <= 1; offset++) {
				RETRO_DrawLine(roundf(v1->sx + ox * offset), roundf(v1->sy + oy * offset),
							   roundf(v2->sx + ox * offset), roundf(v2->sy + oy * offset), 250);
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/font_16x16.pcx", true);
	unsigned char *font = RETRO_ImageData();

	for (int i = 0; i < (int)SCROLL_LENGTH; i++) {
		unsigned char *src = font + (SCROLL_TEXT[i] - 32) * FONT_WIDTH;
		unsigned char *dst = scroll_bitmap + i * GLYPH_ADVANCE;
		for (int y = 0; y < FONT_HEIGHT; y++) {
			for (int x = 0; x < FONT_WIDTH; x++) {
				unsigned char color = src[y * FONT_IMAGE_WIDTH + x];
				for (int dy = 0; dy < FONT_SCALE; dy++) {
					for (int dx = 0; dx < FONT_SCALE; dx++) {
						dst[(y * FONT_SCALE + dy) * SCROLL_WIDTH
							+ x * FONT_SCALE + dx] = color;
					}
				}
				if (color != 0) {
					font_dim = MIN(font_dim, (int)color);
					font_lit = MAX(font_lit, (int)color);
				}
			}
		}
	}

	// Restore the palette after extracting the font.
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_AZURE, RETRO_WHITE);
	RETRO_SetColor(0, RETRO_BLACK);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->texmap = image;
	RETRO_InitializeFaceUVs(model);
}
