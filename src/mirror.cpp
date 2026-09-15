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
#define MIRROR_HOVER 1.20f // model units the centre sits above the platform

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

static void AddVertex(Model3D *model, float x, float y, float z)
{
	if (model->vertices >= RETRO_MAX_VERTICES) {
		RETRO_RageQuit("Too many vertices\n");
	}
	model->vertex[model->vertices].pos = { x, y, z };
	model->vertices++;
}

static void AddQuad(Model3D *model, int a, int b, int c, int d, int color)
{
	if (model->faces >= RETRO_MAX_FACES) {
		RETRO_RageQuit("Too many faces\n");
	}
	Face *face = &model->face[model->faces++];
	face->vertices = 4;
	face->vertex[0] = a;
	face->vertex[1] = b;
	face->vertex[2] = c;
	face->vertex[3] = d;
	face->c = color;
	face->backc = 0;
}

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
			AddVertex(model, x0, y0, z0);
			AddVertex(model, x1, y0, z0);
			AddVertex(model, x1, y0, z1);
			AddVertex(model, x0, y0, z1);
			AddQuad(model, v, v + 1, v + 2, v + 3, color);
		}
	}

	// Front side, z = −extent, toward the camera
	for (int col = 0; col < FLOOR_TILES; col++) {
		float x0 = origin + col * FLOOR_TILE;
		float x1 = x0 + FLOOR_TILE;
		float z = origin;
		int color = (col + 1) & 1;
		int v = model->vertices;
		AddVertex(model, x0, y0, z);
		AddVertex(model, x1, y0, z);
		AddVertex(model, x1, y1, z);
		AddVertex(model, x0, y1, z);
		AddQuad(model, v, v + 3, v + 2, v + 1, color);
	}

	// Back side
	for (int col = 0; col < FLOOR_TILES; col++) {
		float x0 = origin + col * FLOOR_TILE;
		float x1 = x0 + FLOOR_TILE;
		float z = FLOOR_EXTENT;
		int color = col & 1;
		int v = model->vertices;
		AddVertex(model, x0, y0, z);
		AddVertex(model, x0, y1, z);
		AddVertex(model, x1, y1, z);
		AddVertex(model, x1, y0, z);
		AddQuad(model, v, v + 1, v + 2, v + 3, color);
	}

	// Left side
	for (int row = 0; row < FLOOR_TILES; row++) {
		float z0 = origin + row * FLOOR_TILE;
		float z1 = z0 + FLOOR_TILE;
		float x = origin;
		int color = (row + 1) & 1;
		int v = model->vertices;
		AddVertex(model, x, y0, z0);
		AddVertex(model, x, y1, z0);
		AddVertex(model, x, y1, z1);
		AddVertex(model, x, y0, z1);
		AddQuad(model, v, v + 1, v + 2, v + 3, color);
	}

	// Right side
	for (int row = 0; row < FLOOR_TILES; row++) {
		float z0 = origin + row * FLOOR_TILE;
		float z1 = z0 + FLOOR_TILE;
		float x = FLOOR_EXTENT;
		int color = row & 1;
		int v = model->vertices;
		AddVertex(model, x, y0, z0);
		AddVertex(model, x, y0, z1);
		AddVertex(model, x, y1, z1);
		AddVertex(model, x, y1, z0);
		AddQuad(model, v, v + 1, v + 2, v + 3, color);
	}

	RETRO_InitializeFaceNormals(model);
}

