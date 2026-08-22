//
// Mask demo
//
// One mesh, many materials. Each key selects a renderer and the maps it
// reads: texture, shade table, phong/env lookup, bump height. The maths of
// each path live in retropoly.h; this file only chooses the inputs.
//
// Shade-table level 0 sits at 33° of incidence, on the shoulder of the
// highlight: high enough that the material shows, low enough that the
// untinted specular does not wash the texture out. Halfway up the ramp would
// be 44°, past the 45° cutoff where every specular term is zero.
//
// H is the height difference that tilts to grazing; larger H is shallower.
// A bare phong map has only the sheen to spend, so a bump uses 3/2 H. A
// metal environment map is a Blinn/Newell sphere map of the reflection of
// V about N, so a tilt lands on a different part of the photo rather than
// a neighbouring shade; that bump uses 2H (half the default tilt). Euler
// angles live on 2π. The mesh starts at ax = −π/2, az = π so the face
// is upright.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"
#include "lib/retrofont.h"

#define TEXTURE_WIDTH 256
#define TEXTURE_HEIGHT 256
#define ROTATION_SPEED 1 // radians a second, about each axis

enum { ASSET_TEXMAP, ASSET_ENVMAP, ASSET_PHONGMAP, ASSET_MINIPHONGMAP, ASSET_BUMPMAP };
enum { MATERIAL_FLAT, MATERIAL_GOURAUD, MATERIAL_PHONG, MATERIALS };

static unsigned char MaterialShadeTables[MATERIALS][RETRO_MAX_SHADING_COLORS];

