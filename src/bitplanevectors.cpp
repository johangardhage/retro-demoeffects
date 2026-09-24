//
// Bitplane vectors
//
// Three flat plates, a triangle, a square and a hexagon, turn through one
// another about a shared centre, drawn the way a planar display draws them:
// plate k owns bit k of the palette index, and each of its faces writes only
// that,
//
//   pixel = (pixel & ~mask) | (color & mask)
//
// A plate covers any pixel at most once, and no plate touches another's bits,
// so nothing is sorted and nothing is depth tested: in any draw order, every
// pixel ends up with the bit of each plate that covers it. The palette does
// the rest: the color of index i is the sum of the colors of the plates whose
// bits it has set, so the overlaps mix like light. The plates are two sided
// and unlit, so a plate turned away from the viewer still shows. Euler angles
// live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define PLATE_COUNT 3

struct Plate {
	int sides;				// A regular polygon with this many corners
	float radius;			// Centre to corner, in projection units
	vec3 speed;				// Radians a second about x, y, z
	RETRO_Palette color;
};

static const Plate Plates[PLATE_COUNT] = {
	{ 3, 1.9f, { 0.9f, 0.6f, 0.4f }, RETRO_SCARLET },
	{ 4, 1.5f, { -0.5f, 0.8f, 0.7f }, RETRO_SPRINGGREEN },
	{ 6, 1.6f, { 0.7f, -0.4f, 0.9f }, RETRO_OCEANBLUE }
};

static Model3D *PlateModels[PLATE_COUNT];

void DEMO_Render(double time, double deltatime)
{
	for (int k = 0; k < PLATE_COUNT; k++) {
		const Plate &plate = Plates[k];
		float ax = fmod(time * plate.speed.x, 2 * M_PI);
		float ay = fmod(time * plate.speed.y, 2 * M_PI);
		float az = fmod(time * plate.speed.z, 2 * M_PI);

		RETRO_RotateModel(ax, ay, az, PlateModels[k]);
		RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, PlateModels[k]);
		RETRO_RenderModel(RETRO_POLY_MASKED, RETRO_SHADE_NONE, PlateModels[k], false);
	}
}

void DEMO_Initialize(void)
{
	// Init palette. Every combination of the three plates gets its own entry,
	// the sum of their colors
	RETRO_Palette palette[RETRO_COLORS];
	memset(palette, 0, sizeof(palette));
	for (int i = 0; i < 1 << PLATE_COUNT; i++) {
		int r = 0, g = 0, b = 0;
		for (int k = 0; k < PLATE_COUNT; k++) {
			if (i & (1 << k)) {
				r += Plates[k].color.r;
				g += Plates[k].color.g;
				b += Plates[k].color.b;
			}
		}
		palette[i] = { (unsigned char)MIN(r, 255), (unsigned char)MIN(g, 255), (unsigned char)MIN(b, 255) };
	}
	RETRO_SetPalette(palette);

	// Each plate is a regular polygon in the xy plane, fanned from its first
	// corner into quads. An odd corner count ends on a triangle, a quad that
	// repeats its last corner. The plate's color is its own bit
	for (int k = 0; k < PLATE_COUNT; k++) {
		const Plate &plate = Plates[k];
		Model3D *model = RETRO_Allocate3DModel();
		for (int i = 0; i < plate.sides; i++) {
			float angle = 2 * M_PI * i / plate.sides;
			RETRO_AddModelVertex(model, plate.radius * cos(angle), plate.radius * sin(angle), 0.0f);
		}

		for (int i = 1; i < plate.sides - 1; i += 2) {
			RETRO_AddModelQuad(model, 0, i, i + 1, MIN(i + 2, plate.sides - 1));
		}

		model->c = 1 << k;
		model->mask = 1 << k;
		model->twosided = true;
		PlateModels[k] = model;
	}
}
