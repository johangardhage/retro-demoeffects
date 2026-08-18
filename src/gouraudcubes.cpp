//
// Gouraud Shaded Cubes
//
// Two equal cubes occupy the same centre and rotate independently. Their faces
// continually cut through one another, so neither cube can be drawn entirely
// before the other: which surface is visible changes within the overlapping
// polygons. A shared q-buffer resolves every pixel using reciprocal depth and
// makes the blue and green faces weave cleanly through each other.
//
// Gouraud shading evaluates the front light at each corner normal, then
// interpolates the resulting palette shade across every face. Each cube owns a
// separate half of the palette so depth changes are easy to see.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define CUBE1_ROTATION_SPEED 1.1f
#define CUBE2_ROTATION_SPEED -1.7f
#define CUBE1_ROTATION_PERIOD (20 * M_PI) // 10, 7 and 13 whole turns
#define CUBE2_ROTATION_PERIOD (10 * M_PI) // 4, 6 and 5 whole turns

static Model3D *Cube1 = NULL;
static Model3D *Cube2 = NULL;

void DEMO_Render(double deltatime)
{
	// Rotate the cubes at different rates and around different combinations of
	// axes, while keeping both centres fixed at the origin.
	static float phase1 = 0;
	static float phase2 = 0;
	phase1 = fmod(phase1 + deltatime * CUBE1_ROTATION_SPEED, CUBE1_ROTATION_PERIOD);
	phase2 = fmod(phase2 + deltatime * CUBE2_ROTATION_SPEED, CUBE2_ROTATION_PERIOD);
	if (phase2 < 0) phase2 += CUBE2_ROTATION_PERIOD;

	// Both models must use the same depth buffer. Clearing before either draw
	// would make the second cube overwrite the first regardless of its depth.
	RETRO_ClearDepthBuffer();

	RETRO_RotateModel(phase1, phase1 * 0.7f, phase1 * 1.3f, Cube1);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Cube1);
	RETRO_RenderModel(RETRO_POLY_GOURAUD, RETRO_SHADE_NONE, Cube1, false);

	RETRO_RotateModel(phase2 * 0.8f, phase2 * 1.2f, phase2, Cube2);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Cube2);
	RETRO_RenderModel(RETRO_POLY_GOURAUD, RETRO_SHADE_NONE, Cube2, false);
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_Palette palette[RETRO_COLORS];
	memset(palette, 0, sizeof(palette));
	RETRO_SetColor(0, RETRO_BLACK, palette);
	RETRO_CreatePhongRamp(&palette[1], 127, RETRO_AZURE, RETRO_K_SPECULAR, 5.0f, 255);
	RETRO_CreatePhongRamp(&palette[128], 127, RETRO_SPRINGGREEN, RETRO_K_SPECULAR, 5.0f, 255);
	RETRO_SetPalette(palette);

	// Load separate instances so each cube retains its own transformed vertices
	// and uses its own Gouraud palette range.
	Cube1 = RETRO_Load3DModel("assets/cube.obj");
	Cube1->c = 1;
	Cube1->cintensity = 126;

	Cube2 = RETRO_Load3DModel("assets/cube.obj");
	Cube2->c = 128;
	Cube2->cintensity = 126;

	RETRO_InitializeLightSource(0, 0, -1);
}
