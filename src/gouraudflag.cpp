//
// Waving flag, gouraud shaded
//
// The same flag and the same wave as flatshadedflag.cpp, lit per vertex instead
// of per face, so the cloth reads as one curved surface rather than as a grid of
// facets. Gouraud needs a normal at every vertex, and the two ways of getting
// one both fail here. Taking them from the faces cannot work on a two sided
// mesh: the front and back copy of a quad carry opposite normals into the same
// vertex, which sum to zero. Taking them from the asset cannot work either,
// since the sheet is deformed every frame. So they are differentiated from the
// wave itself,
//
//   z = AMP u W / (1 + HALF),   W = sin(A) + HALF sin(B)
//
// with A and B the two modes' phases. The surface is a height field over x and
// y, so the normal is (dz/dx, dz/dy, -1) up to length: the u factor makes the
// first term a product rule, and the chain rule brings out the rate each mode
// turns at. RADIAN is there because SIN and COS run on RETRO_SINCOS_ANGLE units
// per turn, so COS is the derivative of SIN only up to that scale.
//
// The asset carries two normals per vertex, the second the negation of the
// first, and its faces index the set for the side they are on. Face normals are
// never needed: back face culling is a screen space winding test.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define FLAG_SUBDIV 2 // quads per cell of the flag, as the asset was built
#define FLAG_COLS (16 * FLAG_SUBDIV) // the flag is 16 cells across, per SFS 1982:269
#define FLAG_SPAN 1.6 // the asset's own size, 16:10
#define FLAG_DROP 1.0
#define FLAG_SCALE 125 // pixels a model unit covers at the depth the flag hangs at
#define FLAG_AMP 0.34 // model units the fly throws, at the crest
#define FLAG_HALF 0.6 // the second mode, against the first
#define FLAG_RATEU 300 // table units the first mode turns across the flag
#define FLAG_RATEV 60 // and down it
#define FLAG_RATEU2 500 // and the second mode, across
#define FLAG_SPEED 55 // table units a second
#define FLAG_SWAY 0.55 // radians of yaw the flag swings through
#define FLAG_SWAYSPEED 13 // table units a second
#define FLAG_TILTX 0.12 // radians the view leans, fixed
#define FLAG_TILTY 0.40
#define FLAG_RADIAN (2 * M_PI / RETRO_SINCOS_ANGLE) // radians a table unit is worth
#define FLAG_SHADES ((RETRO_COLORS - 1) / 2) // palette entries a color's ramp covers, the
                                                 // two sharing everything past the background
#define FLAG_BLUE 1 // where the blue ramp starts, past the background
#define FLAG_GOLD (FLAG_BLUE + FLAG_SHADES)

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase, of the wave running out along the flag and of the yaw it
	// swings through
	double travel = fmod(time * FLAG_SPEED, RETRO_SINCOS_ANGLE);

	double sway = fmod(time * FLAG_SWAYSPEED, RETRO_SINCOS_ANGLE);

	// Shake out the cloth, and slope with it: the height and the two slopes come
	// from the same wave, so the shading is the surface's own and not an average
	// of whatever facets happen to meet
	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < model->vertices; i++) {
		float u = model->vertex[i].x / FLAG_SPAN + 0.5f;
		float v = model->vertex[i].y / FLAG_DROP + 0.5f;
		float a = travel + u * FLAG_RATEU + v * FLAG_RATEV;
		float b = 2 * travel + u * FLAG_RATEU2;
		float wave = SIN(a) + FLAG_HALF * SIN(b);
		float amp = FLAG_AMP / (1 + FLAG_HALF);

		model->vertex[i].z = amp * u * wave;

		float dx = amp * (wave + u * FLAG_RADIAN * (FLAG_RATEU * COS(a) + FLAG_HALF * FLAG_RATEU2 * COS(b))) / FLAG_SPAN;
		float dy = amp * u * FLAG_RADIAN * FLAG_RATEV * COS(a) / FLAG_DROP;
		// The slope is (dx, dy, -1) before scaling, so the length is never zero
		float inverselength = 1.0f / sqrt(dx * dx + dy * dy + 1);

		// The front of the sheet faces the viewer, which is -z, and the back is
		// the same normal turned around. Both go in unit, as every Direction is
		model->normal[i].x = dx * inverselength;
		model->normal[i].y = dy * inverselength;
		model->normal[i].z = -inverselength;
		model->normal[model->vertices + i].x = -dx * inverselength;
		model->normal[model->vertices + i].y = -dy * inverselength;
		model->normal[model->vertices + i].z = inverselength;
	}

	// Draw flag
	RETRO_RotateModel(FLAG_TILTX, FLAG_TILTY + FLAG_SWAY * SIN(sway), 0);
	RETRO_ProjectModel(FLAG_SCALE);
	RETRO_RenderModel(RETRO_POLY_GOURAUD);
}

void DEMO_Initialize(void)
{
	// Init palette, one ramp per color of the flag, each out of the dark it sits
	// against, so a face is placed in its own ramp by its own Lambert term
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreateGradientPalette(FLAG_BLUE, FLAG_BLUE + FLAG_SHADES, RETRO_BLUEBLACK, RETRO_CERULEAN);
	RETRO_CreateGradientPalette(FLAG_GOLD, FLAG_GOLD + FLAG_SHADES, RETRO_BLUEBLACK, RETRO_GOLD);

	Model3D *model = RETRO_Load3DModel("assets/flag.obj");
	model->c = FLAG_BLUE;
	model->shades = FLAG_SHADES;

	// The faces come in pairs, the two sides of one quad, in row major order.
	// A face is wholly one color, so the shade interpolated over it stays inside
	// that color's ramp and the cross keeps its edges
	for (int i = 0; i < model->faces; i++) {
		int cellx = i / 2 % FLAG_COLS / FLAG_SUBDIV;
		int celly = i / 2 / FLAG_COLS / FLAG_SUBDIV;

		model->face[i].c = (cellx >= 5 && cellx < 7) || (celly >= 4 && celly < 6) ? FLAG_SHADES : 0;
	}

	RETRO_InitializeLightSource(-0.4, -0.5, -0.77);
}
