//
// Mirror
//
// A thin square box tumbling over a 6×6 checkerboard platform. Two
// opposite faces are a planar mirror: a pixel on the glass takes the
// view ray, reflects it in the face normal, and samples the floor plane
// y = 0. A hit inside the board is the same checker the platform uses;
// a miss is the sky. The other four faces are opaque magenta. The
// platform is a box, top and sides, so the front row of tiles has
// thickness. Both models share a q-buffer. Faces are wound outward so
// the magenta sides close the box instead of showing the inside.
//
// Scene space is after the tumble and before the shared pitch. The
// camera is the inverse of that pitch and the scene translation, so
// the reflection can be taken with the floor still at y = 0. Scene
// position is interpolated * q so the hit is perspective-correct.
// Euler angles live on 2π.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define FLOOR_TILES 6
#define FLOOR_TILE 1.0f // model units a checker square spans
#define FLOOR_THICKNESS 0.48f // model units of the platform's side
#define FLOOR_EXTENT (FLOOR_TILES * FLOOR_TILE / 2)

#define MIRROR_HX 0.95f // half-extent of the square face
#define MIRROR_HY 0.95f
#define MIRROR_HZ 0.18f // half-thickness, much thinner than a cube
#define MIRROR_HOVER 1.20f // model units the center sits above the platform

#define WORLD_TILT 0.50f // radians of pitch, looking down onto the board
#define SCENE_Y 1.55f // model units the scene is shifted down the screen
#define SCENE_Z 3.90f // and away from the camera
#define SCENE_SCALE 44

#define TUMBLE_X 0.55f // radians a second about each axis
#define TUMBLE_Y 0.92f
#define TUMBLE_Z 0.37f

#define COL_BG 0
#define COL_FLOOR_DARK 1
#define COL_FLOOR_LIGHT 2
#define COL_GLASS 3
#define COL_RIM 4

static Model3D *Floor;
static Model3D *Mirror;
static Vertex MirrorRest[RETRO_MAX_VERTICES];
static vec3 CameraScene;

static void BuildFloor(Model3D *model)
{
	float y0 = 0.0f;
	float y1 = FLOOR_THICKNESS;
	float origin = -FLOOR_EXTENT;

	for (int row = 0; row < FLOOR_TILES; row++) {
		for (int col = 0; col < FLOOR_TILES; col++) {
			float x0 = origin + col * FLOOR_TILE;
			float x1 = x0 + FLOOR_TILE;
			float z0 = origin + row * FLOOR_TILE;
			float z1 = z0 + FLOOR_TILE;
			int color = (row + col + 1) & 1;

			int v = model->vertices;
			RETRO_AddModelVertex(model, x0, y0, z0);
			RETRO_AddModelVertex(model, x1, y0, z0);
			RETRO_AddModelVertex(model, x1, y0, z1);
			RETRO_AddModelVertex(model, x0, y0, z1);
			RETRO_AddModelQuad(model, v, v + 1, v + 2, v + 3, color);
		}
	}

	// Front side, z = −extent, toward the camera
	for (int col = 0; col < FLOOR_TILES; col++) {
		float x0 = origin + col * FLOOR_TILE;
		float x1 = x0 + FLOOR_TILE;
		float z = origin;
		int color = (col + 1) & 1;
		int v = model->vertices;
		RETRO_AddModelVertex(model, x0, y0, z);
		RETRO_AddModelVertex(model, x1, y0, z);
		RETRO_AddModelVertex(model, x1, y1, z);
		RETRO_AddModelVertex(model, x0, y1, z);
		RETRO_AddModelQuad(model, v, v + 3, v + 2, v + 1, color);
	}

	// Back side
	for (int col = 0; col < FLOOR_TILES; col++) {
		float x0 = origin + col * FLOOR_TILE;
		float x1 = x0 + FLOOR_TILE;
		float z = FLOOR_EXTENT;
		int color = col & 1;
		int v = model->vertices;
		RETRO_AddModelVertex(model, x0, y0, z);
		RETRO_AddModelVertex(model, x0, y1, z);
		RETRO_AddModelVertex(model, x1, y1, z);
		RETRO_AddModelVertex(model, x1, y0, z);
		RETRO_AddModelQuad(model, v, v + 1, v + 2, v + 3, color);
	}

	// Left side
	for (int row = 0; row < FLOOR_TILES; row++) {
		float z0 = origin + row * FLOOR_TILE;
		float z1 = z0 + FLOOR_TILE;
		float x = origin;
		int color = (row + 1) & 1;
		int v = model->vertices;
		RETRO_AddModelVertex(model, x, y0, z0);
		RETRO_AddModelVertex(model, x, y1, z0);
		RETRO_AddModelVertex(model, x, y1, z1);
		RETRO_AddModelVertex(model, x, y0, z1);
		RETRO_AddModelQuad(model, v, v + 1, v + 2, v + 3, color);
	}

	// Right side
	for (int row = 0; row < FLOOR_TILES; row++) {
		float z0 = origin + row * FLOOR_TILE;
		float z1 = z0 + FLOOR_TILE;
		float x = FLOOR_EXTENT;
		int color = row & 1;
		int v = model->vertices;
		RETRO_AddModelVertex(model, x, y0, z0);
		RETRO_AddModelVertex(model, x, y0, z1);
		RETRO_AddModelVertex(model, x, y1, z1);
		RETRO_AddModelVertex(model, x, y1, z0);
		RETRO_AddModelQuad(model, v, v + 1, v + 2, v + 3, color);
	}

	RETRO_InitializeFaceNormals(model);
}

