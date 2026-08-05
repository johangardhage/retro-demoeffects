//
// Burning wireframe cube
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

//
// Advance the trail in fixed steps. It is one blur pass per step into a framebuffer
// that is never cleared, so its length follows the step rate.
//
void DEMO_Update(double deltatime)
{
	static float ax, ay, az;
	ax += deltatime * 2;
	ay += deltatime * 2;
	az += deltatime * 2;

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_WIREFRAME, RETRO_SHADE_WIREFIRE);

	RETRO_Blur(RETRO_BLUR_FIRE, 3);
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
