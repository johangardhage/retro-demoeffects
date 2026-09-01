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

#define FACE_FRAMES 11
#define FACE_VERTS 529
#define FACE_TRIS 968
#define WALL_SCALE 21
#define MORPH_PERIOD 14.0f
#define FACE_SPECULAR 0.78f
#define FACE_FALLOFF 16.0f
#define FACE_COLORS 8

static Model3D *Wall;
static unsigned char WallShadeTable[RETRO_MAX_SHADING_COLORS];

void DEMO_Render(double deltatime)
{
	static float time;
	time += deltatime;
	float phase = fmodf(time, MORPH_PERIOD) / MORPH_PERIOD;

	float yaw = time * 0.42f;
	float pitch = -0.20f + 0.48f * sinf(time * 0.31f);
	float roll = 0.06f * sinf(time * 0.19f);

	// Ping-pong the captured frames: flat, the head leans in, then it lets go.
	float u = phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
	RETRO_MorphModel(u, Wall);
	RETRO_InitializeFaceNormals(Wall);
	RETRO_InitializeVertexNormals(Wall);
	RETRO_RotateModel(pitch, yaw, roll, Wall);
	RETRO_ProjectModel(WALL_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Wall);
	RETRO_RenderModel(RETRO_POLY_TEXTURE, RETRO_SHADE_GOURAUD, Wall);
}

void DEMO_Initialize(void)
{
	RETRO_LoadImage("assets/facewall_256x256.pcx");

	// Init palette
	RETRO_Palette texturepalette[FACE_COLORS];
	RETRO_CreateGradientPalette(0, FACE_COLORS, RETRO_BLACK, RETRO_To6bitColor(RETRO_WHITE), texturepalette);
	RETRO_CreateOptimalPalette(texturepalette, FACE_COLORS, FACE_SPECULAR, FACE_FALLOFF);
	RETRO_CreateShadeTable(texturepalette, FACE_COLORS, FACE_SPECULAR, FACE_FALLOFF, WallShadeTable);
	RETRO_Set6bitPalette(RETRO_OptimalPalette());

	// Load model
	Wall = RETRO_Load3DModel("assets/facewall.obj", "assets/facewall_%02d.obj", FACE_FRAMES);
	if (Wall->vertices != FACE_VERTS || Wall->faces != FACE_TRIS) {
		RETRO_RageQuit("facewall.obj has %d vertices, %d faces (expected %d, %d)\n", Wall->vertices, Wall->faces, FACE_VERTS, FACE_TRIS);
	}
	Wall->twosided = true;
	Wall->c = 0;
	Wall->shades = RETRO_SHADES;
	Wall->texmap = RETRO_ImageData();
	Wall->shadetable = WallShadeTable;

	// Init lightsource
	RETRO_InitializeLightSource(-0.28f, -0.42f, -0.86f);
}
