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

constexpr int ROTATION_SPEED = 1; // radians a second, about each axis
constexpr float DISTANCE_MIN = 0.1f; // the smallest projection scale the , and . keys reach, so the mesh never collapses or flips
constexpr int MATCAP_SIZE = 256; // the matcaps are square, and this is their side in pixels
constexpr vec3 MATCAP_FACE = { 0.86f, 0.23f, 0.59f }; // the material, as intensities

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

// A key, the mode it selects and the palette that mode reads, NULL for the
// texture's own
struct ModeKey {
	SDL_Scancode key;
	Mode mode;
	const RETRO_Palette *palette;
};

// The texture's own palette, with black behind the mesh and the text color
static void SetTexturePalette(void)
{
	RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_TEXMAP));
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(255, RETRO_PERIWINKLE);
}

void DEMO_Render(RETRO_Time time)
{
	static Mode mode = { .rendertype = RETRO_POLY_TEXTURE };
	static unsigned char *bumpmap = NULL;
	static bool rotate = true;
	static bool usage = true;
	// Start with the mask's authored face upright before rotating all three axes.
	static float ax = -M_PI / 2;
	static float ay = 0;
	static float az = M_PI;
	static float distance = 0.5;

	// Built on the first frame, once the images it points into are loaded
	static const ModeKey ModeKeys[] = {
		{ SDL_SCANCODE_T, { .rendertype = RETRO_POLY_TEXTURE }, NULL },
		{ SDL_SCANCODE_0, { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_TABLE, .shadetable = MaterialShadeTables[MATERIAL_GOURAUD], .color = RETRO_SHADE_TABLE_SHADES * 5 / 8 }, MaterialPalette },
		{ SDL_SCANCODE_1, { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_FLAT, .shadetable = MaterialShadeTables[MATERIAL_FLAT] }, MaterialPalette },
		{ SDL_SCANCODE_2, { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_GOURAUD, .shadetable = MaterialShadeTables[MATERIAL_GOURAUD] }, MaterialPalette },
		{ SDL_SCANCODE_3, { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_MATCAP, .shadetable = MaterialShadeTables[MATERIAL_PHONG], .envmap = MiniPhongMap, .color = 128, .envmapradius = 90 }, MaterialPalette },
		{ SDL_SCANCODE_4, { .rendertype = RETRO_POLY_TEXTURE, .shadertype = RETRO_SHADE_PHONG, .shadetable = MaterialShadeTables[MATERIAL_PHONG] }, MaterialPalette },
		{ SDL_SCANCODE_D, { .rendertype = RETRO_POLY_DOT, .color = 255 }, MaterialPalette },
		{ SDL_SCANCODE_W, { .rendertype = RETRO_POLY_WIREFRAME, .color = 255 }, MaterialPalette },
		{ SDL_SCANCODE_P, { .rendertype = RETRO_POLY_MATCAP, .envmap = PhongMap, .color = 128, .envmapradius = 90, .bumpgrazing = RETRO_BUMP_GRAZING * 3 / 2 }, PhongPalette },
		{ SDL_SCANCODE_O, { .rendertype = RETRO_POLY_MATCAP, .envmap = MiniPhongMap, .color = 128, .envmapradius = 90, .bumpgrazing = RETRO_BUMP_GRAZING * 3 / 2 }, PhongPalette },
		{ SDL_SCANCODE_F, { .rendertype = RETRO_POLY_PHONG, .shades = RETRO_COLORS }, PhongPalette },
		{ SDL_SCANCODE_M, { .rendertype = RETRO_POLY_ENVIRONMENT, .envmap = RETRO_ImageData(ASSET_ENVMAP), .color = 128, .envmapradius = 90, .bumpgrazing = RETRO_BUMP_GRAZING * 2 }, RETRO_ImagePalette(ASSET_ENVMAP) },
	};

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
	if (RETRO_KeyState(SDL_SCANCODE_COMMA)) {
		distance += 1 * time.delta;
	}
	if (RETRO_KeyState(SDL_SCANCODE_PERIOD)) {
		distance -= 1 * time.delta;
	}
	for (const ModeKey &modekey : ModeKeys) {
		if (RETRO_KeyPressed(modekey.key)) {
			mode = modekey.mode;
			if (modekey.palette) {
				RETRO_Set6bitPalette(modekey.palette);
			} else {
				SetTexturePalette();
			}
		}
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

	// Update rotation
	if (rotate) {
		ax += time.delta * ROTATION_SPEED;
		ay += time.delta * ROTATION_SPEED;
		az += time.delta * ROTATION_SPEED;
	}
	ax = mod(ax, (float)(2 * M_PI));
	ay = mod(ay, (float)(2 * M_PI));
	az = mod(az, (float)(2 * M_PI));
	distance = MAX(distance, DISTANCE_MIN);

	// Draw model
	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(distance);
	RETRO_RenderModel(mode.rendertype, mode.shadertype);

	// Draw help
	if (usage) {
		RETRO_PutString("Runtime controls:\n"
						"\n"
						"r to toggle rotation\n"
						", and . to change object distance\n"
						"i and k to change x rotation\n"
						"x and z to change y rotation\n"
						"j and l to change z rotation\n"
						"\n"
						"d to use dots\n"
						"w to use wireframe\n"
						"t to use texture mapping\n"
						"0 to use shade table texture mapping\n"
						"1 to use flat shaded texture mapping\n"
						"2 to use gouraud shaded texture mapping\n"
						"3 to use matcap shaded texture mapping\n"
						"4 to use phong shaded texture mapping\n"
						"m to use metal environment mapping\n"
						"p to use matcap mapping\n"
						"o to use mini matcap mapping\n"
						"f to use phong shading\n"
						"b to toggle bumpmapping\n"
						"\n"
						"h to toggle this help screen", 0, 10, 255);
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
	SetTexturePalette();
	RETRO_SetFont(RETRO_FONT_VGA_8X8);

	// Load model. Every mode that reads a texture reads the same one
	Model3D *model = RETRO_Load3DModel("assets/mask.obj");
	model->texmap = RETRO_ImageData(ASSET_TEXMAP);
}
