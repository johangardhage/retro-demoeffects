//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMODEL_H_
#define _RETROMODEL_H_

#include "retro.h"
#include "retromatrix.h"
#include "retrovector.h"
#include "retropoly.h"

// The grazing height G is the height difference across two texels that tilts
// a normal 45°. That difference, divided by the surface one texel covers and
// by G, is the tilt, so a larger G reads shallower. A metal env map
// turns a small tilt into a different color, so it wants a shallower G; a
// shade table has only the material ramp to spend, so it takes the deeper
// default. Below about 24 a finely detailed bump map breaks a highlight up
// rather than roughening it. G is a height in the bump map's own units and has
// nothing to do with bumpmapheight, which is that map's number of rows.
#define RETRO_BUMP_GRAZING 32
#define RETRO_ENVMAP_SIZE 256

// The loaders scale a model's UVs by this, so they come out as texels of a map this
// wide. It is also the default texture, bump and stencil map size, but the renderers
// stride each map by the model's own width, so a model given a map of another size
// sets that width and rescales its UVs to match.
#define RETRO_TEXMAP_SIZE 256

#define RETRO_MAX_VERTICES 1000
#define RETRO_MAX_UVS 1000
#define RETRO_MAX_NORMALS 1500 // a two sided mesh needs one per side of a vertex
#define RETRO_MAX_FACES 2000
#define RETRO_MAX_FACEVERTICES 5
#define RETRO_MAX_MATERIALS 16
#define RETRO_MATERIAL_NAME 32 // the longest material name, with its terminator
#define RETRO_MAX_MODELS 32 // a demo can hold several models at once, as it can images; a
									// per-character glyph cache is the heaviest current user

struct Vertex {
	vec3 pos;					// Model space coordinates
	vec3 rpos;					// Rotated coordinates
	vec2 spos;					// Screen coordinates
	float q;					// Reciprocal projection depth
};

// A unit vector: where something points. Every UnitVector is normalized as it
// is written (dir is filled with a normalized vector), so nothing that
// reads one has to divide it out first, and the model's rotation is orthonormal,
// so it stays unit all the way to the drawers.
//
// Unlike a Vertex it has no screen form and does not translate, which is the
// whole distinction - RETRO_TranslateModel moves vertices and leaves these
// alone, so only the rotation reaches them. Normals and tangents share the
// type because the library only ever rotates: a normal is strictly a covector
// and transforms by the inverse transpose, which for an orthonormal matrix is
// the matrix itself.
struct UnitVector {
	vec3 dir;					// Model space direction, unit length
	vec3 rdir;					// Rotated direction, still unit
};

struct Face {
	int vertices;								// Number of vertices in face
	int vertex[RETRO_MAX_FACEVERTICES];			// Index of vertices in face
	int uv[RETRO_MAX_FACEVERTICES];				// Index of UV coordinates in face
	int vertexnormal[RETRO_MAX_FACEVERTICES];	// Index of vertex normals in face
	int c;										// Front-facing offset from the model's c, in whatever
												// space that renderer measures in
	int backc;									// Back-facing offset; zero makes that side transparent
	UnitVector facenormal;						// Face normal
	UnitVector tangent;							// Surface direction of +u, for bump mapping
	UnitVector bitangent;						// and of +v
	float area;									// Its area, the weight it lends a vertex normal
	bool frontfacing;							// Which side the winding shows. Only set for a face that
												// reached the winding test, so read it only for a face in
												// the draw list
	float depth;								// Mean rotated depth, the painter's sort key
	int material;								// Which of the model's material names the face was listed
												// under, and 0 for a face listed before any
};

// Default cap on Glenz's additive framebuffer write: the unsigned char
// ceiling. RETRO_DrawGlenzPolygon clamps to this unless a model sets a
// lower GlenzLighting::colormax, so a triple overlap fills a chosen shade
// instead of walking into white.
#define RETRO_GLENZ_COLORMAX_DEFAULT (RETRO_COLORS - 1)

// Configuration for the Glenz renderer. The flat-shading fields set how a
// face is lit: the default signed falloff lets rear faces shade below their
// material offset, while Model3D::twosided instead lights inward normals
// and stays inside each material ramp. colormax applies to every Glenz face
// regardless of shading, since it caps the additive framebuffer write itself.
struct GlenzLighting {
	float diffuse = 1.0f;
	float highlight = 0.0f;
	float exponent = 4.0f;
	float backstrength = 0.5f;
	int colormax = RETRO_GLENZ_COLORMAX_DEFAULT;
};

