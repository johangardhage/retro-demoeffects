//
// Scroller (texture) mapped cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

#define TEXT_HEIGHT 5
#define TEXT_WIDTH 77
#define FONT_HEIGHT 5
#define FONT_WIDTH 5
#define IMAGE_HEIGHT 256
#define IMAGE_WIDTH 256

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
	static float frame;
	frame += deltatime * 100;

	// Move scroller
	for (int x = 0; x < IMAGE_WIDTH; x++) {
		int xsrc = (int)(frame + x) % (FONT_WIDTH * TEXT_WIDTH);
		for (int y = 0; y < TEXT_HEIGHT * FONT_HEIGHT; y++) {
			int ysrc = TEXT_HEIGHT * FONT_HEIGHT - (y + 1);
			image[(120 + y) * IMAGE_WIDTH + x] = bitmap[ysrc][xsrc];
		}
	}

	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

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
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i, i, i);
	}

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