static void BuildMirror(Model3D *model)
{
	float hx = MIRROR_HX, hy = MIRROR_HY, hz = MIRROR_HZ;
	RETRO_AddModelVertex(model, -hx, -hy, -hz); // 0
	RETRO_AddModelVertex(model, hx, -hy, -hz);  // 1
	RETRO_AddModelVertex(model, hx, hy, -hz);   // 2
	RETRO_AddModelVertex(model, -hx, hy, -hz);  // 3
	RETRO_AddModelVertex(model, -hx, -hy, hz);  // 4
	RETRO_AddModelVertex(model, hx, -hy, hz);   // 5
	RETRO_AddModelVertex(model, hx, hy, hz);    // 6
	RETRO_AddModelVertex(model, -hx, hy, hz);   // 7

	// Outward winding. Opposite ±Z faces are the glass (face.c == 0).
	RETRO_AddModelQuad(model, 0, 3, 2, 1, 0); // −Z
	RETRO_AddModelQuad(model, 4, 5, 6, 7, 0); // +Z
	RETRO_AddModelQuad(model, 1, 2, 6, 5, COL_RIM); // +X
	RETRO_AddModelQuad(model, 0, 4, 7, 3, COL_RIM); // −X
	RETRO_AddModelQuad(model, 2, 3, 7, 6, COL_RIM); // +Y
	RETRO_AddModelQuad(model, 0, 1, 5, 4, COL_RIM); // −Y

	RETRO_InitializeFaceNormals(model);
}

static void PlaceScene(Model3D *model)
{
	RETRO_RotateModel(WORLD_TILT, 0, 0, model);
	RETRO_TranslateModel(0, SCENE_Y, SCENE_Z, model);
	RETRO_ProjectModel(SCENE_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, model);
}

static unsigned char SampleFloor(float x, float z)
{
	if (x < -FLOOR_EXTENT || x >= FLOOR_EXTENT || z < -FLOOR_EXTENT || z >= FLOOR_EXTENT) {
		return COL_BG;
	}

	int col = (int)((x + FLOOR_EXTENT) / FLOOR_TILE);
	int row = (int)((z + FLOOR_EXTENT) / FLOOR_TILE);
	col = CLAMP(col, 0, FLOOR_TILES);
	row = CLAMP(row, 0, FLOOR_TILES);
	return COL_FLOOR_DARK + ((row + col + 1) & 1);
}

// One pixel of glass: a planar bounce of the view ray onto y = 0.
static unsigned char ReflectFloor(const Fragment &fragment)
{
	vec3 p = fragment.position;
	vec3 reflected = reflect(fragment.view, fragment.normal);

	if (fabsf(reflected.y) < 1.0e-6f) {
		return COL_GLASS;
	}

	float t = -p.y / reflected.y;
	if (t < 1.0e-4f) {
		return COL_GLASS;
	}

	return SampleFloor(p.x + t * reflected.x, p.z + t * reflected.z);
}

static void RenderMirror(void)
{
	RETRO_SortFaces(false, Mirror);

	for (int i = 0; i < Mirror->drawfaces; i++) {
		Face *face = &Mirror->face[Mirror->drawface[i]];
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &Mirror->vertex[face->vertex[j]];
			point[j].pos = vertex->spos;
			point[j].q = vertex->q;
			point[j].p = vertex->pos * vertex->q;
			point[j].n = face->facenormal.dir;
		}

		if (face->c == COL_RIM) {
			RETRO_DrawFlatPolygon(point, face->vertices, COL_RIM);
			continue;
		}

		// The scene position is perspective-correct: p holds P · q at the
		// corners, and dividing by the interpolated q recovers P.
		RETRO_DrawShaderPolygon(point, face->vertices, CameraScene, ReflectFloor);
	}
}

void DEMO_Render(RETRO_Time time)
{
	float ax = fmod(time.total * TUMBLE_X, 2 * M_PI);
	float ay = fmod(time.total * TUMBLE_Y, 2 * M_PI);
	float az = fmod(time.total * TUMBLE_Z, 2 * M_PI);

	mat3 tumble = rotate(ax, ay, az);
	for (int i = 0; i < Mirror->vertices; i++) {
		Mirror->vertex[i].pos = tumble * MirrorRest[i].pos - vec3{ 0, MIRROR_HOVER, 0 };
	}
	RETRO_InitializeFaceNormals(Mirror);

	mat3 tilt = rotateX(WORLD_TILT);
	CameraScene = -(transpose(tilt) * vec3{ 0.0f, SCENE_Y, SCENE_Z });

	PlaceScene(Floor);
	PlaceScene(Mirror);

	RETRO_RenderModel(RETRO_POLY_FLAT, RETRO_SHADE_NONE, Floor);
	RenderMirror();
}

void DEMO_Initialize(void)
{
	RETRO_SetColor(COL_BG, RETRO_INDIGOBLACK);
	RETRO_SetColor(COL_FLOOR_DARK, RETRO_NAVY);
	RETRO_SetColor(COL_FLOOR_LIGHT, RETRO_DEEPCERULEAN);
	RETRO_SetColor(COL_GLASS, RETRO_ONYX);
	RETRO_SetColor(COL_RIM, RETRO_ROYALVIOLET);

	Floor = RETRO_Allocate3DModel();
	Floor->c = COL_FLOOR_DARK;
	BuildFloor(Floor);

	Mirror = RETRO_Allocate3DModel();
	BuildMirror(Mirror);
	for (int i = 0; i < Mirror->vertices; i++) {
		MirrorRest[i] = Mirror->vertex[i];
	}
}
