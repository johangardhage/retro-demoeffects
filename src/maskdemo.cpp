//
// Mask demo
//
// One mesh, many materials. Each key selects a renderer and the maps it
// reads: texture, shade table, matcap/env lookup, bump height. The math of
// each path lives in retropoly.h; this file only chooses the inputs.
//
// Shade-table level 0 sits at 33° of incidence, on the shoulder of the
// highlight: high enough that the material shows, low enough that the
// untinted specular does not wash the texture out. Halfway up the ramp would
// be 44°, past the 45° cutoff where every specular term is zero.
//
// H is the height difference that tilts to grazing; larger H is shallower.
// A bare matcap has only the sheen to spend, so a bump uses 3/2 H. A metal
// environment map is a Blinn/Newell sphere map of the reflection of V about
// N, so a tilt lands on a different part of the photo rather than a
// neighboring shade; that bump uses 2H (half the default tilt). Euler
// angles live on 2π. The mesh starts at ax = −π/2, az = π so the face
// is upright.
//
// The matcaps are generated, not loaded: RETRO_CreateDiamondAnglePhongMap lays
// out the lit ball, and RETRO_CreateAnglePhongPalette colors it with a face of
// (0.86, 0.23, 0.59) under the library's default material, in 6 bits. The
// full map spans the palette. The mini map is the same ball in shade levels
// for the textured matcap, brightest one below the shade table's height and
// that many steps to a quarter turn, so it tops out at the table's last
// level; read bare through the palette it keeps to the dark half, below the
// highlight.
//
// The light is a headlight, which the shaded texture modes, the bumps and the
// Phong mode are lit by; the matcaps and the metal environment map are
// front-lit by construction. The Phong mode reads the same palette as the
// matcap, and the textured Phong mode the same shade table as the matcap
// texture mode, so each pair shows the same material, per pixel and by
// lookup.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"
#include "lib/retrofont.h"

#define ROTATION_SPEED 1 // radians a second, about each axis
#define MATCAP_SIZE 256 // the matcaps are square, and this is their side in pixels
#define MATCAP_FACE vec3{ 0.86f, 0.23f, 0.59f } // the material, as intensities

enum { ASSET_TEXMAP, ASSET_ENVMAP, ASSET_BUMPMAP };
enum { MATERIAL_FLAT, MATERIAL_GOURAUD, MATERIAL_PHONG, MATERIALS };

static RETRO_Palette MaterialPalette[RETRO_COLORS];
static unsigned char MaterialShadeTables[MATERIALS][RETRO_SHADE_TABLE_SIZE];
static unsigned char PhongMap[MATCAP_SIZE * MATCAP_SIZE];
static unsigned char MiniPhongMap[MATCAP_SIZE * MATCAP_SIZE];
static RETRO_Palette PhongPalette[RETRO_COLORS];

// What a key selects: a renderer and the maps it reads. A mode names only what
// it changes from these defaults
struct Mode {
	RETRO_POLY_TYPE rendertype;
	RETRO_POLY_SHADE shadertype = RETRO_SHADE_NONE;
	unsigned char *shadetable = NULL;
	unsigned char *envmap = NULL;
	unsigned char color = 0;
	int shades = RETRO_SHADE_TABLE_SHADES; // the texture modes' shade table, all of it
	int envmapradius = 0;
	int bumpgrazing = RETRO_BUMP_GRAZING;
};

