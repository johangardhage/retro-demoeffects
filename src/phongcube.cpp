//
// Phong shaded cube
//
// The same eight corner normals as gouraudcube.cpp, stored as n q and
// interpolated in screen space. Normalizing the interpolated n q is the
// same direction as divide-by-q then normalize (q > 0 in front of the
// near plane). The pixel is then
//
//   I = ShadeFractionFromLambert(max(N · L, 0))
//   color = c + intensity · I
//
// with L = (0, 0, −1). The palette falloff is 30: tight enough to read
// as a highlight, broad enough that a per-pixel N can resolve it. Euler
// angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 2 // radians a second, about each axis

void DEMO_Render(RETRO_Time time)
{
	// Calculate rotation
	float ax = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time.total * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(time.total * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_PHONG);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreatePlasticPalette(RETRO_DEEPPINK, 30);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;
}
