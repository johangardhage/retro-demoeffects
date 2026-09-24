//
// Cube shadow
//
// A red glass cube turning in place over a checkerboard wall that turns
// slowly in its own plane, independently of the cube.
//
// The wall is a rotator, not a texture: each pixel's offset from screen
// centre is rotated by angle, which turns the pattern the other way, and the
// parity of its two cell indices (each floored to WALL_CELL) picks one of
// two colors. There is no scale term, so the tiling never zooms, only turns.
//
// The shadow is a real planar projection, not a translated copy of the cube.
// A directional light travels from the light, through the cube, on to the
// wall. Each rotated vertex Q is carried along that direction until its z
// reaches the wall's depth:
//
//   t = (WALLZ - Q.z) / L.z
//   shadow = Q + t L
//
// so a vertex nearer the camera - farther from the wall - is carried
// sideways by more than one already close to it, which is what skews the
// cast shape away from the cube's own silhouette as it turns, rather than
// just sliding a copy of it sideways. The flattened positions are projected
// with the same camera as the cube. Only the faces the light falls on come
// out front facing once flattened, and on a convex solid those alone cover
// the whole shadow, each pixel of it exactly once.
//
// Lighting is worked out in linear light and only encoded to sRGB for the
// palette. Everything is lit by an ambient share A that reaches everywhere
// and a direct share D = 1 - A from the light. The light has a fixed tilt
// towards the wall as it orbits, so the wall's irradiance is a constant, and
// it is the unit everything else is measured in: a surface turned n = N·L
// towards the light receives
//
//   E = A + D n / n_wall
//
// Each face of the cube is a thin pane of red glass, which transmits T of the
// light through it and scatters R back out diffusely, from either side. A
// face the light reaches from outside (n > 0) gets E at |n|; one it reaches
// from inside has had it pass through a lit face first, and gets the direct
// share times T. Any ray through a closed convex shell crosses exactly two
// panes, so the wall behind the cube is lit by
//
//   shadowed = W (A + D T^2)
//
// and a pixel seen through the cube shows its front pane over its back pane
// over the wall, shadowed or not:
//
//   color = R E_front + T (R E_back + T wall)
//
// The palette is laid out so that each layer is an offset:
//
//   color = cell * LIGHT_CELL + shadowed * SHADOW + layer
//
// where layer is 0 for the bare wall, and 1 + front * SIDES + back for a
// pixel seen through the cube, front and back being the two sides of the
// cube it looks through. The frame is cleared to 0 and everything is added
// with the Glenz filler: each front face adds 1 + side * SIDES, each back
// face its side, the shadow adds SHADOW, and the wall pass
// adds LIGHT_CELL wherever the checker cell is light. The palette entries for
// the cube are worked out again every frame, since they follow the faces'
// lighting.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define CUBESHADOW_ROTATION_SPEED 1.1 // radians a second, about each axis

#define CUBESHADOW_SCALE 37.5
#define CUBESHADOW_CX (RETRO_WIDTH / 2.0)
#define CUBESHADOW_CY (RETRO_HEIGHT / 2.0 - 10)

#define CUBESHADOW_WALLZ 3.0f // model units behind the cube's centre the wall stands

#define CUBESHADOW_LIGHT_SPEED 0.6 // radians a second the light orbits the board
#define CUBESHADOW_LIGHT_RADIUS 0.8 // how far off axis it swings, in x and y
#define CUBESHADOW_LIGHT_Z -1.1f // fixed depth component, toward the camera side

#define CUBESHADOW_WALL_CELL 24.0 // pixels across one checker cell
#define CUBESHADOW_WALL_SPEED 0.25 // radians a second the wall turns

#define CUBESHADOW_DIRECT_LIGHT 0.65f // share of the wall's light that comes straight from the light, 0..1; the rest is ambient and still falls in the shadow
#define CUBESHADOW_GLASS_TRANSMIT vec3{ 0.75f, 0.08f, 0.08f } // linear light a pane lets through, per channel
#define CUBESHADOW_GLASS_SCATTER vec3{ 0.20f, 0.01f, 0.01f } // linear light a pane scatters out diffusely; with the transmission at most 1

#define CUBESHADOW_SIDES 6

#define CUBESHADOW_WALL 0
#define CUBESHADOW_SHADOW (1 + CUBESHADOW_SIDES * CUBESHADOW_SIDES)
#define CUBESHADOW_LIGHT_CELL (2 * CUBESHADOW_SHADOW)

// The side of the cube each face belongs to: faces with the same normal
static int CubeShadowSide[RETRO_MAX_FACES];

// Adds amount to every pixel under one face, at its projected position
static void CubeShadowAddFace(Model3D *model, Face *face, int amount)
{
	PolygonPoint point[RETRO_MAX_FACEVERTICES];
	for (int j = 0; j < face->vertices; j++) {
		point[j].pos = model->vertex[face->vertex[j]].spos;
	}
	RETRO_DrawGlenzPolygon(point, face->vertices, amount, RETRO_COLORS);
}