void DEMO_Render(RETRO_Time time)
{
	static Mode mode = { .rendertype = RETRO_POLY_TEXTURE, .color = 64 };
	static unsigned char *bumpmap = NULL;
	static bool rotate = true;
	static bool usage = true;

	// Handle keys
	if (RETRO_KeyPressed(SDL_SCANCODE_H)) {
		usage = !usage;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_R)) {
		rotate = !rotate;
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_B)) {
		bumpmap = bumpmap ? NULL : RETRO_ImageData(ASSET_BUMPMAP);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_T)) {
		mode = { .rendertype = RETRO_POLY_TEXTURE };
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_TEXMAP));
		RETRO_SetColor(0, RETRO_BLACK);
		RETRO_SetColor(255, RETRO_PERIWINKLE);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_0)) {
		mode = { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_TABLE, .shadetable = MaterialShadeTables[MATERIAL_GOURAUD], .color = RETRO_SHADE_TABLE_SHADES * 5 / 8 };
		RETRO_Set6bitPalette(MaterialPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_1)) {
		mode = { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_FLAT, .shadetable = MaterialShadeTables[MATERIAL_FLAT] };
		RETRO_Set6bitPalette(MaterialPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_2)) {
		mode = { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_GOURAUD, .shadetable = MaterialShadeTables[MATERIAL_GOURAUD] };
		RETRO_Set6bitPalette(MaterialPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_3)) {
		mode = { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_MATCAP, .shadetable = MaterialShadeTables[MATERIAL_PHONG], .envmap = MiniPhongMap, .color = 128, .envmapradius = 90 };
		RETRO_Set6bitPalette(MaterialPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_4)) {
		mode = { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_PHONG, .shadetable = MaterialShadeTables[MATERIAL_PHONG] };
		RETRO_Set6bitPalette(MaterialPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_D)) {
		mode = { .rendertype = RETRO_POLY_DOT, .color = 255 };
		RETRO_Set6bitPalette(MaterialPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_W)) {
		mode = { .rendertype = RETRO_POLY_WIREFRAME, .color = 255 };
		RETRO_Set6bitPalette(MaterialPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_P)) {
		mode = { .rendertype = RETRO_POLY_MATCAP, .envmap = PhongMap, .color = 128, .envmapradius = 90, .bumpgrazing = RETRO_BUMP_GRAZING * 3 / 2 };
		RETRO_Set6bitPalette(PhongPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_O)) {
		mode = { .rendertype = RETRO_POLY_MATCAP, .envmap = MiniPhongMap, .color = 128, .envmapradius = 90, .bumpgrazing = RETRO_BUMP_GRAZING * 3 / 2 };
		RETRO_Set6bitPalette(PhongPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_F)) {
		mode = { .rendertype = RETRO_POLY_PHONG, .shades = RETRO_COLORS };
		RETRO_Set6bitPalette(PhongPalette);
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_M)) {
		mode = { .rendertype = RETRO_POLY_ENVIRONMENT, .envmap = RETRO_ImageData(ASSET_ENVMAP), .color = 128, .envmapradius = 90, .bumpgrazing = RETRO_BUMP_GRAZING * 2 };
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_ENVMAP));
	}

	// Update model
	Model3D *model = RETRO_Get3DModel();
	model->shadetable = mode.shadetable;
	model->envmap = mode.envmap;
	model->bumpmap = bumpmap;
	model->c = mode.color;
	model->shades = mode.shades;
	model->envmapradius = mode.envmapradius;
	model->bumpgrazing = mode.bumpgrazing;

	// Start with the mask's authored face upright before rotating all three axes.
	static float ax = -M_PI / 2, ay = 0, az = M_PI, distance = 0.5;
	if (rotate) {
		ax += time.delta * ROTATION_SPEED;
		ay += time.delta * ROTATION_SPEED;
		az += time.delta * ROTATION_SPEED;
	}

	if (RETRO_KeyState(SDL_SCANCODE_I)) {
		rotate = false;
		ax += ROTATION_SPEED * time.delta;
	}
	if (RETRO_KeyState(SDL_SCANCODE_K)) {
		rotate = false;
		ax -= ROTATION_SPEED * time.delta;
	}
	if (RETRO_KeyState(SDL_SCANCODE_X)) {
		rotate = false;
		ay += ROTATION_SPEED * time.delta;
	}
	if (RETRO_KeyState(SDL_SCANCODE_Z)) {
		rotate = false;
		ay -= ROTATION_SPEED * time.delta;
	}
	if (RETRO_KeyState(SDL_SCANCODE_J)) {
		rotate = false;
		az += ROTATION_SPEED * time.delta;
	}
	if (RETRO_KeyState(SDL_SCANCODE_L)) {
		rotate = false;
		az -= ROTATION_SPEED * time.delta;
	}
	ax = mod(ax, (float)(2 * M_PI));
	ay = mod(ay, (float)(2 * M_PI));
	az = mod(az, (float)(2 * M_PI));
	if (RETRO_KeyState(SDL_SCANCODE_COMMA)) {
		distance += 1 * time.delta;
	}
	if (RETRO_KeyState(SDL_SCANCODE_PERIOD)) {
		distance -= 1 * time.delta;
	}

	// Draw model
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(distance);
	RETRO_RenderModel(mode.rendertype, mode.shadertype);

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
		RETRO_PutString("3 to use matcap shaded texture mapping", 0, 150, 255);
		RETRO_PutString("4 to use phong shaded texture mapping", 0, 160, 255);
		RETRO_PutString("m to use metal environment mapping", 0, 170, 255);
		RETRO_PutString("p to use matcap mapping", 0, 180, 255);
		RETRO_PutString("o to use mini matcap mapping", 0, 190, 255);
		RETRO_PutString("f to use phong shading", 0, 200, 255);
		RETRO_PutString("b to toggle bumpmapping", 0, 210, 255);
		RETRO_PutString("h to toggle this help screen", 0, 230, 255);
	}
}

