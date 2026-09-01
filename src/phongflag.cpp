//
// Waving flag, phong shaded
//
// The same flag and the same analytic normals as gouraudflag.cpp, but the
// normal is interpolated across a face and lit at every pixel instead of being
// lit at the corners and the shade interpolated. Cloth is where that difference
// shows: a sheen sits on the crest of a fold, which is inside a quad and not on
// its corners, so gouraud walks straight past it and phong draws it moving over
// the cloth as the wave runs out.
//
// Sheen, not plastic. RETRO_K_SPECULAR over RETRO_K_FALLOFF is a tight white
// highlight, which on a mesh this coarse would be a bright spot crossing one
// quad at a time. SHEEN is weaker and FALLOFF far broader, so the highlight is
// wide enough that the cloth carries it.
//
// The light does not sit where the other two flags put it. A highlight is only
// drawn where the surface faces the light, and this surface cannot face just
// anywhere: the wave turns the normal by about 64 degrees across the flag but
// only 17 down it, so a light steep in y is never met and no sheen appears at
// all. LIGHT is inside the cone the cloth sweeps, which puts a tenth of it
// within the highlight at any moment.
//
// Two materials in one palette: the phong renderer shades a face from
// model->c + face->c, so the same face color that picks a ramp for the flat and
// gouraud flags picks one here, and blue and gold each get their own phong ramp
// out of the dark.
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
#define FLAG_SHEEN 0.35 // how much of the highlight the cloth returns
#define FLAG_FALLOFF 8 // how far it spreads, against RETRO_K_FALLOFF's 150
#define FLAG_LIGHTX -0.7 // a direction the cloth reaches, so the sheen is lit
#define FLAG_LIGHTY -0.22
#define FLAG_LIGHTZ -1.0
#define FLAG_SHADES 127 // palette entries a color's ramp covers
#define FLAG_BLUE 1 // where the blue ramp starts, past the background
#define FLAG_GOLD (FLAG_BLUE + FLAG_SHADES)

void DEMO_Render(double deltatime)
{
	// Calculate phase, of the wave running out along the flag and of the yaw it
	// swings through
	static double travel = 0;
	travel = fmod(travel + deltatime * FLAG_SPEED, RETRO_SINCOS_ANGLE);

	static double sway = 0;
	sway = fmod(sway + deltatime * FLAG_SWAYSPEED, RETRO_SINCOS_ANGLE);

	// Shake out the cloth, and slope with it: the height and the two slopes come
	// from the same wave, so the normal the highlight rides on is the surface's
	// own at every vertex, and phong keeps it exact between them
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
	RETRO_RenderModel(RETRO_POLY_PHONG);
}

void DEMO_Initialize(void)
{
	// Init palette, a phong ramp per color of the flag, each out of the dark it
	// sits against, so a face is shaded within its own material
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_CreatePhongPalette(FLAG_BLUE, FLAG_BLUE + FLAG_SHADES, RETRO_CERULEAN, FLAG_SHEEN, FLAG_FALLOFF);
	RETRO_CreatePhongPalette(FLAG_GOLD, FLAG_GOLD + FLAG_SHADES, RETRO_GOLD, FLAG_SHEEN, FLAG_FALLOFF);

	Model3D *model = RETRO_Load3DModel("assets/flag.obj");
	model->c = FLAG_BLUE;
	model->shades = FLAG_SHADES;

	// The faces come in pairs, the two sides of one quad, in row major order
	for (int i = 0; i < model->faces; i++) {
		int cellx = i / 2 % FLAG_COLS / FLAG_SUBDIV;
		int celly = i / 2 / FLAG_COLS / FLAG_SUBDIV;

		model->face[i].c = (cellx >= 5 && cellx < 7) || (celly >= 4 && celly < 6) ? FLAG_SHADES : 0;
	}

	RETRO_InitializeLightSource(FLAG_LIGHTX, FLAG_LIGHTY, FLAG_LIGHTZ);
}
