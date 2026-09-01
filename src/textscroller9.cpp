//
// Scroller, on a cube
//
// The font is packed into a horizontal strip, copied each frame into the
// middle of a 256×256 texture, and mapped in full onto every face of a
// rotating cube, so the same scroller wraps around it. The cube is drawn
// with RETRO_POLY_TEXTURE; the scroller itself is still
//
//   color = strip[row][(x + phase) mod stripwidth]
//
// Font greys are remapped onto the cube's colour ramp rather than driven
// into white: the darkest atlas ink becomes the low end of the ramp and
// the lightest the high end. Model-space cube edges are overstroked after
// the textured faces so the silhouette stays visible when a face is
// nearly edge-on. phase lives on stripwidth, in columns per second; the
// cube turns about each axis at ROTATION_SPEED.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retrofont.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define FONT RETRO_FontAsset{ "assets/font_16x16.pcx", 16, 16 }
//#define FONT RETRO_FONT_MINECRAFT_8X8
#define FONT_SCALE 4
#define FONT_SPACING 2 // proportional inter-letter space, in unscaled font pixels
#define IMAGE_WIDTH 256
#define IMAGE_HEIGHT 256
#define SCROLL_SPEED 200.0 // strip columns per second
#define ROTATION_SPEED 2.0 // radians per second, about each axis

static const char *const ScrollText[] = { "     RETRO DEMOEFFECTS..." };

RETRO_Image *ScrollImage;
unsigned char Image[IMAGE_WIDTH * IMAGE_HEIGHT];
int ScrollY;
int FontDim = 255;
int FontLit = 0;

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase
	double phase = fmod(time * SCROLL_SPEED, ScrollImage->width);
	int iphase = (int)phase;

	memset(Image, 0, sizeof(Image));
	for (int y = 0; y < ScrollImage->height; y++) {
		for (int x = 0; x < IMAGE_WIDTH; x++) {
			unsigned char color = ScrollImage->data[y * ScrollImage->width + WRAP(x + iphase, ScrollImage->width)];
			if (color != 0) {
				// Spread the shades used by the font across the cube's colored
				// palette instead of driving them all into white.
				unsigned char texel = 40;
				if (FontLit > FontDim) {
					texel += (color - FontDim) * 200 / (FontLit - FontDim);
				}
				Image[(ScrollY + y) * IMAGE_WIDTH + x] = texel;
			}
		}
	}

	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED, 2 * M_PI);

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
	ScrollImage = RETRO_GenerateTextImage(RETRO_LoadFont(FONT), ScrollText, sizeof(ScrollText) / sizeof(ScrollText[0]), FONT_SCALE, FONT_SPACING);
	ScrollY = (IMAGE_HEIGHT - ScrollImage->height) / 2;

	for (int i = 0; i < ScrollImage->width * ScrollImage->height; i++) {
		unsigned char color = ScrollImage->data[i];
		if (color != 0) {
			FontDim = MIN(FontDim, (int)color);
			FontLit = MAX(FontLit, (int)color);
		}
	}

	// Restore the palette after extracting the font.
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_AZURE, RETRO_WHITE);
	RETRO_SetColor(0, RETRO_BLACK);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->texmap = Image;
	RETRO_InitializeFaceUVs(model);
}
