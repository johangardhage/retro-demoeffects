//
// Scroller cube
//
// The same strip idea as scroller.cpp, painted into a 256×256 texture
// that is mapped onto a cube. Rows of 5×5 block letters scroll right
// to left; t lives on 5 · 77. The cube is otherwise the stock textured
// path. Cube edges that are axis-aligned in model space are overstroked
// so the silhouette stays visible when a face is nearly edge-on. Euler
// angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define TEXT_HEIGHT 5
#define TEXT_WIDTH 77
#define FONT_HEIGHT 5
#define FONT_WIDTH 5
#define IMAGE_HEIGHT 256
#define IMAGE_WIDTH 256
#define ROTATION_SPEED 2 // radians a second, about each axis

int font[FONT_HEIGHT][FONT_WIDTH] = {{10, 30, 30, 30, 10},
									 {30, 90, 70, 50, 30},
									 {30, 70, 70, 50, 30},
									 {30, 50, 50, 50, 30},
									 {10, 30, 30, 30, 10}};

char text[TEXT_HEIGHT][TEXT_WIDTH] = {{"                            ##  ## ##   #  #   #    ### ### # # ###         "},
					                  {"                           #   #   # # # # #   #     #  #   # #  #          "},
					                  {"                            #  #   ##  # # #   #     #  ##   #   #          "},
					                  {"                             # #   # # # # #   #     #  #   # #  #          "},
					                  {"                           ##   ## # #  #  ### ###   #  ### # #  #          "}};

unsigned char image[IMAGE_WIDTH * IMAGE_HEIGHT];

unsigned char bitmap[FONT_HEIGHT * TEXT_HEIGHT][FONT_WIDTH * TEXT_WIDTH];

void DEMO_Render(double deltatime)
{
	static float phase = 0;
	phase = fmod(phase + deltatime * 100, FONT_WIDTH * TEXT_WIDTH);

	// Move scroller
	for (int x = 0; x < IMAGE_WIDTH; x++) {
		int xsrc = WRAP(phase + x, FONT_WIDTH * TEXT_WIDTH);
		for (int y = 0; y < TEXT_HEIGHT * FONT_HEIGHT; y++) {
			int ysrc = TEXT_HEIGHT * FONT_HEIGHT - (y + 1);
			image[(120 + y) * IMAGE_WIDTH + x] = bitmap[ysrc][xsrc];
		}
	}

	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

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
				float length = sqrt(dx * dx + dy * dy);
				if (length == 0.0f) continue;
				float ox = -dy / length;
				float oy = dx / length;
				for (int offset = 0; offset <= 1; offset++) {
					RETRO_DrawLine(round(v1->sx + ox * offset), round(v1->sy + oy * offset),
								   round(v2->sx + ox * offset), round(v2->sy + oy * offset), 60);
				}
			}
		}
	}
}

void DEMO_Initialize(void)
{
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_WHITE);

	Model3D *model = RETRO_Load3DModel("assets/cube3.obj");
	model->texmap = image;

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