struct Model3D {
	int faces;									// Number of faces
	int vertices;								// Number of vertices
	int uvs;									// Number of UV coordinates
	int normals;								// Number of vertex normals
	Face face[RETRO_MAX_FACES];					// Face list
	Vertex vertex[RETRO_MAX_VERTICES];			// Vertex list
	vec2 uv[RETRO_MAX_UVS];						// UV list, in texels: x across texmapwidth, y down texmapheight
	UnitVector normal[RETRO_MAX_NORMALS];		// Vertex normal list
	int drawfaces;								// Number of faces in the draw list
	int drawface[RETRO_MAX_FACES];				// Faces to draw, sorted far to near
	mat3 matrix;								// Rotation matrix
	const RETRO_Lighting *lighting;				// The lights it is shaded by, RETRO_Headlight
												// unless it is given others
	RETRO_ShadeTable colortable;				// With a table, flat, Gouraud and Phong light it in the
												// lights' colors and look the result up here, red,
												// green and blue as its three tints, textured or not:
												// a texture's table has a row for each of its colors;
												// see RETRO_CreateColorLightTable. Without one, only
												// their brightness counts
	int c;										// The base the shading is measured from, which the
												// renderer decides the space of: the first entry of the
												// model's own ramp for the palette renderers, and a level
												// in the 128 entry shade table for the texture ones.
												// face.c offsets from it either way
	int shades;									// Entries in the face's shade ramp, so that
												// c + face.c + shades is one past its last
												// entry, matching the half-open range the
												// palette constructors are given. A texture
												// renderer steps it through the shade table
												// rather than the palette, and falls back to
												// RETRO_SHADE_TABLE_SHADES, the table's full
												// height, for a model that leaves this zero
	bool twosided;								// Draw every face from either side, shading the one
												// turned away by the reverse of its normal. A surface
												// with no inside - a sheet, an open shell - is otherwise
												// lost the moment it turns: its normals point away from
												// the viewer, so the whole of it lands on the dark end of
												// the ramp. Honored by the shaded renderers for whether a
												// back face is drawn at all; Glenz draws both sides
												// regardless, but still reads this for how to light the
												// back one, and the wireframe path ignores it entirely
	unsigned char mask = 0xff;					// The palette index bits a masked write replaces,
												// leaving the rest of each pixel as it was
	float *frame = NULL;						// Morph targets: frames blocks of vertices model space
												// x, y, z, the same vertex list posed differently. Only
												// the positions are held, since the topology, the UVs
												// and the shading are the model's own and do not move
												// with the pose. Allocated only when an animation is
												// loaded, and read through RETRO_MorphModel
	int frames;									// Morph targets held, or zero for a still model
	unsigned char *texmap = NULL;				// Texture
	int texmapwidth = RETRO_TEXMAP_SIZE;		// Texture width, which is also the space the UVs are in
	int texmapheight = RETRO_TEXMAP_SIZE;		// Texture height
	unsigned char *shadetable = NULL;			// Texture lighting table
	unsigned char *envmap = NULL;				// Environment texture
	int envmapwidth = RETRO_ENVMAP_SIZE;		// Environment texture width
	int envmapheight = RETRO_ENVMAP_SIZE;		// Environment texture height
	int envmapradius = RETRO_ENVMAP_SIZE / 2;	// Texels from the map's middle a grazing normal reaches,
												// so half the width samples the lighting map's whole disk
	bool envmapperspective;						// Reflect each vertex's own view ray, from the eye, rather
												// than the view axis. Off, the reflection renderer looks a
												// pixel up by its normal alone, as if seen head-on: close on
												// a curved surface, whose normals already sweep the map,
												// and cheaper. A flat face needs it on, since its normal
												// never varies and the whole picture is in the view ray
	unsigned char *bumpmap = NULL;				// Bump texture
	int bumpmapwidth = RETRO_TEXMAP_SIZE;		// Bump texture width, which need not match the texture's
	int bumpmapheight = RETRO_TEXMAP_SIZE;		// Bump texture height
	int bumpgrazing = RETRO_BUMP_GRAZING;		// Height difference across two bump texels that tilts a normal 45°
	unsigned char *stencilmap = NULL;			// Stencil picture, read in screen space behind each face
	int stencilmapwidth = RETRO_TEXMAP_SIZE;	// Stencil picture width
	int stencilmapheight = RETRO_TEXMAP_SIZE;	// Stencil picture height
	vec2 stencilmaporigin;						// Where the picture's first texel sits, in pixels right
												// of and below each face's own top-left
	unsigned char (*shader)(const Fragment &fragment) = NULL;	// RETRO_POLY_SHADER's per-pixel function: the color of one
												// Fragment of the surface
	float eye;									// Model units from the rotated origin back to the eye,
												// along -z, as RETRO_ProjectModel last placed it. What
												// envmapperspective aims each view ray from; zero, a
												// model never projected that way, keeps every ray parallel
												// to the view axis. RETRO_POLY_SHADER has no such
												// fallback and needs it set
	GlenzLighting glenzlighting;				// Configuration for the Glenz renderer
	char material[RETRO_MAX_MATERIALS][RETRO_MATERIAL_NAME];	// The names a file's usemtl lines give its parts,
												// in the order they first appear. What each one looks
												// like is the demo's to decide, so no library is read
	int materials;								// Material names held, or zero for a file without any
};

inline struct {
	Model3D *model[RETRO_MAX_MODELS];
	int models = 0;
} RETRO_Model;

//
// The model a library call uses when it is handed none
//
// Model 0 is that model, so a demo holding one never has to name it and every
// call that takes a Model3D * can go on leaving it out. A demo holding several
// keeps the pointers its loads returned and passes the one it means, or asks
// for it here by id.
//
inline Model3D *RETRO_Get3DModel(int id = 0)
{
	return id >= 0 && id < RETRO_MAX_MODELS ? RETRO_Model.model[id] : NULL;
}

//
// Allocate a model and register it
//
// A model built in code rather than read from a file is allocated here too, so
// it is reached and released like any other. The allocation is calloc rather
// than malloc: the counts, pointers and arrays have to start at zero, and
// Model3D's member initializers make the type non-trivial, so memset cannot
// be given a Model3D *. calloc does not run those initializers, so the
// defaults that are not zero are assigned by hand.
//
inline Model3D *RETRO_Allocate3DModel(void)
{
	// First free slot. Ids are slot numbers and are recycled: free(0) then
	// load reuses 0 even if a later model still holds a higher id.
	int id = 0;
	while (id < RETRO_MAX_MODELS && RETRO_Model.model[id]) {
		id++;
	}
	if (id == RETRO_MAX_MODELS) {
		RETRO_RageQuit("Too many 3D models to fit the model list\n");
	}

	Model3D *model = (Model3D *)calloc(1, sizeof(Model3D));
	if (model == NULL) {
		RETRO_RageQuit("Cannot allocate 3D model memory\n");
	}
	model->glenzlighting = GlenzLighting{};
	model->lighting = &RETRO_Headlight;
	model->texmapwidth = RETRO_TEXMAP_SIZE;
	model->texmapheight = RETRO_TEXMAP_SIZE;
	model->envmapwidth = RETRO_ENVMAP_SIZE;
	model->envmapheight = RETRO_ENVMAP_SIZE;
	model->bumpmapwidth = RETRO_TEXMAP_SIZE;
	model->bumpmapheight = RETRO_TEXMAP_SIZE;
	model->stencilmapwidth = RETRO_TEXMAP_SIZE;
	model->stencilmapheight = RETRO_TEXMAP_SIZE;
	model->envmapradius = RETRO_ENVMAP_SIZE / 2;
	model->bumpgrazing = RETRO_BUMP_GRAZING;
	model->mask = 0xff;

	RETRO_Model.model[id] = model;
	RETRO_Model.models++;

	return model;
}

