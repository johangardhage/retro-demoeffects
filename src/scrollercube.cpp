//
// Scroller cube
//
// The same strip idea as scroller.cpp, painted into a 256×256 texture
// that is mapped onto a cube. Rows of 5×5 block letters, each source row
// stamped twice so the text is twice as tall as it is wide, scroll right
// to left; phase lives on 5 · 126.
//
// The cube's own UVs are a cross atlas, which would hand each face a
// different quarter of the texture, so RETRO_InitializeFaceUVs replaces
// them with the whole texture on every face, in frames that follow the face
// normals rather than the order the corners are listed in. That is what
// makes the strip sit the same way up on all four sides, and it puts the
// texture's rows down the screen, so the strip is stamped unflipped.
//
// Cube edges that are axis-aligned in model space are overstroked, so the
// silhouette stays visible when a face is nearly edge-on and the texture
// border becomes subpixel. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define TEXT_HEIGHT 5
#define TEXT_WIDTH 126
#define FONT_HEIGHT 5
#define FONT_WIDTH 5
#define IMAGE_HEIGHT 256
#define IMAGE_WIDTH 256
#define SCROLL_SPEED 110 // texels per second
#define SCROLL_Y ((IMAGE_HEIGHT - TEXT_HEIGHT * FONT_HEIGHT * 2) / 2)
#define ROTATION_SPEED 2 // radians a second, about each axis

int font[FONT_HEIGHT][FONT_WIDTH] = {{10, 30, 30, 30, 10},
									 {30, 90, 70, 50, 30},
									 {30, 70, 70, 50, 30},
									 {30, 50, 50, 50, 30},
									 {10, 30, 30, 30, 10}};

char text[TEXT_HEIGHT][TEXT_WIDTH] = {{"                     ###  #### ###  ###   ##     ###  #### #   #  ##  #### #### #### ####  ##  ###   ####                    "},
					                  {"                     #  # #     #   #  # #  #    #  # #    ## ## #  # #    #    #    #    #  #  #   #                        "},
					                  {"                     ###  ###   #   ###  #  #    #  # ###  # # # #  # ###  ###  ###  ###  #     #    ###                     "},
					                  {"                     # #  #     #   # #  #  #    #  # #    #   # #  # #    #    #    #    #  #  #       #                    "},
					                  {"                     #  # ####  #   #  #  ##     ###  #### #   #  ##  #### #    #    ####  ##   #   ####                     "}};

unsigned char image[IMAGE_WIDTH * IMAGE_HEIGHT];
unsigned char bitmap[FONT_HEIGHT * TEXT_HEIGHT][FONT_WIDTH * TEXT_WIDTH];

void DEMO_Render(double deltatime)
{
	// Calculate phase
	static float phase = 0;
	phase = fmodf(phase + deltatime * SCROLL_SPEED, FONT_WIDTH * TEXT_WIDTH);
	int iphase = (int)phase;

	// Move scroller. A zero texel is background, so the letters slide over it
	memset(image, 0, sizeof(image));

	for (int x = 0; x < IMAGE_WIDTH; x++) {
		int xsrc = WRAP(iphase + x, FONT_WIDTH * TEXT_WIDTH);

		for (int y = 0; y < TEXT_HEIGHT * FONT_HEIGHT; y++) {
			unsigned char texel = bitmap[y][xsrc];
			if (texel == 0) continue;

			unsigned char color = CLAMP256(100 + texel * 1.5f);
			image[(SCROLL_Y + y * 2) * IMAGE_WIDTH + x] = color;
			image[(SCROLL_Y + y * 2 + 1) * IMAGE_WIDTH + x] = color;
		}
	}

	static float ax, ay, az;
	ax = fmodf(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmodf(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmodf(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_TEXTURE);

	// Keep the cube outline visible when the texture border becomes subpixel
	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < model->visiblefaces; i++) {
		Face *face = &model->face[model->visibleface[i]];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *v1 = &model->vertex[face->vertex[j]];
			Vertex *v2 = &model->vertex[face->vertex[(j + 1) % face->vertices]];
			int changedCoordinates = (v1->x != v2->x) + (v1->y != v2->y) + (v1->z != v2->z);
			if (changedCoordinates == 1) {
				float dx = v2->sx - v1->sx;
				float dy = v2->sy - v1->sy;
				float length = sqrtf(dx * dx + dy * dy);
				if (length == 0.0f) continue;
				float ox = -dy / length;
				float oy = dx / length;
				// Centred, since which way (-dy, dx) points follows the winding
				for (int offset = -1; offset <= 1; offset++) {
					RETRO_DrawLine(roundf(v1->sx + ox * offset), roundf(v1->sy + oy * offset),
								   roundf(v2->sx + ox * offset), roundf(v2->sy + oy * offset), 250);
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_AZURE, RETRO_WHITE);
	RETRO_SetColor(0, RETRO_BLACK);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->texmap = image;

	RETRO_InitializeFaceUVs(model);

	// Init scroller bitmap
	for (int ty = 0; ty < TEXT_HEIGHT; ty++) {
		for (int tx = 0; tx < TEXT_WIDTH; tx++) {
			if (text[ty][tx] == '#') {
				for (int fy = 0; fy < FONT_HEIGHT; fy++) {
					for (int fx = 0; fx < FONT_WIDTH; fx++) {
						bitmap[ty * FONT_HEIGHT + fy][tx * FONT_WIDTH + fx] = font[fy][fx];
					}
				}
			}
		}
	}
}
