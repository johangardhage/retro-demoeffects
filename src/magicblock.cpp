//
// Magic block
//
// A cube turns through a slab and vanishes wherever it is inside it. The slab's
// two broad faces are drawn far one first, and after each one the part of the
// cube beyond that face, clipped against its plane,
//
//   keep p where n · p < −h
//
// with n the unit normal pointing from that face into the slab and h the
// slab's half thickness. The cube has inside faces too, wound the other way,
// so the cut opens onto its hollow interior. The four edge faces go last.
//
// No write replaces a whole pixel unless it has to. The cube replaces all four
// bits of the palette index, the slab only bits 2 and 3, so the part of the
// cube behind the near face keeps its low two bits and shows through it as
// purple. There is no depth buffer: the draw order is the whole of it.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retropalette.h"

#define BLOCK_SPEED_X  0.86f
#define BLOCK_SPEED_Y -0.43f
#define BLOCK_SPEED_Z  0.43f

#define CUBE_SPEED_X  1.29f
#define CUBE_SPEED_Y -0.43f
#define CUBE_SPEED_Z  1.29f

#define BLOCK_HALF_THICKNESS 0.25f // magicblock.obj's broad faces sit at z = ±0.25

#define STAR_COUNT 81
#define STAR_FIELD 4.0f // stars fill a cube this far from the centre along each axis
#define STAR_SPEED 2.5f // along x, units a second
#define STAR_ROTATION_SPEED 0.43f

// The block replaces index bits 2 and 3 only, so the blue it covers keeps its
// low two bits and shows through as purple. The cube replaces all four.
#define BLOCK_MASK 12
#define CUBE_MASK 15

// With the library's default eye distance of 250, the eye sits 250 / 32, about
// 7.8 units, from the centre.
#define PROJECTION_SCALE 32.0f

// Face colors, in the order the assets list their faces. The block's broad
// faces, then its edges; the cube's outside faces, then its inside ones.
static constexpr int BlockColors[] = { 8, 8, 4, 12, 4, 12 };
static constexpr int CubeColors[] = { 1, 2, 3, 2, 3, 1, 5, 6, 7, 6, 7, 5 };

// 0 is the background and 1 to 7 are blues, for the stars and the cube. 8 to
// 15, bit 3 set, are purples, for the slab over whatever lies behind it.
static constexpr RETRO_Palette Palette[] = {
	{ 0, 0, 0 }, { 0, 28, 36 }, { 0, 56, 72 }, { 0, 84, 108 },
	{ 0, 102, 146 }, { 0, 129, 183 }, { 0, 154, 222 }, { 0, 183, 255 },
	{ 49, 5, 47 }, { 47, 24, 63 }, { 46, 40, 81 }, { 48, 60, 100 },
	{ 46, 79, 117 }, { 48, 97, 138 }, { 47, 115, 153 }, { 49, 137, 174 }
};

static Model3D *BlockModel;
static Model3D *CubeModel;
static vec3 Stars[STAR_COUNT];

// Fill one face of a model if it faces the viewer.
static void DrawFace(Model3D *model, const Face *face)
{
	if (!face->frontfacing) return;

	PolygonPoint pts[4];
	for (int k = 0; k < 4; k++) {
		pts[k].pos = model->vertex[face->vertex[k]].spos;
	}
	RETRO_DrawMaskedPolygon(pts, 4, face->c, model->mask);
}

// Draw the part of the cube with n · p < −h, where n is a unit normal, the
// visible faces only.
static void DrawSection(Model3D *cubemodel, vec3 normal)
{
	for (int f = 0; f < cubemodel->faces; f++) {
		Face *face = &cubemodel->face[f];
		if (!face->frontfacing) continue;

		vec3 clipped[8];
		int count = 0;
		for (int i = 0; i < 4; i++) {
			vec3 a = cubemodel->vertex[face->vertex[i]].rpos;
			vec3 b = cubemodel->vertex[face->vertex[(i + 1) % 4]].rpos;
			float da = dot(normal, a) + BLOCK_HALF_THICKNESS;
			float db = dot(normal, b) + BLOCK_HALF_THICKNESS;
			if (da < 0) clipped[count++] = a;
			if ((da < 0) != (db < 0)) {
				float t = da / (da - db);
				clipped[count++] = a + (b - a) * t;
			}
		}

		if (count >= 3) {
			PolygonPoint pts[8];
			for (int i = 0; i < count; i++) {
				Vertex tempvertex;
				tempvertex.rpos = clipped[i];
				RETRO_ProjectVertex(&tempvertex, PROJECTION_SCALE);
				pts[i].pos = tempvertex.spos;
			}
			RETRO_DrawMaskedPolygon(pts, count, face->c, cubemodel->mask);
		}
	}
}