inline void RETRO_Free3DModel(int id = 0)
{
	if (id >= 0 && id < RETRO_MAX_MODELS && RETRO_Model.model[id]) {
		free(RETRO_Model.model[id]->frame);
		free(RETRO_Model.model[id]);
		RETRO_Model.model[id] = NULL;
		RETRO_Model.models--;
	}
}

//
// Append one vertex to a model built procedurally rather than loaded
//
inline int RETRO_AddModelVertex(Model3D *model, float x, float y, float z)
{
	if (model->vertices >= RETRO_MAX_VERTICES) {
		RETRO_RageQuit("Too many model vertices\n");
	}

	int i = model->vertices++;
	model->vertex[i].pos = { x, y, z };
	return i;
}

//
// Append one triangle face, referencing vertices already added
//
// facec is the face's own offset from the model's c (see Model3D::c); it
// defaults to 0, which is every renderer's neutral value. backc is the same
// for the side facing away, and its 0 leaves that side transparent.
//
inline void RETRO_AddModelTriangle(Model3D *model, int a, int b, int c, int facec = 0, int backc = 0)
{
	if (model->faces >= RETRO_MAX_FACES) {
		RETRO_RageQuit("Too many model faces\n");
	}

	Face *face = &model->face[model->faces++];
	memset(face, 0, sizeof(Face));
	face->vertices = 3;
	face->vertex[0] = a;
	face->vertex[1] = b;
	face->vertex[2] = c;
	face->c = facec;
	face->backc = backc;
}

//
// Append one quad face, referencing vertices already added. facec and backc
// are RETRO_AddModelTriangle's.
//
inline void RETRO_AddModelQuad(Model3D *model, int a, int b, int c, int d, int facec = 0, int backc = 0)
{
	RETRO_AddModelTriangle(model, a, b, c, facec, backc);

	Face *face = &model->face[model->faces - 1];
	face->vertices = 4;
	face->vertex[3] = d;
}

// Area-weighted average of the adjacent face normals (Hearn & Baker / Foley).
// Each face lends its unit normal scaled by Face::area, so a large face pulls
// harder on the vertices it meets than a small one does. Needs face normals
// first. For a cube at the origin this comes out along the vertex position; for
// a general mesh it does not.
inline void RETRO_InitializeVertexNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->normal[i].dir = { 0.0f, 0.0f, 0.0f };
	}

	for (int i = 0; i < model->faces; i++) {
		Face *face = &model->face[i];
		vec3 n = face->facenormal.dir * face->area;

		for (int j = 0; j < face->vertices; j++) {
			int v = face->vertex[j];
			model->normal[v].dir += n;
		}
	}

	for (int i = 0; i < model->vertices; i++) {
		model->normal[i].dir = normalize(model->normal[i].dir);
	}

	model->normals = model->vertices;

	for (int i = 0; i < model->faces; i++) {
		for (int j = 0; j < model->face[i].vertices; j++) {
			model->face[i].vertexnormal[j] = model->face[i].vertex[j];
		}
	}
}

// N = (v0 - v1) × (v0 - v2), the geometric normal of the first triangle of the
// face, stored as a unit direction. It stands for the whole face only while the
// face is planar, which every asset's is at rest; deforming a mesh tilts the two
// halves of a quad apart and this follows the half the rasterizer draws first.
//
// Face::area is the whole face either way, since it is a weight rather than a
// direction: the first triangle is half a quad only when the quad is a
// parallelogram, and the quads of a lat-long mesh are trapezoids.
inline void RETRO_InitializeFaceNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		Face *face = &model->face[i];
		Vertex *v0 = &model->vertex[face->vertex[0]];
		Vertex *v1 = &model->vertex[face->vertex[1]];
		Vertex *v2 = &model->vertex[face->vertex[2]];
		vec3 p0 = v0->pos;
		vec3 e1 = p0 - v1->pos;
		vec3 e2 = p0 - v2->pos;

		vec3 n = cross(e1, e2);
		float len = length(n);
		float invlen = len > 0.0f ? 1.0f / len : 0.0f;
		face->facenormal.dir = n * invlen;

		// Fan the rest of the face from vertex 0, a triangle at a time, so the area
		// is the whole face's and not just the first triangle's
		float area = len / 2;

		for (int j = 3; j < face->vertices; j++) {
			Vertex *v3 = &model->vertex[face->vertex[j - 1]];
			Vertex *v4 = &model->vertex[face->vertex[j]];
			vec3 e3 = p0 - v3->pos;
			vec3 e4 = p0 - v4->pos;
			area += length(cross(e3, e4)) / 2;
		}

		face->area = area;
	}
}

//
// Tangent frame of each face, from its UV parametrization
//
// A bump map is a height field over (u, v), so its gradient tilts along dP/du
// and dP/dv. Both come from the two edges of the face and the UVs at its
// corners (Lengyel):
//
//   e1 = P1 - P0,  e2 = P2 - P0
//   T  = ( e1 dv2 - e2 dv1) / (du1 dv2 - du2 dv1)
//   B  = ( e2 du1 - e1 du2) / (du1 dv2 - du2 dv1)
//
// then Gram-Schmidt against the face normal, so a skewed UV layout does not
// shear the tilt. A face with no usable UVs falls back to any frame
// orthogonal to its normal.
//
inline void RETRO_InitializeFaceTangents(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		Face *face = &model->face[i];
		vec3 n = face->facenormal.dir;

		vec3 t = { 0.0f, 0.0f, 0.0f };
		vec3 b = { 0.0f, 0.0f, 0.0f };
		bool derived = false;

		if (model->uvs > 0 && face->vertices >= 3) {
			Vertex *p0 = &model->vertex[face->vertex[0]];
			Vertex *p1 = &model->vertex[face->vertex[1]];
			Vertex *p2 = &model->vertex[face->vertex[2]];
			vec2 t0 = model->uv[face->uv[0]];
			vec2 t1 = model->uv[face->uv[1]];
			vec2 t2 = model->uv[face->uv[2]];

			vec3 e1 = p1->pos - p0->pos;
			vec3 e2 = p2->pos - p0->pos;
			vec2 duv1 = t1 - t0;
			vec2 duv2 = t2 - t0;

			float determinant = duv1.x * duv2.y - duv2.x * duv1.y;
			if (fabs(determinant) > 1.0e-12f) {
				float r = 1.0f / determinant;
				t = (e1 * duv2.y - e2 * duv1.y) * r;
				b = (e2 * duv1.x - e1 * duv2.x) * r;
				derived = true;
			}
		}

		if (!derived) {
			// Any direction not parallel to the normal
			t = fabs(n.x) < 0.9f ? vec3{ 1.0f, 0.0f, 0.0f } : vec3{ 0.0f, 1.0f, 0.0f };
			b = cross(n, t);
		}

		// Gram-Schmidt: drop the part of T along N, then of B along both
		t = t - n * dot(n, t);
		float tlength = length(t);
		if (tlength > 1.0e-12f) {
			t = t * (1.0f / tlength);
		}

		b = b - (n * dot(n, b) + t * dot(t, b));
		float blength = length(b);
		if (blength > 1.0e-12f) {
			b = b * (1.0f / blength);
		} else {
			b = cross(n, t);
		}

		face->tangent.dir = t;
		face->bitangent.dir = b;
	}
}

