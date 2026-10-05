//
// Face behind a wall
//
// A 23×23 morphing sheet, posed by a run of captured frames: the first two
// lie flat, later ones push a real head through the plane. A Mayan stone
// texture is the skin on that sheet while it tumbles.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"
#include "lib/retroshadetable.h"

#define FACE_MD3_SCALE 256.0f // steps per unit facewall.md3 was packed at
#define WALL_SCALE 21
#define MORPH_PERIOD 14.0f
#define FACE_SPECULAR 0.78f
#define FACE_FALLOFF 16.0f
#define FACE_COLORS 8

static unsigned char WallShadeTable[RETRO_SHADE_TABLE_SIZE];

void DEMO_Render(RETRO_Time time)
{
	// Calculate phase
	float phase = fmod(time.total, MORPH_PERIOD) / MORPH_PERIOD;

	float yaw = 0.7f * sinf(time.total * 0.42f);
	float pitch = -0.20f + 0.48f * sinf(time.total * 0.31f);
	float roll = 0.06f * sinf(time.total * 0.19f);

	// Ping-pong the captured frames: flat, the head leans in, then it lets go.
	float u = phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
	RETRO_MorphModel(u);
	RETRO_InitializeFaceNormals();
	RETRO_InitializeVertexNormals();

	// Draw wall
	RETRO_RotateModel(pitch, yaw, roll);
	RETRO_ProjectModel(WALL_SCALE);
	RETRO_RenderModel(RETRO_POLY_TEXTURE, RETRO_SHADE_GOURAUD);
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/facewall_256x256.pcx");

	// Init palette
	RETRO_Palette texturepalette[FACE_COLORS];
	RETRO_CreateGradientPalette(0, FACE_COLORS, RETRO_BLACK, RETRO_To6bitColor(RETRO_WHITE), texturepalette);
	RETRO_Palette palette[RETRO_COLORS];
	RETRO_CreatePhongShadeTablePalette(texturepalette, FACE_COLORS, palette, FACE_SPECULAR, FACE_FALLOFF);
	RETRO_CreatePhongShadeTable(texturepalette, FACE_COLORS, palette, WallShadeTable, FACE_SPECULAR, FACE_FALLOFF);
	RETRO_Set6bitPalette(palette);

	// Load model
	Model3D *model = RETRO_LoadMD3Model("assets/facewall.md3", FACE_MD3_SCALE);
	model->twosided = true;
	model->c = 0;
	model->shades = RETRO_SHADE_TABLE_SHADES;
	model->texmap = RETRO_ImageData();
	model->shadetable = WallShadeTable;

	// Init lightsource
	RETRO_InitializeLightSource(-0.28f, -0.42f, -0.86f);
}