// Every pixel's offset from centre, rotated by angle, then floored into
// cells whose combined parity is the checker. Whatever was drawn over a light
// cell moves to its light variant
static void CubeShadowDrawWall(double time)
{
	unsigned char *dest = RETRO_FrameBuffer();
	double angle = time * CUBESHADOW_WALL_SPEED;
	double ca = cos(angle), sa = sin(angle);
	for (int y = 0; y < RETRO_HEIGHT; y++) {
		double dy = (y + 0.5) - RETRO_HEIGHT / 2.0;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			double dx = (x + 0.5) - RETRO_WIDTH / 2.0;
			double u = dx * ca - dy * sa;
			double v = dx * sa + dy * ca;
			int iu = (int)floor(u / CUBESHADOW_WALL_CELL);
			int iv = (int)floor(v / CUBESHADOW_WALL_CELL);
			if ((iu + iv) & 1) {
				dest[y * RETRO_WIDTH + x] += CUBESHADOW_LIGHT_CELL;
			}
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Calculate rotation
	float ax = fmod(time * CUBESHADOW_ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * CUBESHADOW_ROTATION_SPEED * 0.7, 2 * M_PI);
	float az = fmod(time * CUBESHADOW_ROTATION_SPEED * 0.4, 2 * M_PI);

	Model3D *model = RETRO_Get3DModel();
	RETRO_RotateModel(ax, ay, az);

	// Orbit the light around the board. tolight points from the cube back
	// toward the light, for the irradiance of each surface; the shadow travels
	// the other way, out from the light and through the cube to the wall
	vec3 tolight = normalize(RETRO_RotateLightSource(time, CUBESHADOW_LIGHT_SPEED, CUBESHADOW_LIGHT_RADIUS, CUBESHADOW_LIGHT_Z));
	vec3 light = -tolight;

	// Irradiance of the wall, whose normal faces the camera, and of each side
	float ambient = 1.0f - CUBESHADOW_DIRECT_LIGHT;
	float nwall = -tolight.z;
	vec3 transmit = CUBESHADOW_GLASS_TRANSMIT;
	vec3 scatter = CUBESHADOW_GLASS_SCATTER;

	// Which way each side faces, and its pane's color. Both faces of a side
	// agree, so the second simply writes the same again
	RETRO_ProjectModel(CUBESHADOW_SCALE, CUBESHADOW_CX, CUBESHADOW_CY);
	RETRO_SortFaces(true, model);
	bool front[CUBESHADOW_SIDES];
	vec3 pane[CUBESHADOW_SIDES];
	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		int side = CubeShadowSide[model->drawface[i]];
		front[side] = face->frontfacing;
		float n = dot(face->facenormal.rdir, tolight);
		vec3 direct = vec3{ 1, 1, 1 } * (CUBESHADOW_DIRECT_LIGHT * fabs(n) / nwall);
		vec3 irradiance = vec3{ ambient, ambient, ambient } + (n > 0 ? direct : direct * transmit);
		pane[side] = scatter * irradiance;
	}

	// Palette for this frame: the wall, lit or in shadow, and the wall seen
	// through every pair of front and back pane, once for each cell color
	vec3 shadowlight = vec3{ ambient, ambient, ambient } + transmit * transmit * CUBESHADOW_DIRECT_LIGHT;
	for (int cell = 0; cell < 2; cell++) {
		for (int shadowed = 0; shadowed < 2; shadowed++) {
			int base = cell * CUBESHADOW_LIGHT_CELL + shadowed * CUBESHADOW_SHADOW;
			vec3 wall = vec3{ 1, 1, 1 } * (float)cell;
			if (shadowed) wall = wall * shadowlight;
			RETRO_SetColor(base + CUBESHADOW_WALL, RETRO_LinearToColor(wall));
			for (int f = 0; f < CUBESHADOW_SIDES; f++) {
				if (!front[f]) continue;
				for (int b = 0; b < CUBESHADOW_SIDES; b++) {
					if (front[b]) continue;
					vec3 color = pane[f] + transmit * (pane[b] + transmit * wall);
					RETRO_SetColor(base + 1 + f * CUBESHADOW_SIDES + b, RETRO_LinearToColor(color));
				}
			}
		}
	}

	// Draw the glass cube: every face adds its side's step, so each pixel ends
	// up naming the front and back pane it is seen through
	for (int i = 0; i < model->drawfaces; i++) {
		int side = CubeShadowSide[model->drawface[i]];
		CubeShadowAddFace(model, &model->face[model->drawface[i]], front[side] ? 1 + side * CUBESHADOW_SIDES : side);
	}

	// Draw shadow: flatten the rotated vertices onto the wall along light and
	// add the shadow offset under the faces that come out front facing. The
	// cube is already drawn, and the next RETRO_RotateModel starts over from
	// the model's own positions, so they are flattened in place
	for (int i = 0; i < model->vertices; i++) {
		float t = (CUBESHADOW_WALLZ - model->vertex[i].rpos.z) / light.z;
		model->vertex[i].rpos += light * t;
	}
	RETRO_ProjectModel(CUBESHADOW_SCALE, CUBESHADOW_CX, CUBESHADOW_CY);
	RETRO_SortFaces(false, model);
	for (int i = 0; i < model->drawfaces; i++) {
		CubeShadowAddFace(model, &model->face[model->drawface[i]], CUBESHADOW_SHADOW);
	}

	CubeShadowDrawWall(time);
}

void DEMO_Initialize(void)
{
	Model3D *model = RETRO_Load3DModel("assets/cube.obj");

	// Group the faces into sides by their normals, CUBESHADOW_SIDES of them
	int sides = 0;
	for (int i = 0; i < model->faces; i++) {
		CubeShadowSide[i] = -1;
		for (int j = 0; j < i && CubeShadowSide[i] < 0; j++) {
			if (dot(model->face[i].facenormal.dir, model->face[j].facenormal.dir) > 0.999f) {
				CubeShadowSide[i] = CubeShadowSide[j];
			}
		}
		if (CubeShadowSide[i] < 0) {
			CubeShadowSide[i] = sides++;
		}
	}
}