//
// UVs that lay the whole texture over every face, in the face's own frame
//
// A model can arrive with its faces sharing one atlas, or with no usable UVs
// at all. Reparametrizing it here hands each face the texture entire, in a
// frame taken from the face normal rather than from the order the face's
// corners happen to be listed in:
//
//   t = n × up,  b = t × n
//
// where up is +y, which is screen down, so v runs down the texture the way
// its rows do and every face carries the picture the same way up. A face
// looking along up itself has no such t, no u on it being upright, and falls
// back to +x. Since b follows from t, no face comes out mirrored.
//
// The frame is measured against the model's bounding box rather than against
// the face, so a face on the side of the box gets the whole texture and a
// quad keeps one parametrization after being split into triangles. A face
// carries its own UVs afterward, one set per corner, and the tangent frames,
// which are derived from the UVs, are rebuilt to match.
//
inline void RETRO_InitializeFaceUVs(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	if (model->vertices == 0) {
		return;
	}

	int uvs = 0;
	for (int i = 0; i < model->faces; i++) {
		uvs += model->face[i].vertices;
	}
	if (uvs > RETRO_MAX_UVS) {
		RETRO_RageQuit("Too many face UV coordinates to fit the UV list\n");
	}

	// Bounding box center and half extent
	vec3 boxmin = model->vertex[0].pos;
	vec3 boxmax = boxmin;

	for (int i = 1; i < model->vertices; i++) {
		boxmin = min(boxmin, model->vertex[i].pos);
		boxmax = max(boxmax, model->vertex[i].pos);
	}

	vec3 center = (boxmin + boxmax) * 0.5f;
	vec3 half = (boxmax - boxmin) * 0.5f;

	model->uvs = 0;

	for (int i = 0; i < model->faces; i++) {
		Face *face = &model->face[i];
		vec3 n = face->facenormal.dir;

		// t = n × (0, 1, 0)
		vec3 t = cross(n, vec3{ 0.0f, 1.0f, 0.0f });
		float tlength = length(t);
		t = tlength > 1.0e-12f ? t * (1.0f / tlength) : vec3{ 1.0f, 0.0f, 0.0f };

		// b = t × n
		vec3 b = cross(t, n);

		// The box reaches this far along each of them
		float textent = dot(abs(t), half);
		float bextent = dot(abs(b), half);
		float inversetextent = textent > 0.0f ? 1.0f / textent : 0.0f;
		float inversebextent = bextent > 0.0f ? 1.0f / bextent : 0.0f;

		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];

			vec3 p = vertex->pos - center;
			float u = (dot(p, t) * inversetextent + 1) / 2;
			float v = (dot(p, b) * inversebextent + 1) / 2;

			model->uv[model->uvs] = { u * model->texmapwidth, v * model->texmapheight };
			face->uv[j] = model->uvs;
			model->uvs++;
		}
	}

	RETRO_InitializeFaceTangents(model);
}

//
// Room for frames poses of the model's vertices, replacing any it held
//
inline void RETRO_AllocateModelFrames(Model3D *model, int frames)
{
	free(model->frame);
	model->frame = (float *)malloc((size_t)frames * model->vertices * 3 * sizeof(float));
	if (model->frame == NULL) {
		RETRO_RageQuit("Cannot allocate animation memory\n");
	}
	model->frames = frames;
}

//
// Poses of a model that is already loaded, one file per frame, named by a
// printf pattern taking the frame number: "assets/thing_%02d.obj" for
// thing_00.obj upward. Only the v lines are read, since a pose differs from the
// model it poses in nothing but where the vertices are - the faces, the UVs and
// the normals are the model's own and do not move with it - and a file naming a
// different number of vertices is not a pose of this model at all
//
// Reloading an animation over one already held replaces it, so a model carries
// at most the one it was last given
//
inline void RETRO_Load3DModelFrames(Model3D *model, const char *pattern, int frames)
{
	if (frames <= 0) {
		RETRO_RageQuit("An animation needs at least one frame: %s\n", pattern);
	}

	RETRO_AllocateModelFrames(model, frames);

	for (int frame = 0; frame < frames; frame++) {
		char filename[128];
		snprintf(filename, sizeof(filename), pattern, frame);

		FILE *fp = fopen(filename, "rb");
		if (fp == NULL) {
			RETRO_RageQuit("Cannot open file: %s\n", filename);
		}

		float *pose = &model->frame[(size_t)frame * model->vertices * 3];
		int vertices = 0;

		char row[128];
		while (fscanf(fp, "%127s", row) != EOF) {
			if (strcmp(row, "v") == 0) {
				// Check before writing, as the model loader does: one vertex too
				// many walks off the end of this pose and into the next
				if (vertices >= model->vertices) {
					RETRO_RageQuit("Pose names more vertices than the model it poses: %s\n", filename);
				}
				if (fscanf(fp, "%f %f %f\n", &pose[vertices * 3], &pose[vertices * 3 + 1], &pose[vertices * 3 + 2]) != 3) {
					RETRO_RageQuit("Cannot read vertex, expected three floats: %s\n", filename);
				}
				vertices++;
			} else { // Topology the model already carries, eat up the rest of the line
				fgets(row, 128, fp);
			}
		}
		fclose(fp);

		if (vertices != model->vertices) {
			RETRO_RageQuit("Pose names %d vertices, the model it poses has %d: %s\n", vertices, model->vertices, filename);
		}
	}
}

