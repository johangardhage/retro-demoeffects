//
// Waving flag, flat filled
//
// The same flag and the same wave as flatshadedflag.cpp with the light taken
// away: one constant color per face, so blue is blue wherever it lies. The wave
// is left to read from the silhouette rippling and from the cross bending
// across it, which is what a flag looked like before anyone could afford a
// lambert term per face.
//
// Giving the reverse of the cloth its own colors, the way flatcube.cpp gives
// each side of the cube one, buys nothing here: this wave turns the surface by
// about 64 degrees at the fly, and with the flag yawed no further than SWAY it
// only just passes edge on at the ends of the sway. Measured over a sway cycle
// the far side is a few dozen pixels in one frame out of twelve, so the second
// pair of colors would be specks rather than a fold.
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
#define FLAG_BLUE 1 // the two colors of the flag, past the background
#define FLAG_GOLD 2

void DEMO_Render(double time, double deltatime)
{
	// Calculate phase, of the wave running out along the flag and of the yaw it
	// swings through
	double travel = fmod(time * FLAG_SPEED, RETRO_SINCOS_ANGLE);

	double sway = fmod(time * FLAG_SWAYSPEED, RETRO_SINCOS_ANGLE);

	// Shake out the cloth. No normals are taken: an unlit face carries its color
	// and nothing else
	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < model->vertices; i++) {
		float u = model->vertex[i].x / FLAG_SPAN + 0.5f;
		float v = model->vertex[i].y / FLAG_DROP + 0.5f;
		float wave = SIN(travel + u * FLAG_RATEU + v * FLAG_RATEV) + FLAG_HALF * SIN(2 * travel + u * FLAG_RATEU2);

		model->vertex[i].z = FLAG_AMP * u * wave / (1 + FLAG_HALF);
	}

	// Draw flag
	RETRO_RotateModel(FLAG_TILTX, FLAG_TILTY + FLAG_SWAY * SIN(sway), 0);
	RETRO_ProjectModel(FLAG_SCALE);
	RETRO_RenderModel(RETRO_POLY_FLAT);
}

void DEMO_Initialize(void)
{
	// Init palette, the two colors of the flag and nothing else
	RETRO_SetColor(0, RETRO_BLACK);
	RETRO_SetColor(FLAG_BLUE, RETRO_CERULEAN);
	RETRO_SetColor(FLAG_GOLD, RETRO_GOLD);

	Model3D *model = RETRO_Load3DModel("assets/flag.obj");
	model->c = FLAG_BLUE;

	// The faces come in pairs, the two sides of one quad, in row major order
	for (int i = 0; i < model->faces; i++) {
		int cellx = i / 2 % FLAG_COLS / FLAG_SUBDIV;
		int celly = i / 2 / FLAG_COLS / FLAG_SUBDIV;

		model->face[i].c = (cellx >= 5 && cellx < 7) || (celly >= 4 && celly < 6) ? FLAG_GOLD - FLAG_BLUE : 0;
	}
}