// The field drifts along x, wrapping from one side of its cube to the other,
// and turns as a whole. Stars dim from 7 at the near side of the cube to 1 at
// the far side, and every other one has four arms at half its brightness.
static void DrawStars(double time)
{
	float angle = fmod(time * STAR_ROTATION_SPEED, 2 * M_PI);
	mat3 rotation = rotateZ(-angle) * rotateY(angle);
	float travel = fmod(time * STAR_SPEED, 2 * STAR_FIELD);

	for (int i = 0; i < STAR_COUNT; i++) {
		vec3 star = Stars[i];
		star.x -= travel;
		if (star.x < -STAR_FIELD) star.x += 2 * STAR_FIELD;

		Vertex vertex;
		vertex.rpos = rotation * star;
		RETRO_ProjectVertex(&vertex, PROJECTION_SCALE);
		if (vertex.q <= 0.0f) continue;
		int sx = (int)vertex.spos.x;
		int sy = (int)vertex.spos.y;
		if (sx < 1 || sx >= RETRO_WIDTH - 1 || sy < 1 || sy >= RETRO_HEIGHT - 1) continue;

		int shade = CLAMP((int)(4 - 3 * vertex.rpos.z / STAR_FIELD), 1, 8);
		RETRO_PutPixel(sx, sy, shade);
		int arm = shade >> 1;
		if ((i & 1) && arm > 0) {
			RETRO_PutPixel(sx - 1, sy, arm);
			RETRO_PutPixel(sx + 1, sy, arm);
			RETRO_PutPixel(sx, sy - 1, arm);
			RETRO_PutPixel(sx, sy + 1, arm);
		}
	}
}

void DEMO_Render(double time, double deltatime)
{
	DrawStars(time);

	// Composed Rz * Rx * Ry rather than the Rz * Ry * Rx of rotate(), for this
	// tumble; rotate()'s order would take both objects along another path.
	// The block starts a quarter turn about X, edge-on.
	float bax = fmod(time * BLOCK_SPEED_X - M_PI / 2, 2 * M_PI);
	float bay = fmod(time * BLOCK_SPEED_Y, 2 * M_PI);
	float baz = fmod(time * BLOCK_SPEED_Z, 2 * M_PI);
	mat3 blockrotation = rotateZ(baz) * rotateX(bax) * rotateY(bay);

	float cax = fmod(time * CUBE_SPEED_X, 2 * M_PI);
	float cay = fmod(time * CUBE_SPEED_Y, 2 * M_PI);
	float caz = fmod(time * CUBE_SPEED_Z, 2 * M_PI);
	mat3 cuberotation = rotateZ(caz) * rotateX(cax) * rotateY(cay);

	// RETRO_SortFaces sets frontfacing on every face. The draw order below is
	// the effect's own, so its sorted list goes unused.
	RETRO_RotateModel(blockrotation, BlockModel);
	RETRO_ProjectModel(PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, BlockModel);
	RETRO_SortFaces(false, BlockModel);

	RETRO_RotateModel(cuberotation, CubeModel);
	RETRO_ProjectModel(PROJECTION_SCALE, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, CubeModel);
	RETRO_SortFaces(false, CubeModel);

	// The broad faces' normals point out of the slab, in opposite directions.
	// Face 0 is the near one when its normal points toward the viewer (z < 0),
	// so face 1 goes first. The section is clipped along the reversed normal.
	int first = BlockModel->face[0].facenormal.rdir.z <= 0.0f ? 1 : 0;
	for (int pass = 0; pass < 2; pass++) {
		Face *broadface = &BlockModel->face[first ^ pass];
		DrawFace(BlockModel, broadface);
		DrawSection(CubeModel, -broadface->facenormal.rdir);
	}

	// Faces 2 onward are the edges
	for (int i = 2; i < BlockModel->faces; i++) {
		DrawFace(BlockModel, &BlockModel->face[i]);
	}
}

void DEMO_Initialize(void)
{
	RETRO_SetPalette(Palette, 16);

	for (vec3 &star : Stars) {
		star.x = RANDOMF(2 * STAR_FIELD) - STAR_FIELD;
		star.y = RANDOMF(2 * STAR_FIELD) - STAR_FIELD;
		star.z = RANDOMF(2 * STAR_FIELD) - STAR_FIELD;
	}

	BlockModel = RETRO_Load3DModel("assets/magicblock.obj");
	BlockModel->mask = BLOCK_MASK;
	for (int i = 0; i < BlockModel->faces; i++) {
		BlockModel->face[i].c = BlockColors[i];
	}

	CubeModel = RETRO_Load3DModel("assets/magiccube.obj");
	CubeModel->mask = CUBE_MASK;
	for (int i = 0; i < CubeModel->faces; i++) {
		CubeModel->face[i].c = CubeColors[i];
	}
}