//
// Pose the model between any two of its poses, a fraction s of the way from
// pose a to pose b,
//
//   p = (1 - s) a + s b
//
// RETRO_MorphModel runs the poses in order through this. Called directly it
// serves an animation that does not: a loop closing from its last pose back to
// its first, or one of several sequences held in the same list of poses.
// Normals are left as they were, as RETRO_MorphModel leaves them
//
inline void RETRO_BlendModelPoses(int a, int b, float s, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	if (a < 0 || a >= model->frames || b < 0 || b >= model->frames) {
		RETRO_RageQuit("RETRO_BlendModelPoses needs poses the model holds, 0 to %d\n", model->frames - 1);
	}

	const float *from = &model->frame[(size_t)a * model->vertices * 3];
	const float *to = &model->frame[(size_t)b * model->vertices * 3];

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].pos = {
			mix(from[i * 3], to[i * 3], s),
			mix(from[i * 3 + 1], to[i * 3 + 1], s),
			mix(from[i * 3 + 2], to[i * 3 + 2], s)
		};
	}
}

//
// Pose the model at u along its animation, u in [0, 1] over the whole of it.
// u * (frames - 1) names the pair of poses it falls between and the fraction s
// to mix them by,
//
//   p(u) = (1 - s) a + s b
//
// so u = 0 lands on the first pose exactly and u = 1 on the last. A demo owns
// the clock: it decides what u does with time, whether that is once through,
// a loop, or a ping-pong.
//
// Only the vertices move. The poses carry no normals, so a model that is
// shaded needs RETRO_InitializeFaceNormals and RETRO_InitializeVertexNormals
// run over the result before it is drawn, and one that is bump mapped
// RETRO_InitializeFaceTangents as well. RETRO_InitializeVertexNormals derives
// the normals afresh, replacing any the model's file supplied
//
inline void RETRO_MorphModel(float u, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	if (model->frames == 0) {
		RETRO_RageQuit("RETRO_MorphModel needs a model with poses: RETRO_Load3DModel with an animation, RETRO_Load3DModelFrames, RETRO_LoadMD2Model or RETRO_LoadMD3Model\n");
	}

	float f = CLAMP01(u) * (model->frames - 1);
	int a = f;
	// u = 1 lands on the last pose with nothing past it to mix toward, and s is
	// zero there, so b carries no weight and only has to stay in range
	int b = MIN(a + 1, model->frames - 1);
	RETRO_BlendModelPoses(a, b, f - a, model);
}

// The index of a material the model's file named, or -1 if it named none so
inline int RETRO_ModelMaterial(const Model3D *model, const char *name)
{
	for (int i = 0; i < model->materials; i++) {
		if (strcmp(model->material[i], name) == 0) return i;
	}
	return -1;
}

