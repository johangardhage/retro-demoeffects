//
// Texture mapped cube
//
// Unlit texels, perspective-correct. The drawer interpolates the
// homogeneous pair
//
//   (u q, v q, q)
//
// and recovers (u, v) = (uq/q, vq/q) at each pixel. Affine (u, v) would
// warp the picture because perspective is not linear in screen x, y.
// There is no light and no bump: SHADE_NONE has nothing to modulate.
// Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

#define ROTATION_SPEED 2 // radians a second, about each axis

void DEMO_Render(double deltatime)
{
	// Rotate
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_TEXTURE);
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/cube_256x256.pcx", true);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->texmap = RETRO_ImageData();
}
