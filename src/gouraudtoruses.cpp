//
// Linked Gouraud Shaded Toruses
//
// Two gouraud shaded toruses form a permanent two-ring link. Their planes are
// perpendicular and their centres are separated by one ring radius, so each
// torus passes through the hole of the other without their tubes intersecting.
// The complete link tumbles as one rigid object, preserving the knot while its
// rings continually pass in front of and behind each other.
//
// Both are the same model, assets/torusquads.obj, loaded twice so each keeps
// its own rotated vertices and its own base color. The model carries the
// analytic outward normals of the surface, which is what gouraud interpolates
// between, so the shading runs smooth around the tube with no seam at the
// wrap.
//
// A torus is not convex, so its own near side hides parts of its far side and
// the two toruses interleave. Neither the back-face test nor the painter's
// sort can settle that, so the depth buffer is cleared once a frame and both
// models are drawn into it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"
#include "lib/retromath.h"

// Pixels per model unit. Each torus reaches 2.1 from its centre and the linked
// centres sit 0.75 to either side of the origin.
#define PROJECTION_SCALE 36.0f

#define LINK_RADIUS 1.5f
#define LINK_ROTATION_SPEED 0.8f
#define LINK_ROTATION_PERIOD (20 * M_PI) // 10, 7 and 4 whole turns on x, y and z

static Model3D *Torus1 = NULL;
static Model3D *Torus2 = NULL;

// Bake one torus into the linked pair before the animation starts. Translation
// is part of the source geometry so the later rotation carries both the ring
// and its centre around the origin as one rigid object.
void PlaceTorusInLink(Model3D *model, float angle, float tx)
{
	float c = cos(angle);
	float s = sin(angle);

	for (int i = 0; i < model->vertices; i++) {
		float y = model->vertex[i].y;
		float z = model->vertex[i].z;
		model->vertex[i].x += tx;
		model->vertex[i].y = c * y - s * z;
		model->vertex[i].z = s * y + c * z;

		y = model->normal[i].y;
		z = model->normal[i].z;
		model->normal[i].y = c * y - s * z;
		model->normal[i].z = s * y + c * z;
	}

	for (int i = 0; i < model->faces; i++) {
		Direction *directions[] = {
			&model->face[i].facenormal,
			&model->face[i].tangent,
			&model->face[i].bitangent,
		};
		for (Direction *direction : directions) {
			float y = direction->y;
			float z = direction->z;
			direction->y = c * y - s * z;
			direction->z = s * y + c * z;
		}
	}
}

void DEMO_Render(double deltatime)
{
	// Rotate the baked pair by exactly the same matrix, keeping the link rigid.
	static float phase = 0;
	phase = fmod(phase + deltatime * LINK_ROTATION_SPEED, LINK_ROTATION_PERIOD);
	float ax = phase;
	float ay = phase * 0.7f;
	float az = phase * 0.4f;

	// Clear the q depth buffer once per frame, so both toruses resolve depth
	// against the same one. Each model is then drawn with cleardepth false, or
	// it would start a depth range of its own and erase the one before it
	RETRO_ClearDepthBuffer();

	// Draw torus 1 (blue gradient)
	RETRO_RotateModel(ax, ay, az, Torus1);
	RETRO_ProjectModel(PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Torus1);
	RETRO_RenderModel(RETRO_POLY_GOURAUD, RETRO_SHADE_NONE, Torus1, false);

	// Draw torus 2 (green gradient)
	RETRO_RotateModel(ax, ay, az, Torus2);
	RETRO_ProjectModel(PROJECTION_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Torus2);
	RETRO_RenderModel(RETRO_POLY_GOURAUD, RETRO_SHADE_NONE, Torus2, false);
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

	// Load the same torus twice, so each instance carries its own rotated
	// vertices and its own end of the palette
	Torus1 = RETRO_Load3DModel("assets/torusquads.obj");
	Torus1->c = 1;
	Torus1->shades = 127;

	Torus2 = RETRO_Load3DModel("assets/torusquads.obj");
	Torus2->c = 128;
	Torus2->shades = 127;

	// One ring lies in the xy plane and the other in the xz plane. Separating
	// their centres by the major radius produces a Hopf link.
	PlaceTorusInLink(Torus1, 0, -LINK_RADIUS / 2);
	PlaceTorusInLink(Torus2, M_PI / 2, LINK_RADIUS / 2);

	// Initialize directional light source coming straight from the front
	RETRO_InitializeLightSource(0, 0, -1);
}
