//
// Waving flag, flat shaded
//
// The same flag as flag.cpp, as polygons rather than pixels: assets/flag.obj is
// a grid of quads with one flat color each, and the wave is in the geometry
// instead of in a height field sampled per pixel.
//
// The asset is a flat sheet. Every frame the wave is written into z,
//
//   z = AMP u (sin(travel + u RATEU + v RATEV) + HALF sin(2 travel + u RATEU2))
//
// with u across the flag and v down it, so the hoist is nailed to its mast and
// the fly is free to throw the furthest. The second mode runs at twice the
// phase speed of the first, a whole multiple, so both close together when
// travel wraps. Deforming a mesh invalidates the face normals the loader took
// from it, so they are taken again before the model is rotated and lit: that
// one call is the difference between this and any other flat shaded model.
//
// The color is the flag and the shade is the light. SFS 1982:269 divides the
// flag 5:2:9 across and 4:2:4 down, so the cross is the two cells after the
// fifth across and after the fourth down, which is a test rather than a
// picture. SUBDIV quads cover each cell exactly, so a face is only ever wholly
// blue or wholly gold and the cross stays sharp however coarse the mesh is.
// Each color owns a ramp: the flat renderer places a face at c + face->c +
// ShadeFromLambert(N . L) * shades, so the face color picks the ramp and the
// Lambert term picks the entry within it.
//
// The asset carries each quad twice, once in each winding, because the
// renderer culls back faces and a flag is seen from both sides. Without the
// reverse copy a fold would open a hole.
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

	// Shake out the cloth, then take the face normals again: they are the sheet's
	// until the wave is written into it
	Model3D *model = RETRO_Get3DModel();
	for (int i = 0; i < model->vertices; i++) {
		float u = model->vertex[i].x / FLAG_SPAN + 0.5f;
		float v = model->vertex[i].y / FLAG_DROP + 0.5f;
		float wave = SIN(travel + u * FLAG_RATEU + v * FLAG_RATEV) + FLAG_HALF * SIN(2 * travel + u * FLAG_RATEU2);

		model->vertex[i].z = FLAG_AMP * u * wave / (1 + FLAG_HALF);
	}
	RETRO_InitializeFaceNormals(model);

	// Draw flag
	RETRO_RotateModel(FLAG_TILTX, FLAG_TILTY + FLAG_SWAY * SIN(sway), 0);
	RETRO_ProjectModel(FLAG_SCALE);
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);
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

	// The faces come in pairs, the two sides of one quad, in row major order
	for (int i = 0; i < model->faces; i++) {
		int cellx = i / 2 % FLAG_COLS / FLAG_SUBDIV;
		int celly = i / 2 / FLAG_COLS / FLAG_SUBDIV;

		model->face[i].c = (cellx >= 5 && cellx < 7) || (celly >= 4 && celly < 6) ? FLAG_SHADES : 0;
	}

	RETRO_InitializeLightSource(-0.4, -0.5, -0.77);
}
