//
// Burning wireframe cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

void DEMO_Render2(double deltatime)
{
	// Advance the trail in fixed steps. It is one blur pass per step into a framebuffer
	// that is never cleared, so its length follows the step rate.
	while (RETRO_PerformSimulation()) {
		static float ax, ay, az;
		ax += RETRO_SIMULATION_STEP * 2;
		ay += RETRO_SIMULATION_STEP * 2;
		az += RETRO_SIMULATION_STEP * 2;

		RETRO_RotateModel(ax, ay, az);
		RETRO_ProjectModel();
		RETRO_RenderModel(RETRO_POLY_WIREFRAME, RETRO_SHADE_WIREFIRE);

		RETRO_Blur(RETRO_BLUR_FIRE, 3);
	}
	RETRO_Flip();
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_BLACK);
	RETRO_CreateGradientPalette(0, 63, RETRO_BLACK, RETRO_RED);
	RETRO_CreateGradientPalette(63, 127, RETRO_RED, RETRO_YELLOW);
	RETRO_CreateGradientPalette(127, 190, RETRO_YELLOW, RETRO_WHITE);

	Model3D *model = RETRO_Load3DModel("assets/cube4.obj");
	model->c = 80;
	model->cintensity = 100;
}
