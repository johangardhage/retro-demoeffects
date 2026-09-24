//
// Texture mapped globe
//
// spherequads.obj wrapped in an equirectangular Earth map: u runs around the
// equator and v pole to pole, so each texel row is a parallel and each column
// a meridian. Unlit and perspective-correct like texturecube.cpp; only the
// 16 x 8 facets of the sphere show at the silhouette. The three axes turn at
// unequal rates, so the globe tumbles instead of spinning about one pole.
// Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

#define GLOBE_SCALE 80 // a unit sphere comes out this many pixels in radius
#define ROTATION_SPEED_X 0.7 // radians a second
#define ROTATION_SPEED_Y 1.1 // radians a second
#define ROTATION_SPEED_Z 0.4 // radians a second

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED_X, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED_Y, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED_Z, 2 * M_PI);

	// Draw globe
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(GLOBE_SCALE);
	RETRO_RenderModel(RETRO_POLY_TEXTURE);
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/earthsphere_256x256.pcx", true);

	Model3D *model = RETRO_Load3DModel("assets/spherequads.obj");
	model->texmap = RETRO_ImageData();
}
