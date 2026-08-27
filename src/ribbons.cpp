//
// 3D ribbons
//
// Helical strips. Ribbon i is the helix
//
//   c(t) = (R cos(t + φ),  H (t / t_max − 1/2),  R sin(t + φ))
//
// t ∈ [0, t_max], t_max = 4π so the helix makes two turns. The strip
// is c ± w ρ, where ρ = (cos(t+φ), 0, sin(t+φ)) is the radial unit
// of the cylinder the helix sits on. Consecutive samples are a quad;
// the reverse winding is stored too, so a face that has turned its
// back still has a partner that faces the camera. Flat Lambert, the
// same as a cube: a ribbon's quad is small and a highlight would
// flash the whole of it. Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrocolor.h"

#define RIBBON_COUNT 3
#define RIBBON_SEGMENTS 80 // samples along t, so this many − 1 quads
#define RIBBON_TURNS 2 // how many times the helix wraps
#define ROTATION_SPEED 0.7 // radians a second about the middle axis
#define ROTATION_SPREAD 0.25

static const float RibbonRadius[RIBBON_COUNT] = { 1.15f, 0.78f, 0.48f };
static const float RibbonWidth[RIBBON_COUNT] = { 0.20f, 0.16f, 0.13f };
static const float RibbonHeight[RIBBON_COUNT] = { 2.1f, 2.1f, 2.1f };
static const float RibbonPhase[RIBBON_COUNT] = { 0.0f, 2.1f, 4.2f };

void AddQuad(Model3D *model, int a, int b, int c, int d)
{
	if (model->faces >= RETRO_MAX_FACES) {
		RETRO_RageQuit("Too many ribbon faces\n");
	}

	Face *face = &model->face[model->faces++];
	face->vertices = 4;
	face->vertex[0] = a;
	face->vertex[1] = b;
	face->vertex[2] = c;
	face->vertex[3] = d;
	face->c = 0;
	face->backc = 0;
}

void BuildRibbons(Model3D *model)
{
	float tmax = RIBBON_TURNS * 2 * M_PI;

	for (int r = 0; r < RIBBON_COUNT; r++) {
		int base = model->vertices;
		float radius = RibbonRadius[r];
		float width = RibbonWidth[r];
		float height = RibbonHeight[r];
		float phase = RibbonPhase[r];

		for (int i = 0; i < RIBBON_SEGMENTS; i++) {
			if (model->vertices + 2 > RETRO_MAX_VERTICES) {
				RETRO_RageQuit("Too many ribbon vertices\n");
			}

			float t = tmax * i / (RIBBON_SEGMENTS - 1);
			float a = t + phase;
			float ca = cos(a);
			float sa = sin(a);
			float y = height * (t / tmax - 0.5f);

			model->vertex[model->vertices].x = (radius - width) * ca;
			model->vertex[model->vertices].y = y;
			model->vertex[model->vertices].z = (radius - width) * sa;
			model->vertices++;

			model->vertex[model->vertices].x = (radius + width) * ca;
			model->vertex[model->vertices].y = y;
			model->vertex[model->vertices].z = (radius + width) * sa;
			model->vertices++;
		}

		for (int i = 0; i < RIBBON_SEGMENTS - 1; i++) {
			int l0 = base + 2 * i;
			int r0 = l0 + 1;
			int l1 = l0 + 2;
			int r1 = l0 + 3;
			AddQuad(model, l0, l1, r1, r0);
			AddQuad(model, l0, r0, r1, l1);
		}
	}

	RETRO_InitializeFaceNormals(model);
}

void DEMO_Render(double deltatime)
{
	static float ax, ay, az;
	ax = fmod(ax + deltatime * ROTATION_SPEED * (1 - ROTATION_SPREAD), 2 * M_PI);
	ay = fmod(ay + deltatime * ROTATION_SPEED, 2 * M_PI);
	az = fmod(az + deltatime * ROTATION_SPEED * (1 + ROTATION_SPREAD), 2 * M_PI);

	RETRO_RotateModel(ax, ay, az);
	RETRO_ProjectModel(90);
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(RETRO_PHONG_OFFSET, RETRO_COLORS, RETRO_SADDLEBROWN, RETRO_GOLD);

	Model3D *model = RETRO_Allocate3DModel();
	model->c = RETRO_PHONG_OFFSET;
	model->shades = RETRO_PHONG_SHADES;
	BuildRibbons(model);

	RETRO_InitializeLightSource(0, 0, -1);
}
