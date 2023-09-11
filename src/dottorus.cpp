//
// dottorus.cpp
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retrogfx.h"

#define NUM_TORUS 5
#define K 15
#define BLUR 2
#define NUM_COLORS 163
#define SCALE 1
#define SINE_VALUES 720

// Big 5x5 torus block
unsigned char Form[NUM_TORUS][NUM_TORUS] = {
	{K * 0, K * 3, K * 3, K * 3, K * 0},
	{K * 2, K * 4, K * 4, K * 4, K * 2},
	{K * 3, K * 4, K * 5, K * 4, K * 3},
	{K * 2, K * 4, K * 4, K * 4, K * 2},
	{K * 0, K * 3, K * 3, K * 3, K * 0}
};

float SinTable[SINE_VALUES];
float CosTable[SINE_VALUES];

void DEMO_Render2(double deltatime)
{
	// Calculate frame
	static double frame_counter = 0;
	frame_counter += deltatime * 200;
	int frame = frame_counter;

	Model3D *model = RETRO_Get3DModel();
	Vertex *vertex = model->vertex;

	// Draw blob
	for (int b = 0; b < BLUR; b++) {
		for (int p = 0; p < model->vertices; p++) {
			RETRO_RotateVertex(&vertex[p], CosTable[(frame + b) % SINE_VALUES], SinTable[(frame + b) % SINE_VALUES]);
			RETRO_ProjectVertex(&vertex[p], SCALE);

			for (int y = 0; y < NUM_TORUS; y++) {
				for (int x = 0; x < NUM_TORUS; x++) {
					unsigned char color = RETRO_GetPixel(vertex[p].sx + x, model->vertex[p].sy + y) + Form[x][y];

					if (color > NUM_COLORS) {
						color = NUM_COLORS;
					}

					RETRO_PutPixel(vertex[p].sx + x, vertex[p].sy + y, color);
				}
			}
		}
	}

	RETRO_Blur(RETRO_BLUR_4, 3);
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init sine table
	for (int i = 0; i < SINE_VALUES; i++) {
		SinTable[i] = sin(i * 2 * M_PI / SINE_VALUES) * 0.7;
		CosTable[i] = cos(i * 2 * M_PI / SINE_VALUES);
	}

	// Init palette
	for (int i = 0; i < NUM_COLORS; i++) {
		unsigned char r = NUM_COLORS * exp(2 * log((double) i / (NUM_COLORS - 10)));
		unsigned char g = NUM_COLORS * exp(7 * log((double) i / (NUM_COLORS - 10)));
		unsigned char b = NUM_COLORS * exp(7 * log((double) i / (NUM_COLORS - 10)));
		RETRO_SetColor(i, r, g, b);
	}

	RETRO_Load3DModel("assets/torus.obj");
}