void DEMO_Initialize(void)
{
	// Load assets
	RETRO_LoadImage("assets/mask_texmap_256x256.pcx");
	RETRO_LoadImage("assets/mask_envmap_256x256.pcx");
	RETRO_LoadImage("assets/mask_bumpmap_256x256.pcx");

	// Init matcaps
	RETRO_CreateDiamondAnglePhongMap(PhongMap, MATCAP_SIZE, MATCAP_SIZE);
	RETRO_CreateDiamondAnglePhongMap(MiniPhongMap, MATCAP_SIZE, MATCAP_SIZE, RETRO_SHADE_TABLE_SHADES - 1, RETRO_SHADE_TABLE_SHADES);
	RETRO_CreateAnglePhongPalette(MATCAP_FACE, RETRO_K_SPECULAR, RETRO_K_FALLOFF, PhongPalette, 63);

	// Init palette. One palette, three shade tables. Flat: a strong, moderately focused
	// highlight. Gouraud: the full plastic highlight, sampled at vertices.
	// Phong: the library's default falloff.
	RETRO_Palette *texturepalette = RETRO_ImagePalette(ASSET_TEXMAP);
	RETRO_CreatePhongShadeTablePalette(texturepalette, RETRO_SHADE_TABLE_COLORS, MaterialPalette, RETRO_K_SPECULAR, 5.0f);
	RETRO_CreatePhongShadeTable(texturepalette, RETRO_SHADE_TABLE_COLORS, MaterialPalette, MaterialShadeTables[MATERIAL_GOURAUD], RETRO_K_SPECULAR, 5.0f);
	RETRO_CreatePhongShadeTable(texturepalette, RETRO_SHADE_TABLE_COLORS, MaterialPalette, MaterialShadeTables[MATERIAL_FLAT], 0.6f, 5.0f);
	RETRO_CreatePhongShadeTable(texturepalette, RETRO_SHADE_TABLE_COLORS, MaterialPalette, MaterialShadeTables[MATERIAL_PHONG], RETRO_K_SPECULAR, RETRO_K_FALLOFF);
	RETRO_Set6bitPalette(texturepalette);
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(255, RETRO_PERIWINKLE);
	RETRO_SetFont(RETRO_FONT_VGA_8X8);

	// Load model. Every mode that reads a texture reads the same one
	Model3D *model = RETRO_Load3DModel("assets/mask.obj");
	model->texmap = RETRO_ImageData(ASSET_TEXMAP);
}
