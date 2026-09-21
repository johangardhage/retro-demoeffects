//
// Cube shadow
//
// A flat shaded cube turning in front of a checkerboard wall, casting a dark
// shadow on it. The cube is lit with one Lambert term per face, as in the
// flat shaded cube, by a light up and to the left of the camera.
//
// The wall is a rotator that turns slowly in its own plane: each pixel's
// offset from screen centre is rotated by angle, and the parity of its two
// cell indices, each floored to WALL_CELL, picks the cell's color.
//
// The shadow is a planar projection along the light. Each rotated vertex Q is
// carried along the light's direction L, from the light towards the wall,
// until its z reaches the wall's depth:
//
//   t = (WALLZ - Q.z) / L.z
//   shadow = Q + t L
//
// The flattened cube is then projected with the same camera and drawn in one
// dark color. Only the faces the light falls on come out front facing once
// flattened, and on a convex solid those alone cover the whole shadow, so the
// usual back face culling is right for it. The cube is drawn over it
// afterwards, rotated afresh, and hides whatever part of the shadow lies
// behind it. The wall goes in last, under both: every pixel still showing
// the wall or the shadow moves to its light cell variant where the cell is
// light.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 1.1 // radians a second, about each axis

#define SCALE 37.5
#define CX (RETRO_WIDTH / 2.0)
#define CY (RETRO_HEIGHT / 2.0 - 10)

#define WALLZ 3.0f // model units behind the cube's centre the wall stands
#define WALL_CELL 24.0 // pixels across one checker cell
#define WALL_SPEED 0.25 // radians a second the wall turns
#define SHADOW_LIGHT 0.4 // share of the wall's brightness left in the shadow

// The wall and the shadow on a dark cell; LIGHT_CELL further on, on a light one
#define WALL 0
#define SHADOW 1
#define LIGHT_CELL 2
#define CUBE 4

static void SetShadowColors(int index, RETRO_Palette wall)
{
	RETRO_SetColor(index + WALL, wall);
	RETRO_SetColor(index + SHADOW, (unsigned char)(wall.r * SHADOW_LIGHT), (unsigned char)(wall.g * SHADOW_LIGHT), (unsigned char)(wall.b * SHADOW_LIGHT));
}

// Every pixel's offset from centre, rotated by angle, then floored into
// cells whose combined parity is the checker. The wall and the shadow move
// to their light variants on a light cell; the cube is left as it is
static void DrawWall(double time)
{
	unsigned char *dest = RETRO_FrameBuffer();
	double angle = time * WALL_SPEED;
	double ca = cos(angle), sa = sin(angle);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double dy = (y + 0.5) - RETRO_HEIGHT / 2.0;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double dx = (x + 0.5) - RETRO_WIDTH / 2.0;
			double u = dx * ca - dy * sa;
			double v = dx * sa + dy * ca;
			int iu = (int)floor(u / WALL_CELL);
			int iv = (int)floor(v / WALL_CELL);
			if (((iu + iv) & 1) && dest[y * RETRO_WIDTH + x] < CUBE) {
				dest[y * RETRO_WIDTH + x] += LIGHT_CELL;
			}
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED * 0.7, 2 * M_PI);
	float az = fmod(time * ROTATION_SPEED * 0.4, 2 * M_PI);

	Model3D *model = RETRO_Get3DModel();

	// Draw shadow. The light source points from the cube towards the light,
	// so the shadow is cast the other way
	vec3 light = -RETRO_Render.lightsource.dir;
	RETRO_RotateModel(ax, ay, az);
	for (int i = 0; i < model->vertices; i++) {
		float t = (WALLZ - model->vertex[i].rpos.z) / light.z;
		model->vertex[i].rpos += light * t;
	}
	model->c = SHADOW;
	RETRO_ProjectModel(SCALE, CX, CY);
	RETRO_RenderModel(RETRO_POLY_FLAT);

	// Draw cube
	RETRO_RotateModel(ax, ay, az);
	model->c = CUBE;
	RETRO_ProjectModel(SCALE, CX, CY);
	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_FLAT);

	DrawWall(time);
}

void DEMO_Initialize(void)
{
	// Init palette: both cells, each also in shadow, and a matte ramp for the cube
	SetShadowColors(0, RETRO_BLACK);
	SetShadowColors(LIGHT_CELL, RETRO_WHITE);
	RETRO_CreatePhongPalette(CUBE, RETRO_COLORS, RETRO_DEEPPINK, 0.0f);

	Model3D *model = RETRO_Load3DModel("assets/cube.obj");
	model->shades = RETRO_COLORS - CUBE;

	RETRO_InitializeLightSource(-0.5f, -0.5f, -1);
}