//
// Load a model, scaling its 0 to 1 UVs into texels, and with it the animation
// that poses it, if it is given one
//
// Nothing downstream converts them: a drawer indexes the texture with what UV holds, so
// they have to be texels by the time it gets there, and this is where that happens. The
// map they are scaled into is RETRO_TEXMAP_SIZE square, which is what texmapwidth and
// texmapheight start out as; a model given a texture of another size rescales them.
//
// The animation is a printf pattern and a frame count, handed on to
// RETRO_Load3DModelFrames, and the file named here is the model those poses are
// read against: it is what fixes the topology and how many vertices a pose has
// to name. Without a pattern, the default, the model is loaded still, with no
// poses
//
inline Model3D *RETRO_Load3DModel(const char *filename, const char *animation = NULL, int frames = 0)
{
	Model3D *model = RETRO_Allocate3DModel();

	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	int vertices = 0, uvs = 0, normals = 0, faces = 0;
	int material = 0;

	// Check before writing: overflow walks into the next list in this struct.
	char row[128];
	while (fscanf(fp, "%127s", row) != EOF) {
		if (strcmp(row, "v") == 0) { // Load vertices
			if (vertices >= RETRO_MAX_VERTICES) {
				RETRO_RageQuit("Too many vertices to fit the vertex list: %s\n", filename);
			}
			if (fscanf(fp, "%f %f %f\n", &model->vertex[vertices].pos.x, &model->vertex[vertices].pos.y, &model->vertex[vertices].pos.z) != 3) {
				RETRO_RageQuit("Cannot read vertex, expected three floats: %s\n", filename);
			}
			vertices++;
		} else if (strcmp(row, "vt") == 0) { // Load UV coordinates
			if (uvs >= RETRO_MAX_UVS) {
				RETRO_RageQuit("Too many UV coordinates to fit the UV list: %s\n", filename);
			}
			if (fscanf(fp, "%f %f\n", &model->uv[uvs].x, &model->uv[uvs].y) != 2) {
				RETRO_RageQuit("Cannot read UV coordinate, expected two floats: %s\n", filename);
			}
			model->uv[uvs] = model->uv[uvs] * (float)RETRO_TEXMAP_SIZE;
			uvs++;
		} else if (strcmp(row, "vn") == 0) { // Load normals
			if (normals >= RETRO_MAX_NORMALS) {
				RETRO_RageQuit("Too many normals to fit the normal list: %s\n", filename);
			}
			if (fscanf(fp, "%f %f %f\n", &model->normal[normals].dir.x, &model->normal[normals].dir.y, &model->normal[normals].dir.z) != 3) {
				RETRO_RageQuit("Cannot read normal, expected three floats: %s\n", filename);
			}
			// A file's vn need not be unit, and everything downstream assumes it is
			model->normal[normals].dir = normalize(model->normal[normals].dir);
			normals++;
		} else if (strcmp(row, "f") == 0) {
			if (faces >= RETRO_MAX_FACES) {
				RETRO_RageQuit("Too many faces to fit the face list: %s\n", filename);
			}

			// The rest of the line, so the face is read to its last corner and no
			// further: a corner past what a face holds is an error, not the start
			// of the next line
			char line[256];
			if (fgets(line, sizeof(line), fp) == NULL) {
				line[0] = '\0';
			} else if (strchr(line, '\n') == NULL && !feof(fp)) {
				RETRO_RageQuit("Cannot read face, line too long: %s\n", filename);
			}

			Face *face = &model->face[faces];
			int corners = 0, offset = 0, consumed = 0;

			// int, not unsigned: %d writes an int, and an index of 0 in the file
			// would wrap on the -1 below before the range check ever saw it
			int vertex, uv, normal;
			while (sscanf(line + offset, " %d/%d/%d%n", &vertex, &uv, &normal, &consumed) == 3) {
				if (corners == RETRO_MAX_FACEVERTICES) {
					RETRO_RageQuit("Face has more than %d corners: %s\n", RETRO_MAX_FACEVERTICES, filename);
				}
				face->vertex[corners] = vertex - 1;
				face->uv[corners] = uv - 1;
				face->vertexnormal[corners] = normal - 1;
				corners++;
				offset += consumed;
			}

			// Whole triples only: anything left but whitespace is a corner that is
			// not one
			const char *rest = line + offset;
			if (corners < 3 || strspn(rest, " \t\r\n") != strlen(rest)) {
				RETRO_RageQuit("Cannot read face, expected 3 to %d v/uv/n triples: %s\n", RETRO_MAX_FACEVERTICES, filename);
			}

			face->vertices = corners;
			face->material = material;
			faces++;
		} else if (strcmp(row, "usemtl") == 0) { // The faces after this belong to a part
			if (fscanf(fp, "%127s", row) != 1 || strlen(row) >= RETRO_MATERIAL_NAME) {
				RETRO_RageQuit("Cannot read material name, expected one shorter than %d: %s\n", RETRO_MATERIAL_NAME, filename);
			}
			material = RETRO_ModelMaterial(model, row);
			if (material < 0) {
				if (model->materials >= RETRO_MAX_MATERIALS) {
					RETRO_RageQuit("Too many materials to fit the material list: %s\n", filename);
				}
				material = model->materials++;
				strcpy(model->material[material], row);
			}
		} else { // Probably a comment, eat up the rest of the line
			fgets(row, 128, fp);
		}
	}

	model->vertices = vertices;
	model->uvs = uvs;
	model->normals = normals;
	model->faces = faces;

	// Indices are 1-based into this file; skip UV/normal checks when those
	// lists are empty, because faces still write dummy 1/1/1 triples that
	// are never followed.
	for (int i = 0; i < faces; i++) {
		for (int j = 0; j < model->face[i].vertices; j++) {
			if (model->face[i].vertex[j] < 0 || model->face[i].vertex[j] >= vertices) {
				RETRO_RageQuit("Face names a vertex the file does not define: %s\n", filename);
			}
			if (uvs > 0 && (model->face[i].uv[j] < 0 || model->face[i].uv[j] >= uvs)) {
				RETRO_RageQuit("Face names a UV coordinate the file does not define: %s\n", filename);
			}
			if (normals > 0 && (model->face[i].vertexnormal[j] < 0 || model->face[i].vertexnormal[j] >= normals)) {
				RETRO_RageQuit("Face names a normal the file does not define: %s\n", filename);
			}
		}
	}

	fclose(fp);

	RETRO_InitializeFaceNormals(model);
	RETRO_InitializeFaceTangents(model);

	if (model->normals == 0) {
		RETRO_InitializeVertexNormals(model);
	}

	// The poses are read against the vertex list this file just defined, so they
	// can only be loaded once it stands
	if (animation) {
		RETRO_Load3DModelFrames(model, animation, frames);
	}

	return model;
}

//
// A whole binary model file in memory, for the caller to free. One shorter
// than minsize, too short to hold its header, is not read
//
inline unsigned char *RETRO_ReadModelFile(const char *filename, long minsize, long *size)
{
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}
	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned char *data = (unsigned char *)malloc(*size > 0 ? *size : 1);
	if (data == NULL) {
		RETRO_RageQuit("Cannot allocate model file memory\n");
	}
	if (*size < minsize || fread(data, *size, 1, fp) != 1) {
		RETRO_RageQuit("Cannot read file: %s\n", filename);
	}
	fclose(fp);
	return data;
}