void DEMO_Render(double deltatime)
{
	static RETRO_POLY_TYPE rendertype = RETRO_POLY_TEXTURE;
	static RETRO_POLY_SHADE shadertype = RETRO_SHADE_NONE;
	static unsigned char *texmap = RETRO_ImageData(ASSET_TEXMAP);
	static unsigned char *shadetable = NULL;
	static unsigned char *envmap = NULL;
	static unsigned char *bumpmap = NULL;
	static unsigned char color = 64;
	static int envmapradius = 0;
	static int bumpgrazing = RETRO_BUMP_GRAZING;

	static bool bumpmapping = false;
	static bool rotate = true;
	static bool usage = true;

	// Handle keys
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
		shadertype = RETRO_SHADE_NONE;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		shadetable = NULL;
		envmap = NULL;
		color = 0;
		envmapradius = 0;
		bumpgrazing = RETRO_BUMP_GRAZING;
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_TEXMAP));
		RETRO_SetColor(0, RETRO_BLACK);
		RETRO_SetColor(255, RETRO_PERIWINKLE);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_0)) {
		rendertype = RETRO_POLY_TEXTURE;
		shadertype = RETRO_SHADE_TABLE;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = NULL;
		color = RETRO_SHADES * 5 / 8;
		envmapradius = 0;
		bumpgrazing = RETRO_BUMP_GRAZING;
		shadetable = MaterialShadeTables[MATERIAL_GOURAUD];
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_1)) {
		rendertype = RETRO_POLY_TEXTURE;
		shadertype = RETRO_SHADE_FLAT;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = NULL;
		color = 0;
		envmapradius = 0;
		bumpgrazing = RETRO_BUMP_GRAZING;
		shadetable = MaterialShadeTables[MATERIAL_FLAT];
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_2)) {
		rendertype = RETRO_POLY_TEXTURE;
		shadertype = RETRO_SHADE_GOURAUD;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = NULL;
		color = 0;
		envmapradius = 0;
		bumpgrazing = RETRO_BUMP_GRAZING;
		shadetable = MaterialShadeTables[MATERIAL_GOURAUD];
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_3)) {
		rendertype = RETRO_POLY_TEXTURE;
		shadertype = RETRO_SHADE_PHONG;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		envmap = RETRO_ImageData(ASSET_MINIPHONGMAP);
		color = 128;
		envmapradius = 90;
		bumpgrazing = RETRO_BUMP_GRAZING;
		shadetable = MaterialShadeTables[MATERIAL_PHONG];
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_D)) {
		rendertype = RETRO_POLY_DOT;
		shadertype = RETRO_SHADE_NONE;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		shadetable = NULL;
		envmap = NULL;
		color = 255;
		envmapradius = 0;
		bumpgrazing = RETRO_BUMP_GRAZING;
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_W)) {
		rendertype = RETRO_POLY_WIREFRAME;
		shadertype = RETRO_SHADE_NONE;
		texmap = RETRO_ImageData(ASSET_TEXMAP);
		shadetable = NULL;
		envmap = NULL;
		color = 255;
		envmapradius = 0;
		bumpgrazing = RETRO_BUMP_GRAZING;
		RETRO_Set6bitPalette(RETRO_OptimalPalette());
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_P)) {
		rendertype = RETRO_POLY_ENVIRONMENT;
		shadertype = RETRO_SHADE_PHONG;
		shadetable = NULL;
		envmap = RETRO_ImageData(ASSET_PHONGMAP);
		color = 128;
		envmapradius = 90;
		bumpgrazing = RETRO_BUMP_GRAZING * 3 / 2;
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_PHONGMAP));
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_O)) {
		rendertype = RETRO_POLY_ENVIRONMENT;
		shadertype = RETRO_SHADE_PHONG;
		shadetable = NULL;
		envmap = RETRO_ImageData(ASSET_MINIPHONGMAP);
		color = 128;
		envmapradius = 90;
		bumpgrazing = RETRO_BUMP_GRAZING * 3 / 2;
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_PHONGMAP));
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_M)) {
		rendertype = RETRO_POLY_ENVIRONMENT;
		shadertype = RETRO_SHADE_ENVIRONMENT;
		shadetable = NULL;
		envmap = RETRO_ImageData(ASSET_ENVMAP);
		color = 128;
		envmapradius = 90;
		bumpgrazing = RETRO_BUMP_GRAZING * 2;
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_ENVMAP));
	}

	// Update model
	Model3D *model = RETRO_Get3DModel();
	model->texmap = texmap;
	model->shadetable = shadetable;
	model->envmap = envmap;
	model->bumpmap = bumpmap;
	model->c = color;
	model->envmapradius = envmapradius;
	model->bumpgrazing = bumpgrazing;

	// Start with the mask's authored face upright before rotating all three axes.
	static float ax = -M_PI / 2, ay = 0, az = M_PI, distance = 0.5;
	if (rotate) {
		ax += deltatime * ROTATION_SPEED;
		ay += deltatime * ROTATION_SPEED;
		az += deltatime * ROTATION_SPEED;
	}

	if (RETRO_KeyState(SDL_SCANCODE_I)) {
		rotate = false;
		ax += ROTATION_SPEED * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_K)) {
		rotate = false;
		ax -= ROTATION_SPEED * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_X)) {
		rotate = false;
		ay += ROTATION_SPEED * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_Z)) {
		rotate = false;
		ay -= ROTATION_SPEED * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_J)) {
		rotate = false;
		az += ROTATION_SPEED * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_L)) {
		rotate = false;
		az -= ROTATION_SPEED * deltatime;
	}
	ax = fmod(ax, 2 * M_PI);
	if (ax < 0) ax += 2 * M_PI;
	ay = fmod(ay, 2 * M_PI);
	if (ay < 0) ay += 2 * M_PI;
	az = fmod(az, 2 * M_PI);
	if (az < 0) az += 2 * M_PI;
	if (RETRO_KeyState(SDL_SCANCODE_COMMA)) {
		distance += 1 * deltatime;
	}
	if (RETRO_KeyState(SDL_SCANCODE_PERIOD)) {
		distance -= 1 * deltatime;
	}

	// Draw model
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(distance);
	RETRO_RenderModel(rendertype, shadertype);

	// Draw help
	if (usage) {
		RETRO_PutString("Runtime controls:", 0, 10, 255);
		RETRO_PutString("r to toggle rotation", 0, 30, 255);
		RETRO_PutString(", and . to change object distance", 0, 40, 255);
		RETRO_PutString("i and k to change x rotation", 0, 50, 255);
		RETRO_PutString("x and z to change y rotation", 0, 60, 255);
		RETRO_PutString("j and l to change z rotation", 0, 70, 255);
		RETRO_PutString("d to use dots", 0, 90, 255);
		RETRO_PutString("w to use wireframe", 0, 100, 255);
		RETRO_PutString("t to use texture mapping", 0, 110, 255);
		RETRO_PutString("0 to use shade table texture mapping", 0, 120, 255);
		RETRO_PutString("1 to use flat shaded texture mapping", 0, 130, 255);
		RETRO_PutString("2 to use gouraud shaded texture mapping", 0, 140, 255);
		RETRO_PutString("3 to use env shaded texture mapping", 0, 150, 255);
		RETRO_PutString("m to use metal environment mapping", 0, 160, 255);
		RETRO_PutString("p to use phong environment mapping", 0, 170, 255);
		RETRO_PutString("o to use mini phong environment mapping", 0, 180, 255);
		RETRO_PutString("b to toggle bumpmapping", 0, 190, 255);
		RETRO_PutString("h to toggle this help screen", 0, 210, 255);
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

	// Init palette. One palette, three shade tables. Flat: a strong, moderately focused
	// highlight. Gouraud: the full plastic highlight, sampled at vertices.
	// Phong: the library's default falloff.
	RETRO_Palette *texturepalette = RETRO_ImagePalette(ASSET_TEXMAP);
	RETRO_CreateOptimalPalette(texturepalette, RETRO_TEXTURE_COLORS, RETRO_K_SPECULAR, 5.0f);
	RETRO_CreateShadeTable(texturepalette, RETRO_TEXTURE_COLORS, RETRO_K_SPECULAR, 5.0f, MaterialShadeTables[MATERIAL_GOURAUD]);
	RETRO_CreateShadeTable(texturepalette, RETRO_TEXTURE_COLORS, 0.6f, 5.0f, MaterialShadeTables[MATERIAL_FLAT]);
	RETRO_CreateShadeTable(texturepalette, RETRO_TEXTURE_COLORS, RETRO_K_SPECULAR, RETRO_K_FALLOFF, MaterialShadeTables[MATERIAL_PHONG]);
	RETRO_Set6bitPalette(texturepalette);
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(255, RETRO_PERIWINKLE);

	// Load model
	RETRO_Load3DModel("assets/mask.obj");

	// Set up light source
	RETRO_InitializeLightSource(0, 0, -1);
}