static void BuildMirror(Model3D *model)
{
	float hx = MIRROR_HX, hy = MIRROR_HY, hz = MIRROR_HZ;
	AddVertex(model, -hx, -hy, -hz); // 0
	AddVertex(model, hx, -hy, -hz);  // 1
	AddVertex(model, hx, hy, -hz);   // 2
	AddVertex(model, -hx, hy, -hz);  // 3
	AddVertex(model, -hx, -hy, hz);  // 4
	AddVertex(model, hx, -hy, hz);   // 5
	AddVertex(model, hx, hy, hz);    // 6
	AddVertex(model, -hx, hy, hz);   // 7

	// Outward winding. Opposite ±Z faces are the glass (face.c == 0).
	AddQuad(model, 0, 3, 2, 1, 0); // −Z
	AddQuad(model, 4, 5, 6, 7, 0); // +Z
	AddQuad(model, 1, 2, 6, 5, COL_RIM); // +X
	AddQuad(model, 0, 4, 7, 3, COL_RIM); // −X
	AddQuad(model, 2, 3, 7, 6, COL_RIM); // +Y
	AddQuad(model, 0, 1, 5, 4, COL_RIM); // −Y

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

static unsigned char ReflectFloor(vec3 p, vec3 n)
{
	vec3 incident = p - CameraScene;
	float idotn = dot(incident, n);
	vec3 reflected = incident - n * (2.0f * idotn);

	if (fabsf(reflected.y) < 1.0e-6f) {
		return COL_GLASS;
	}

	float t = -p.y / reflected.y;
	if (t < 1.0e-4f) {
		return COL_GLASS;
	}

	return SampleFloor(p.x + t * reflected.x, p.z + t * reflected.z);
}

// Perspective-correct scene position, then a planar bounce onto y = 0.
// n holds P · q at the corners so dividing by the interpolated q recovers P.
static void DrawGlassPolygon(PolygonPoint *point, int points, vec3 normal)
{
	for (int triangle = 1; triangle < points - 1; triangle++) {
		PolygonPoint *p0 = &point[0];
		PolygonPoint *p1 = &point[triangle];
		PolygonPoint *p2 = &point[triangle + 1];
		TriangleSpan span[RETRO_HEIGHT];
		int ystart, yend;
		float determinant = RETRO_ScanTriangle(p0, p1, p2, span, ystart, yend);
		if (determinant == 0.0f) continue;

		float dqdx = ((p1->q - p0->q) * (p2->pos.y - p0->pos.y) - (p2->q - p0->q) * (p1->pos.y - p0->pos.y)) / determinant;
		float dqdy = ((p1->pos.x - p0->pos.x) * (p2->q - p0->q) - (p2->pos.x - p0->pos.x) * (p1->q - p0->q)) / determinant;
		vec3 dndx = ((p1->n - p0->n) * (p2->pos.y - p0->pos.y) - (p2->n - p0->n) * (p1->pos.y - p0->pos.y)) / determinant;
		vec3 dndy = ((p1->pos.x - p0->pos.x) * (p2->n - p0->n) - (p2->pos.x - p0->pos.x) * (p1->n - p0->n)) / determinant;

		for (int y = ystart; y < yend; y++) {
			if (span[y].left > span[y].right) continue;
			int xstart = MAX((int)ceil(span[y].left - 0.5f), 0);
			int xend = MIN((int)ceil(span[y].right - 0.5f), RETRO_WIDTH);
			float px = xstart + 0.5f;
			float py = y + 0.5f;
			float q = p0->q + dqdx * (px - p0->pos.x) + dqdy * (py - p0->pos.y);
			vec3 pq = p0->n + dndx * (px - p0->pos.x) + dndy * (py - p0->pos.y);

			for (int x = xstart; x < xend; x++) {
				int offset = y * RETRO_WIDTH + x;
				if (RETRO_DepthTest(offset, q) && q != 0.0f) {
					vec3 p = pq / q;
					RETRO.framebuffer[offset] = ReflectFloor(p, normal);
				}
				q += dqdx;
				pq += dndx;
			}
		}
	}
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
			point[j].n = vertex->pos * vertex->q;
		}

		if (face->c == COL_RIM) {
			RETRO_DrawFlatPolygon(point, face->vertices, COL_RIM);
			continue;
		}

		vec3 normal = face->facenormal.dir;
		DrawGlassPolygon(point, face->vertices, normal);
	}
}

void DEMO_Render(double time, double deltatime)
{
	float ax = fmod(time * TUMBLE_X, 2 * M_PI);
	float ay = fmod(time * TUMBLE_Y, 2 * M_PI);
	float az = fmod(time * TUMBLE_Z, 2 * M_PI);

	for (int i = 0; i < Mirror->vertices; i++) {
		Vertex v = MirrorRest[i];
		RETRO_RotateVertex(&v, ax, ay, az);
		Mirror->vertex[i].pos = v.rpos - vec3{ 0, MIRROR_HOVER, 0 };
	}
	RETRO_InitializeFaceNormals(Mirror);

	float c = cos(WORLD_TILT);
	float s = sin(WORLD_TILT);
	CameraScene = { 0.0f, -SCENE_Y * c - SCENE_Z * s, SCENE_Y * s - SCENE_Z * c };

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
