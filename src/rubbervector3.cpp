//
// Flubber: a faceted rubber column with black and gold bands.
// Both sides are drawn with additive Glenz polygons. Palette banks encode
// black/black, black/gold and gold/gold overlaps, with lighting in the low
// six bits. Weak perspective keeps the rings aligned.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define FLUBBER_SEGMENTS 12 // vertices around each ring
#define FLUBBER_STACKS 16 // quads from top to bottom, so this many rings minus one of vertices
#define FLUBBER_RX 0.55f // model units, the thin radius of the ellipse
#define FLUBBER_RZ 1.45f // model units, the fat radius
#define FLUBBER_HEIGHT 5.2f // model units, a little taller than the screen at the default scale
#define FLUBBER_SWAY 0.50f // model units of lateral bow at the waist

#define FLUBBER_TWIST (3 * M_PI) // radians of the (modulated + 1) yaw
#define FLUBBER_SPIN_SPEED (2 * M_PI / 12.0) // rad/s, one turn every 12s
#define FLUBBER_TWIST_OMEGA (2 * M_PI / 20.0) // rad/s, 20s period
#define FLUBBER_TWIST_MOD_OMEGA (2 * M_PI / 17.0) // rad/s, 17s period
#define FLUBBER_SWAY_OMEGA (2 * M_PI / 18.0) // rad/s, 18s period

#define FLUBBER_EYE_DISTANCE 1400.0f
#define FLUBBER_DARK_C 0
#define FLUBBER_GOLD_C 64
#define FLUBBER_BACK_STRENGTH 0.8f // rear surface visibility, from 0 to 1

static Model3D *Flubber;
static Vertex RestVertex[RETRO_MAX_VERTICES];

static void AddTriangle(Model3D *model, int a, int b, int c, int color)
{
	if (model->faces >= RETRO_MAX_FACES) {
		RETRO_RageQuit("Too many flubber faces\n");
	}

	Face *face = &model->face[model->faces++];
	face->vertices = 3;
	face->vertex[0] = a;
	face->vertex[1] = b;
	face->vertex[2] = c;
	face->c = color;
	face->backc = color;
}

static void BuildFlubber(Model3D *model)
{
	for (int r = 0; r <= FLUBBER_STACKS; r++) {
		float y = FLUBBER_HEIGHT * ((float)r / FLUBBER_STACKS - 0.5f);
		for (int s = 0; s < FLUBBER_SEGMENTS; s++) {
			if (model->vertices >= RETRO_MAX_VERTICES) {
				RETRO_RageQuit("Too many flubber vertices\n");
			}
			float theta = s * (float)M_PI * 2 / FLUBBER_SEGMENTS;
			model->vertex[model->vertices].pos = { FLUBBER_RX * (float)cos(theta), y, FLUBBER_RZ * (float)sin(theta) };
			model->vertices++;
		}
	}

	for (int r = 0; r < FLUBBER_STACKS; r++) {
		int color = (r & 1) ? FLUBBER_DARK_C : FLUBBER_GOLD_C;
		int row0 = r * FLUBBER_SEGMENTS;
		int row1 = (r + 1) * FLUBBER_SEGMENTS;
		for (int s = 0; s < FLUBBER_SEGMENTS; s++) {
			int s1 = (s + 1) % FLUBBER_SEGMENTS;
			int i0 = row0 + s;
			int i1 = row0 + s1;
			int i2 = row1 + s1;
			int i3 = row1 + s;
			// Outward winding in this y-down, +z-away frame. The shared
			// diagonal is i0–i2 on every quad, so the far side's diagonals
			// cross the near ones into diamonds once Glenz draws both.
			AddTriangle(model, i0, i2, i1, color);
			AddTriangle(model, i0, i3, i2, color);
		}
	}

	RETRO_InitializeFaceNormals(model);
}

void DEMO_Render(double time, double deltatime)
{
	float spin = (float)(time * FLUBBER_SPIN_SPEED);
	float twistCos = cos(time * FLUBBER_TWIST_OMEGA);
	float twistMod = sin(time * FLUBBER_TWIST_MOD_OMEGA);
	float swayCos = cos(time * FLUBBER_SWAY_OMEGA);

	for (int i = 0; i < Flubber->vertices; i++) {
		const vec3 &v = RestVertex[i].pos;
		float t = (v.y + FLUBBER_HEIGHT / 2) / FLUBBER_HEIGHT;
		float xOffset = FLUBBER_SWAY * swayCos * sin(t * M_PI);
		float angle = spin + FLUBBER_TWIST * (twistCos * cos(t * M_PI / 3) * twistMod + 1);
		float cosAngle = cos(angle);
		float sinAngle = sin(angle);
		Flubber->vertex[i].pos = { v.x * cosAngle - v.z * sinAngle + xOffset, v.y, v.x * sinAngle + v.z * cosAngle };
	}

	RETRO_InitializeFaceNormals(Flubber);

	RETRO_RotateModel(0, 0, 0, Flubber);
	RETRO_ProjectModel(RETRO_PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, Flubber, FLUBBER_EYE_DISTANCE);
	RETRO_RenderModel(RETRO_POLY_GLENZ, RETRO_SHADE_FLAT, Flubber);
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(0, RETRO_EMBERBLACK);
	// Each bank has independent brightness, rather than a single ramp that
	// turns two ordinary gold contributions into cream. Reserve the fourth
	// bank for rare overlaps of three gold faces near a fold.
	const RETRO_Palette base[] = { RETRO_JET, RETRO_MUDGOLD, RETRO_DARKBRASS, RETRO_DRABGOLD };
	const RETRO_Palette mid[] = { RETRO_SOOT, RETRO_DUSTYGOLD, RETRO_BRASSGOLD, RETRO_DARKKHAKI };
	const RETRO_Palette peak[] = { RETRO_SILVER, RETRO_DARKGRAY, RETRO_BURLYWOOD, RETRO_KHAKI };
	for (int bank = 0; bank < 4; bank++) {
		int offset = bank * 64;
		RETRO_CreateGradientPalette(offset + 1, offset + 14, base[bank], mid[bank]);
		RETRO_CreateGradientPalette(offset + 14, offset + 42, mid[bank], peak[bank]);
		RETRO_CreateGradientPalette(offset + 42, offset + 64, peak[bank], peak[bank]);
		if (bank != 0) RETRO_SetColor(offset, base[bank]);
	}

	Flubber = RETRO_Allocate3DModel();
	Flubber->c = 1;
	Flubber->shades = 23;
	Flubber->twosided = true;
	Flubber->glenzlighting.diffuse = 0.2f;
	Flubber->glenzlighting.highlight = 0.8f;
	Flubber->glenzlighting.exponent = 4.0f;
	Flubber->glenzlighting.backstrength = FLUBBER_BACK_STRENGTH;
	BuildFlubber(Flubber);
	for (int i = 0; i < Flubber->vertices; i++) {
		RestVertex[i] = Flubber->vertex[i];
	}

	RETRO_InitializeLightSource(0, 0, -1);
}
