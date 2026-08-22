//
// Environment mapped cube
//
// Faces are not lit; they are looked up. A phong map P is the front disk of a
// shiny ball under a light, built once. Pixel with unit normal N reads
//
//   P(W/2 + I * Nx,  H/2 + I * Ny)
//
// A normal facing the viewer lands in the middle of the map; a grazing one
// reaches I = W/2 from it. That is a lighting map, not a Blinn/Newell
// reflection (those use SHADE_ENVIRONMENT). The highlight sits still while
// the cube turns under it. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define PHONGMAP_SIZE 512 // the map is square, and this is its side in pixels
#define PHONG_FALLOFF 30 // how tight the highlight is; the library's own default is far tighter
#define ROTATION_SPEED 2 // radians a second, about each axis

static unsigned char PhongMap[PHONGMAP_SIZE * PHONGMAP_SIZE];

void DEMO_Render(double deltatime)
{
	// Calculate rotation
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED, 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED, 2 * M_PI);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel();
	RETRO_RenderModel(RETRO_POLY_ENVIRONMENT, RETRO_SHADE_PHONG);
}

void DEMO_Initialize(void)
{
	// Init map
	RETRO_CreatePlasticPhongPalette(PHONG_FALLOFF);
	RETRO_CreatePhongMap(PhongMap, PHONGMAP_SIZE, PHONGMAP_SIZE);

	// Load model
	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->envmap = PhongMap;
	model->envmapwidth = PHONGMAP_SIZE;
	model->envmapheight = PHONGMAP_SIZE;
	model->envmapradius = PHONGMAP_SIZE / 2;
}
