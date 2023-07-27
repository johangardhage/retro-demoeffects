//
// Scroller (texture) mapped cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retro3d.h"

#define TEXT_HEIGHT 5
#define TEXT_WIDTH 77
#define FONT_HEIGHT 5
#define FONT_WIDTH 5
#define IMAGE_HEIGHT 128
#define IMAGE_WIDTH 128

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
			image[(50 + y) * IMAGE_WIDTH + x] = bitmap[ysrc][xsrc];
		}
	}

	// Draw square
	RETRO_DrawLine(0, 0, IMAGE_WIDTH-1, 0, 60, image, IMAGE_WIDTH); // top row
	RETRO_DrawLine(0, IMAGE_HEIGHT-1, IMAGE_WIDTH-1, IMAGE_HEIGHT-1, 60, image, IMAGE_WIDTH); // bottom row
	RETRO_DrawLine(0, 0, 0, IMAGE_HEIGHT-1, 60, image, IMAGE_WIDTH); // left side
	RETRO_DrawLine(IMAGE_WIDTH-1, 0, IMAGE_WIDTH-1, IMAGE_HEIGHT-1, 60, image, IMAGE_WIDTH); // right side
#if 1
	// Draw second square
	RETRO_DrawLine(0, 1, IMAGE_WIDTH-1, 1, 60, image, IMAGE_WIDTH); // top row
	RETRO_DrawLine(0, IMAGE_HEIGHT-2, IMAGE_WIDTH-1, IMAGE_HEIGHT-2, 60, image, IMAGE_WIDTH); // bottom row
	RETRO_DrawLine(1, 0, 1, IMAGE_HEIGHT-1, 60, image, IMAGE_WIDTH); // left side
	RETRO_DrawLine(IMAGE_WIDTH-2, 0, IMAGE_WIDTH-2, IMAGE_HEIGHT-1, 60, image, IMAGE_WIDTH); // right side
#endif

	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateMatrix(ax, ay, az);
	RETRO_RotateVertices();
	RETRO_RotateVertexNormals();
	RETRO_ProjectVertices(0.5);
	RETRO_SortVisibleFaces();
	RETRO_RenderModel(RETRO_POLY_TEXTURE);
}

void DEMO_Initialize()
{
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_SetColor(i, i, i, i);
	}

	RETRO_CreateCube3Model();
	RETRO_InitializeVertexNormals();

	Model3D *model = RETRO_Get3DModel();
	model->texture = image;

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