// A little endian int and float from a model file, as the Quake formats store them
inline int RETRO_ReadInt32(const unsigned char *data)
{
	return (int)((uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24);
}

inline float RETRO_ReadFloat32(const unsigned char *data)
{
	int bits = RETRO_ReadInt32(data);
	float value;
	memcpy(&value, &bits, sizeof(float));
	return value;
}

//
// Load a Quake II MD2 model, and every frame it holds as a pose of it
//
// The file keeps its frames as bytes, each frame scaled and offset back to
// model units by its own six floats. Quake is z up, so each point comes in
// turned a quarter about x, which keeps it right handed and facing +x:
//
//   (x, y, z) -> (x, z, -y)
//
// Quake draws a triangle wound clockwise as seen from outside, so each is read
// in reverse and faces out as an OBJ face does. The texture coordinates are
// pixels of the skin, whose size the header gives, and are scaled to texels of
// a map RETRO_TEXMAP_SIZE square; the skin itself is loaded apart, as any
// texture is. The first frame stands as the model's own vertex list. The normals
// the frames carry are Quake's lighting's, and are not read: the model's are
// its first frame's, and a pose leaves them as they were, as an OBJ animation's
// does
//
inline Model3D *RETRO_LoadMD2Model(const char *filename)
{
	long size;
	unsigned char *data = RETRO_ReadModelFile(filename, 68, &size);

	// The header: seventeen little endian ints
	int header[17];
	for (int i = 0; i < 17; i++) {
		header[i] = RETRO_ReadInt32(&data[i * 4]);
	}
	int skinwidth = header[2], skinheight = header[3], framesize = header[4];
	int vertices = header[6], uvs = header[7], faces = header[8], frames = header[10];
	int uvoffset = header[12], faceoffset = header[13], frameoffset = header[14];

	if (memcmp(data, "IDP2", 4) != 0 || header[1] != 8) {
		RETRO_RageQuit("Not an MD2 model: %s\n", filename);
	}
	if (vertices <= 0 || vertices > RETRO_MAX_VERTICES || uvs <= 0 || uvs > RETRO_MAX_UVS || faces <= 0 || faces > RETRO_MAX_FACES || frames <= 0 || skinwidth <= 0 || skinheight <= 0) {
		RETRO_RageQuit("MD2 model does not fit the model lists: %s\n", filename);
	}
	// Check each offset before subtracting it; division avoids overflowing
	// even when a malformed header names a very large frame count or size.
	if (uvoffset < 68 || faceoffset < 68 || frameoffset < 68 || framesize < 40 + vertices * 4 || uvoffset > size || uvs > (size - uvoffset) / 4 || faceoffset > size || faces > (size - faceoffset) / 12 || frameoffset > size || frames > (size - frameoffset) / framesize) {
		RETRO_RageQuit("MD2 model runs past the end of its file: %s\n", filename);
	}

	Model3D *model = RETRO_Allocate3DModel();
	model->vertices = vertices;
	model->uvs = uvs;
	model->faces = faces;

	for (int i = 0; i < uvs; i++) {
		const unsigned char *uv = &data[uvoffset + i * 4];
		model->uv[i].x = (short)(uv[0] | uv[1] << 8) * (float)RETRO_TEXMAP_SIZE / skinwidth;
		model->uv[i].y = (short)(uv[2] | uv[3] << 8) * (float)RETRO_TEXMAP_SIZE / skinheight;
	}

	for (int i = 0; i < faces; i++) {
		const unsigned char *triangle = &data[faceoffset + i * 12];
		Face *face = &model->face[i];
		face->vertices = 3;
		for (int j = 0; j < 3; j++) {
			int vertex = triangle[(2 - j) * 2] | triangle[(2 - j) * 2 + 1] << 8;
			int uv = triangle[6 + (2 - j) * 2] | triangle[6 + (2 - j) * 2 + 1] << 8;
			if (vertex >= vertices || uv >= uvs) {
				RETRO_RageQuit("Face names a vertex or UV coordinate the file does not define: %s\n", filename);
			}
			face->vertex[j] = vertex;
			face->uv[j] = uv;
		}
	}

	RETRO_AllocateModelFrames(model, frames);

	for (int frame = 0; frame < frames; frame++) {
		const unsigned char *source = &data[frameoffset + (size_t)frame * framesize];
		float transform[6]; // scale, then offset, little endian as the header is
		for (int i = 0; i < 6; i++) {
			transform[i] = RETRO_ReadFloat32(&source[i * 4]);
		}
		const unsigned char *point = source + 40; // past the transform and the frame's name
		float *pose = &model->frame[(size_t)frame * vertices * 3];
		for (int i = 0; i < vertices; i++) {
			float x = point[i * 4] * transform[0] + transform[3];
			float y = point[i * 4 + 1] * transform[1] + transform[4];
			float z = point[i * 4 + 2] * transform[2] + transform[5];
			pose[i * 3] = x;
			pose[i * 3 + 1] = z;
			pose[i * 3 + 2] = -y;
		}
	}
	free(data);

	RETRO_BlendModelPoses(0, 0, 0, model);
	RETRO_InitializeFaceNormals(model);
	RETRO_InitializeFaceTangents(model);
	RETRO_InitializeVertexNormals(model);

	return model;
}

//
// Load a Quake III MD3 model, and every frame it holds as a pose of it
//
// The file keeps each point as three shorts, and scale says how many of those
// steps make one model unit. Quake III's own is 64, a step being 1/64 of a
// Quake unit, which is the default; a model packed at another scale is loaded
// at the one it was packed at. The surfaces follow one another in one vertex
// list, all posed by the same frames. The turn from Quake's z up, the reversed
// triangles and the texture coordinates are handled as for MD2, except that
// the coordinates are already 0 to 1 and are only scaled to texels. The first
// frame stands as the model's own vertex list, and the normals the frames
// carry, two bytes each, are not read: the model's are derived from its first
// frame, and a pose leaves them as they were, as an OBJ animation's does
//
inline Model3D *RETRO_LoadMD3Model(const char *filename, float scale = 64.0f)
{
	long size;
	unsigned char *data = RETRO_ReadModelFile(filename, 108, &size);

	// The header: the magic, the version and the name, then nine ints
	int frames = RETRO_ReadInt32(&data[76]);
	int surfaces = RETRO_ReadInt32(&data[84]);
	int surfaceoffset = RETRO_ReadInt32(&data[100]);

	if (memcmp(data, "IDP3", 4) != 0 || RETRO_ReadInt32(&data[4]) != 15) {
		RETRO_RageQuit("Not an MD3 model: %s\n", filename);
	}
	if (frames <= 0 || surfaces <= 0) {
		RETRO_RageQuit("MD3 model has no frames or no surfaces: %s\n", filename);
	}

	// Walk the surfaces once to count them into the model lists, checking each
	// part against the end of the file before it is read
	int vertices = 0, faces = 0;
	long offset = surfaceoffset;

	for (int s = 0; s < surfaces; s++) {
		if (offset < 108 || offset > size - 108 || memcmp(&data[offset], "IDP3", 4) != 0) {
			RETRO_RageQuit("MD3 surface runs past the end of its file: %s\n", filename);
		}
		const unsigned char *surface = &data[offset];
		int surfaceframes = RETRO_ReadInt32(&surface[72]);
		int surfacevertices = RETRO_ReadInt32(&surface[80]);
		int surfacefaces = RETRO_ReadInt32(&surface[84]);
		int faceoffset = RETRO_ReadInt32(&surface[88]);
		int uvoffset = RETRO_ReadInt32(&surface[96]);
		int pointoffset = RETRO_ReadInt32(&surface[100]);
		int endoffset = RETRO_ReadInt32(&surface[104]);
		long remaining = size - offset;

		if (surfaceframes != frames) {
			RETRO_RageQuit("MD3 surface holds %d frames, the model %d: %s\n", surfaceframes, frames, filename);
		}
		if (surfacevertices <= 0 || surfacevertices > RETRO_MAX_VERTICES - vertices || surfacefaces <= 0 || surfacefaces > RETRO_MAX_FACES - faces) {
			RETRO_RageQuit("MD3 model does not fit the model lists: %s\n", filename);
		}
		// Division avoids overflowing on a malformed count
		if (faceoffset < 108 || faceoffset > remaining || surfacefaces > (remaining - faceoffset) / 12 || uvoffset < 108 || uvoffset > remaining || surfacevertices > (remaining - uvoffset) / 8 || pointoffset < 108 || pointoffset > remaining || (long)surfacevertices * frames > (remaining - pointoffset) / 8 || endoffset < 108 || endoffset > remaining) {
			RETRO_RageQuit("MD3 surface runs past the end of its file: %s\n", filename);
		}

		vertices += surfacevertices;
		faces += surfacefaces;
		offset += endoffset;
	}

	Model3D *model = RETRO_Allocate3DModel();
	model->vertices = vertices;
	model->uvs = vertices;
	model->faces = faces;

	RETRO_AllocateModelFrames(model, frames);

	int firstvertex = 0, firstface = 0;
	offset = surfaceoffset;

	for (int s = 0; s < surfaces; s++) {
		const unsigned char *surface = &data[offset];
		int surfacevertices = RETRO_ReadInt32(&surface[80]);
		int surfacefaces = RETRO_ReadInt32(&surface[84]);
		const unsigned char *triangle = &surface[RETRO_ReadInt32(&surface[88])];
		const unsigned char *uv = &surface[RETRO_ReadInt32(&surface[96])];
		const unsigned char *point = &surface[RETRO_ReadInt32(&surface[100])];

		// One UV per vertex, so a vertex names its UV by its own index
		for (int i = 0; i < surfacevertices; i++) {
			vec2 coordinate = { RETRO_ReadFloat32(&uv[i * 8]), RETRO_ReadFloat32(&uv[i * 8 + 4]) };
			model->uv[firstvertex + i] = coordinate * (float)RETRO_TEXMAP_SIZE;
		}

		for (int i = 0; i < surfacefaces; i++) {
			Face *face = &model->face[firstface + i];
			face->vertices = 3;
			for (int j = 0; j < 3; j++) {
				int vertex = RETRO_ReadInt32(&triangle[i * 12 + (2 - j) * 4]);
				if (vertex < 0 || vertex >= surfacevertices) {
					RETRO_RageQuit("Face names a vertex the file does not define: %s\n", filename);
				}
				face->vertex[j] = firstvertex + vertex;
				face->uv[j] = firstvertex + vertex;
			}
		}

		for (int frame = 0; frame < frames; frame++) {
			const unsigned char *source = &point[(size_t)frame * surfacevertices * 8];
			float *pose = &model->frame[((size_t)frame * vertices + firstvertex) * 3];
			for (int i = 0; i < surfacevertices; i++) {
				float x = (short)(source[i * 8] | source[i * 8 + 1] << 8) / scale;
				float y = (short)(source[i * 8 + 2] | source[i * 8 + 3] << 8) / scale;
				float z = (short)(source[i * 8 + 4] | source[i * 8 + 5] << 8) / scale;
				pose[i * 3] = x;
				pose[i * 3 + 1] = z;
				pose[i * 3 + 2] = -y;
			}
		}

		firstvertex += surfacevertices;
		firstface += surfacefaces;
		offset += RETRO_ReadInt32(&surface[104]);
	}
	free(data);

	RETRO_BlendModelPoses(0, 0, 0, model);
	RETRO_InitializeFaceNormals(model);
	RETRO_InitializeFaceTangents(model);
	RETRO_InitializeVertexNormals(model);

	return model;
}

inline void RETRO_Save3DModel(const char *filename, Model3D *model)
{
	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	// Save header. The object name names the object within the file, so it is
	// the file's stem rather than the path it is being written to
	const char *stem = strrchr(filename, '/');
	stem = stem ? stem + 1 : filename;
	const char *extension = strrchr(stem, '.');
	fprintf(fp, "o %.*s\n", extension ? (int)(extension - stem) : (int)strlen(stem), stem);

	// Save vertices
	for (int i = 0; i < model->vertices; i++) {
		fprintf(fp, "v %f %f %f\n", model->vertex[i].pos.x, model->vertex[i].pos.y, model->vertex[i].pos.z);
	}

	// Save UV coordinates, back from texels to the 0 to 1 the loader scales up.
	// They are texels of the model's own map, which need not be the loader's size
	for (int i = 0; i < model->uvs; i++) {
		fprintf(fp, "vt %f %f\n", model->uv[i].x / model->texmapwidth, model->uv[i].y / model->texmapheight);
	}

	// Save normals
	for (int i = 0; i < model->normals; i++) {
		fprintf(fp, "vn %f %f %f\n", model->normal[i].dir.x, model->normal[i].dir.y, model->normal[i].dir.z);
	}

	// Save faces, each run of one material under its name
	int material = -1;
	for (int i = 0; i < model->faces; i++) {
		// The loader reads three corners or more, so only those are written
		Face *face = &model->face[i];
		if (face->vertices < 3) {
			continue;
		}
		if (model->materials > 0 && face->material != material) {
			material = face->material;
			fprintf(fp, "usemtl %s\n", model->material[material]);
		}
		fprintf(fp, "f");
		for (int j = 0; j < face->vertices; j++) {
			fprintf(fp, " %d/%d/%d", face->vertex[j] + 1, face->uv[j] + 1, face->vertexnormal[j] + 1);
		}
		fprintf(fp, "\n");
	}

	fclose(fp);
}

inline void RETRO_Deinitialize_3D(void)
{
	for (int i = 0; i < RETRO_MAX_MODELS; i++) {
		RETRO_Free3DModel(i);
	}
}

#endif
