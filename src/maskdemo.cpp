//
// Mask demo
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retropoly.h"
#include "lib/retrocolor.h"
#include "lib/retropoly.h"
#include "lib/retro3d.h"
#include "lib/retrofont.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256

enum { ASSET_TEXMAP, ASSET_ENVMAP, ASSET_PHONGMAP, ASSET_MINIPHONGMAP, ASSET_BUMPMAP };

void DEMO_Render(double deltatime)
{
	static RETRO_POLY_TYPE rendertype = RETRO_POLY_TEXTURE;
	static RETRO_POLY_SHADE shadertype = RETRO_SHADE_TABLE;
	static unsigned char *texmap = RETRO_ImageData(ASSET_TEXMAP);
	static unsigned char *envmap = NULL;
	static unsigned char *bumpmap = NULL;
	static unsigned char color = 64;
	static unsigned char colorintensity = 0;

	static bool bumpmapping = false;
	static bool rotate = true;
	static bool usage = true;

	if (RETRO_KeyPressed(SDL_SCANCODE_H)) {
		usage = (usage == false);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_R)) {
		rotate = (rotate == false);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_B)) {
		bumpmapping = (bumpmapping == false);
		bumpmap = bumpmapping ? RETRO_ImageData(ASSET_BUMPMAP) : NULL;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_T)) {
		rendertype = RETRO_POLY_TEXTURE;
		shadertype = RETRO_SHADE_TABLE;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = NULL;
		color = 64;
		colorintensity = 0;
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_1)) {
		rendertype = RETRO_POLY_TEXTURE;
		shadertype = RETRO_SHADE_FLAT;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = NULL;
		color = 0;
		colorintensity = 0;
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_2)) {
		rendertype = RETRO_POLY_TEXTURE;
		shadertype = RETRO_SHADE_GOURAUD;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = NULL;
		color = 0;
		colorintensity = 128;
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_3)) {
		rendertype = RETRO_POLY_TEXTURE;
		shadertype = RETRO_SHADE_ENVIRONMENT;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = RETRO_ImageData(ASSET_MINIPHONGMAP);
		color = 128;
		colorintensity = 90;
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_D)) {
		rendertype = RETRO_POLY_DOT;
		shadertype = RETRO_SHADE_NONE;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = NULL;
		color = 255;
		colorintensity = 0;
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_W)) {
		rendertype = RETRO_POLY_WIREFIRE;
		shadertype = RETRO_SHADE_NONE;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = NULL;
		color = 255;
		colorintensity = 0;
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_P)) {
		rendertype = RETRO_POLY_ENVIRONMENT;
		envmap = RETRO_ImageData(ASSET_PHONGMAP);
		color = 128;
		colorintensity = 90;
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_PHONGMAP));
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_O)) {
		rendertype = RETRO_POLY_ENVIRONMENT;
		envmap = RETRO_ImageData(ASSET_MINIPHONGMAP);
		color = 128;
		colorintensity = 90;
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_PHONGMAP));
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_M)) {
		rendertype = RETRO_POLY_ENVIRONMENT;
		envmap = RETRO_ImageData(ASSET_ENVMAP);
		color = 128;
		colorintensity = 90;
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_ENVMAP));
	}

	Model3D *model = RETRO_Get3DModel();
	model->texmap = texmap;
	model->envmap = envmap;
	model->bumpmap = bumpmap;
	model->c = color;
	model->cintensity = colorintensity;

	static float ax = 145, ay = 90, az = 90, distance = 0.5;
	if (rotate) {
		ax += deltatime * 1;
		ay += deltatime * 1;
		az += deltatime * 1;
	}

	if (RETRO_KeyState(SDL_SCANCODE_I)) {
		rotate = false;
		ax += 1 * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_K)) {
		rotate = false;
		ax -= 1 * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_X)) {
		rotate = false;
		ay += 1 * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_Z)) {
		rotate = false;
		ay -= 1 * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_J)) {
		rotate = false;
		az += 1 * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_L)) {
		rotate = false;
		az -= 1 * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_COMMA)) {
		distance += 1 * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_PERIOD)) {
		distance -= 1 * deltatime;
	}

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(distance);
	RETRO_SortVisibleFaces();
	RETRO_RenderModel(rendertype, shadertype);

	if (usage) {
		RETRO_PutString("Runtime controls:", 0, 10, 255);
		RETRO_PutString("r to toggle rotation", 0, 30, 255);
		RETRO_PutString(", and . to change object distance", 0, 40, 255);
		RETRO_PutString("i and k to change x rotation", 0, 50, 255);
		RETRO_PutString("j and l to change y rotation", 0, 60, 255);
		RETRO_PutString("z and x to change z rotation", 0, 70, 255);
		RETRO_PutString("d to use dots", 0, 90, 255);
		RETRO_PutString("w to use wireframe", 0, 100, 255);
		RETRO_PutString("t to use texture mapping", 0, 110, 255);
		RETRO_PutString("1 to use flat shaded texture mapping", 0, 120, 255);
		RETRO_PutString("2 to use gouraud shaded texture mapping", 0, 130, 255);
		RETRO_PutString("3 to use phong shaded texture mapping", 0, 140, 255);
		RETRO_PutString("m to use metal environment mapping", 0, 150, 255);
		RETRO_PutString("p to use phong environment mapping", 0, 160, 255);
		RETRO_PutString("b to toggle bumpmapping", 0, 170, 255);
		RETRO_PutString("h to toggle this help screen", 0, 190, 255);
	}
}

void DEMO_Initialize(void)
{
	// Load assets
	RETRO_LoadImage("assets/mask_texmap_256x256.pcx");
	RETRO_LoadImage("assets/mask_envmap_256x256.pcx");
	RETRO_LoadImage("assets/mask_phongmap_256x256.pcx");
	RETRO_LoadImage("assets/mask_miniphongmap_256x256.pcx");
	RETRO_LoadImage("assets/mask_bumpmap_256x256.pcx");

	// Create optimal palette from texture palette
	RETRO_CreateOptimalPaletteAndShadeTable(RETRO_ImagePalette(ASSET_TEXMAP), 32);
	RETRO_Set6bitPalette(RETRO_OptimalPalette());

	// Load model
	RETRO_Load3DModel("assets/mask.obj", 1);

	// Set up light source
	RETRO_InitializeLightSource(0, 0, -1);
}
