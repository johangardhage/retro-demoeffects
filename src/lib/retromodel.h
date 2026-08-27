//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMODEL_H_
#define _RETROMODEL_H_

// The grazing height G is the height difference that tilts a normal all the
// way to grazing. The gradient across two texels, divided by the surface they
// span and by G, is the tilt, so a larger G reads shallower. A metal env map
// turns a small tilt into a different color, so it wants a shallower G; a
// shade table has only the material ramp to spend, so it takes the deeper
// default. Below about 24 the mask's bump map breaks a highlight up rather
// than roughening it. G is a height in the bump map's own units and has
// nothing to do with bumpmapheight, which is that map's number of rows.
#define RETRO_BUMP_GRAZING 32
#define RETRO_ENVMAP_SIZE 256

// The loader scales a model's UVs by this, so they come out as texels of a map this
// wide, and the renderers stride the texture and bump maps by it. All three have to agree.
#define RETRO_TEXMAP_SIZE 256

#define RETRO_MAX_VERTICES 1000
#define RETRO_MAX_UVS 1000
#define RETRO_MAX_NORMALS 1500 // a two sided mesh needs one per side of a vertex
#define RETRO_MAX_FACES 2000
#define RETRO_MAX_FACEVERTICES 5
#define RETRO_MAX_MODELS 10 // a demo can hold several models at once, as it can images

struct Vertex {
	float x, y, z;				// Model space coordinates
	float rx, ry, rz;			// Rotated coordinates
	float sx, sy, q;			// Screen coordinates and reciprocal projection depth
};

struct UV {
	float u, v;					// Texture coordinates, in texels: u across texmapwidth, v down texmapheight
};

// A unit direction: where something points. Every Direction is normalized as it
// is written, so nothing that reads one has to divide it out first, and the
// model's rotation is orthonormal, so it stays unit all the way to the drawers.
//
// Unlike a Vertex it has no screen form and does not translate, which is the
// whole distinction - RETRO_TranslateModel moves vertices and leaves these
// alone, so only the rotation reaches them. Normals, tangents and the light
// share the type because the library only ever rotates: a normal is strictly a
// covector and transforms by the inverse transpose, which for an orthonormal
// matrix is the matrix itself.
struct Direction {
	float x, y, z;				// Model space direction, unit length
	float rx, ry, rz;			// Rotated direction, still unit
};

struct Face {
	int vertices;								// Number of vertices in face
	int vertex[RETRO_MAX_FACEVERTICES];			// Index of vertices in face
	int uv[RETRO_MAX_FACEVERTICES];				// Index of UV coordinates in face
	int vertexnormal[RETRO_MAX_FACEVERTICES];	// Index of vertex normals in face
	int c;										// Front-facing offset from the model's c, in whatever
												// space that renderer measures in
	int backc;									// Back-facing offset; zero makes that side transparent
	Direction facenormal;						// Face normal
	Direction tangent;							// Surface direction of +u, for bump mapping
	Direction bitangent;						// and of +v
	float area;									// Its area, the weight it lends a vertex normal
	bool frontfacing;							// Which side the winding shows. Only set for a face that
												// reached the winding test, so read it only for a face in
												// the draw list
	float rz;									// Mean rotated depth, the painter's sort key
};

struct Model3D {
	int faces;									// Number of faces
	int vertices;								// Number of vertices
	int uvs;									// Number of UV coordinates
	int normals;								// Number of vertex normals
	Face face[RETRO_MAX_FACES];					// Face list
	Vertex vertex[RETRO_MAX_VERTICES];			// Vertex list
	UV uv[RETRO_MAX_UVS];						// UV list
	Direction normal[RETRO_MAX_NORMALS];		// Vertex normal list
	int drawfaces;								// Number of faces in the draw list
	int drawface[RETRO_MAX_FACES];				// Faces to draw, sorted far to near
	float matrix[3][3];							// Rotation matrix
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
												// RETRO_SHADES, the table's full height, for a
												// model that leaves this zero
	bool twosided;								// Draw every face from either side, shading the one
												// turned away by the reverse of its normal. A surface
												// with no inside - a sheet, an open shell - is otherwise
												// lost the moment it turns: its normals point away from
												// the viewer, so the whole of it lands on the dark end of
												// the ramp. Honoured by the shaded renderers; the Glenz
												// and wireframe paths draw both sides on their own terms
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
	unsigned char *bumpmap = NULL;				// Bump texture
	int bumpmapwidth = RETRO_TEXMAP_SIZE;		// Bump texture width, which need not match the texture's
	int bumpmapheight = RETRO_TEXMAP_SIZE;		// Bump texture height
	int bumpgrazing = RETRO_BUMP_GRAZING;		// Height difference that tilts a normal to grazing
};

struct {
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
Model3D *RETRO_Get3DModel(int id = 0)
{
	return id >= 0 && id < RETRO_MAX_MODELS ? RETRO_Model.model[id] : NULL;
}

//
// Allocate a model and register it
//
// A model built in code rather than read from a file is allocated here too, so
// it is reached and released like any other. Defaults are assigned by hand
// because malloc and memset go around the member initializers Model3D declares.
//
Model3D *RETRO_Allocate3DModel(void)
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

	Model3D *model = (Model3D *)malloc(sizeof(Model3D));
	if (model == NULL) {
		RETRO_RageQuit("Cannot allocate 3D model memory\n");
	}
	memset(model, 0, sizeof(Model3D));
	model->texmapwidth = RETRO_TEXMAP_SIZE;
	model->texmapheight = RETRO_TEXMAP_SIZE;
	model->envmapwidth = RETRO_ENVMAP_SIZE;
	model->envmapheight = RETRO_ENVMAP_SIZE;
	model->bumpmapwidth = RETRO_TEXMAP_SIZE;
	model->bumpmapheight = RETRO_TEXMAP_SIZE;
	model->envmapradius = RETRO_ENVMAP_SIZE / 2;
	model->bumpgrazing = RETRO_BUMP_GRAZING;

	RETRO_Model.model[id] = model;
	RETRO_Model.models++;

	return model;
}

void RETRO_Free3DModel(int id = 0)
{
	if (id >= 0 && id < RETRO_MAX_MODELS && RETRO_Model.model[id]) {
		free(RETRO_Model.model[id]->frame);
		free(RETRO_Model.model[id]);
		RETRO_Model.model[id] = NULL;
		RETRO_Model.models--;
	}
}

// Area-weighted average of the adjacent face normals (Hearn & Baker / Foley).
// Each face lends its unit normal scaled by Face::area, so a large face pulls
// harder on the vertices it meets than a small one does. Needs face normals
// first. For a cube at the origin this comes out along the vertex position; for
// a general mesh it does not.
void RETRO_InitializeVertexNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->normal[i].x = 0;
		model->normal[i].y = 0;
		model->normal[i].z = 0;
	}

	for (int i = 0; i < model->faces; i++) {
		float nx = model->face[i].facenormal.x * model->face[i].area;
		float ny = model->face[i].facenormal.y * model->face[i].area;
		float nz = model->face[i].facenormal.z * model->face[i].area;

		for (int j = 0; j < model->face[i].vertices; j++) {
			int v = model->face[i].vertex[j];
			model->normal[v].x += nx;
			model->normal[v].y += ny;
			model->normal[v].z += nz;
		}
	}

	for (int i = 0; i < model->vertices; i++) {
		float length = sqrt(model->normal[i].x * model->normal[i].x + model->normal[i].y * model->normal[i].y + model->normal[i].z * model->normal[i].z);
		float inverselength = length > 0.0f ? 1.0f / length : 0.0f;
		model->normal[i].x *= inverselength;
		model->normal[i].y *= inverselength;
		model->normal[i].z *= inverselength;
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
void RETRO_InitializeFaceNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		float x1 = model->vertex[model->face[i].vertex[0]].x - model->vertex[model->face[i].vertex[1]].x;
		float y1 = model->vertex[model->face[i].vertex[0]].y - model->vertex[model->face[i].vertex[1]].y;
		float z1 = model->vertex[model->face[i].vertex[0]].z - model->vertex[model->face[i].vertex[1]].z;
		float x2 = model->vertex[model->face[i].vertex[0]].x - model->vertex[model->face[i].vertex[2]].x;
		float y2 = model->vertex[model->face[i].vertex[0]].y - model->vertex[model->face[i].vertex[2]].y;
		float z2 = model->vertex[model->face[i].vertex[0]].z - model->vertex[model->face[i].vertex[2]].z;

		float nx = y1 * z2 - z1 * y2;
		float ny = z1 * x2 - x1 * z2;
		float nz = x1 * y2 - y1 * x2;

		float length = sqrt(nx * nx + ny * ny + nz * nz);
		float inverselength = length > 0.0f ? 1.0f / length : 0.0f;

		model->face[i].facenormal.x = nx * inverselength;
		model->face[i].facenormal.y = ny * inverselength;
		model->face[i].facenormal.z = nz * inverselength;

		// Fan the rest of the face from vertex 0, a triangle at a time, so the area
		// is the whole face's and not just the first triangle's
		float area = length / 2;

		for (int j = 3; j < model->face[i].vertices; j++) {
			float x3 = model->vertex[model->face[i].vertex[0]].x - model->vertex[model->face[i].vertex[j - 1]].x;
			float y3 = model->vertex[model->face[i].vertex[0]].y - model->vertex[model->face[i].vertex[j - 1]].y;
			float z3 = model->vertex[model->face[i].vertex[0]].z - model->vertex[model->face[i].vertex[j - 1]].z;
			float x4 = model->vertex[model->face[i].vertex[0]].x - model->vertex[model->face[i].vertex[j]].x;
			float y4 = model->vertex[model->face[i].vertex[0]].y - model->vertex[model->face[i].vertex[j]].y;
			float z4 = model->vertex[model->face[i].vertex[0]].z - model->vertex[model->face[i].vertex[j]].z;

			float fx = y3 * z4 - z3 * y4;
			float fy = z3 * x4 - x3 * z4;
			float fz = x3 * y4 - y3 * x4;

			area += sqrt(fx * fx + fy * fy + fz * fz) / 2;
		}

		model->face[i].area = area;
	}
}

//
// Tangent frame of each face, from its UV parametrisation
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
void RETRO_InitializeFaceTangents(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		Face *face = &model->face[i];

		float nx = face->facenormal.x;
		float ny = face->facenormal.y;
		float nz = face->facenormal.z;

		float tx = 0, ty = 0, tz = 0, bx = 0, by = 0, bz = 0;
		bool derived = false;

		if (model->uvs > 0 && face->vertices >= 3) {
			Vertex *p0 = &model->vertex[face->vertex[0]];
			Vertex *p1 = &model->vertex[face->vertex[1]];
			Vertex *p2 = &model->vertex[face->vertex[2]];
			UV *t0 = &model->uv[face->uv[0]];
			UV *t1 = &model->uv[face->uv[1]];
			UV *t2 = &model->uv[face->uv[2]];

			float e1x = p1->x - p0->x, e1y = p1->y - p0->y, e1z = p1->z - p0->z;
			float e2x = p2->x - p0->x, e2y = p2->y - p0->y, e2z = p2->z - p0->z;
			float du1 = t1->u - t0->u, dv1 = t1->v - t0->v;
			float du2 = t2->u - t0->u, dv2 = t2->v - t0->v;

			float determinant = du1 * dv2 - du2 * dv1;
			if (fabs(determinant) > 1.0e-12f) {
				float r = 1.0f / determinant;
				tx = (e1x * dv2 - e2x * dv1) * r;
				ty = (e1y * dv2 - e2y * dv1) * r;
				tz = (e1z * dv2 - e2z * dv1) * r;
				bx = (e2x * du1 - e1x * du2) * r;
				by = (e2y * du1 - e1y * du2) * r;
				bz = (e2z * du1 - e1z * du2) * r;
				derived = true;
			}
		}

		if (!derived) {
			// Any direction not parallel to the normal
			tx = fabs(nx) < 0.9f ? 1.0f : 0.0f;
			ty = fabs(nx) < 0.9f ? 0.0f : 1.0f;
			tz = 0.0f;
			bx = ny * tz - nz * ty;
			by = nz * tx - nx * tz;
			bz = nx * ty - ny * tx;
		}

		// Gram-Schmidt: drop the part of T along N, then of B along both
		float nt = nx * tx + ny * ty + nz * tz;
		tx -= nx * nt;
		ty -= ny * nt;
		tz -= nz * nt;
		float tlength = sqrt(tx * tx + ty * ty + tz * tz);
		if (tlength > 1.0e-12f) {
			tx /= tlength;
			ty /= tlength;
			tz /= tlength;
		}

		float nb = nx * bx + ny * by + nz * bz;
		float tb = tx * bx + ty * by + tz * bz;
		bx -= nx * nb + tx * tb;
		by -= ny * nb + ty * tb;
		bz -= nz * nb + tz * tb;
		float blength = sqrt(bx * bx + by * by + bz * bz);
		if (blength > 1.0e-12f) {
			bx /= blength;
			by /= blength;
			bz /= blength;
		} else {
			bx = ny * tz - nz * ty;
			by = nz * tx - nx * tz;
			bz = nx * ty - ny * tx;
		}

		face->tangent.x = tx;
		face->tangent.y = ty;
		face->tangent.z = tz;
		face->bitangent.x = bx;
		face->bitangent.y = by;
		face->bitangent.z = bz;
	}
}

//
// UVs that lay the whole texture over every face, in the face's own frame
//
// A model can arrive with its faces sharing one atlas, or with no usable UVs
// at all. Reparametrising it here hands each face the texture entire, in a
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
// quad keeps one parametrisation after being split into triangles. A face
// carries its own UVs afterwards, one set per corner, and the tangent frames,
// which are derived from the UVs, are rebuilt to match.
//
void RETRO_InitializeFaceUVs(Model3D *model = NULL)
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

	// Bounding box centre and half extent
	float minx = model->vertex[0].x, maxx = minx;
	float miny = model->vertex[0].y, maxy = miny;
	float minz = model->vertex[0].z, maxz = minz;

	for (int i = 1; i < model->vertices; i++) {
		minx = MIN(minx, model->vertex[i].x);
		maxx = MAX(maxx, model->vertex[i].x);
		miny = MIN(miny, model->vertex[i].y);
		maxy = MAX(maxy, model->vertex[i].y);
		minz = MIN(minz, model->vertex[i].z);
		maxz = MAX(maxz, model->vertex[i].z);
	}

	float cx = (minx + maxx) / 2, hx = (maxx - minx) / 2;
	float cy = (miny + maxy) / 2, hy = (maxy - miny) / 2;
	float cz = (minz + maxz) / 2, hz = (maxz - minz) / 2;

	model->uvs = 0;

	for (int i = 0; i < model->faces; i++) {
		Face *face = &model->face[i];

		float nx = face->facenormal.x;
		float ny = face->facenormal.y;
		float nz = face->facenormal.z;

		// t = n × (0, 1, 0)
		float tx = -nz, ty = 0, tz = nx;
		float tlength = sqrt(tx * tx + ty * ty + tz * tz);
		if (tlength > 1.0e-12f) {
			tx /= tlength;
			ty /= tlength;
			tz /= tlength;
		} else {
			tx = 1.0f, ty = 0.0f, tz = 0.0f;
		}

		// b = t × n
		float bx = ty * nz - tz * ny;
		float by = tz * nx - tx * nz;
		float bz = tx * ny - ty * nx;

		// The box reaches this far along each of them
		float textent = fabs(tx) * hx + fabs(ty) * hy + fabs(tz) * hz;
		float bextent = fabs(bx) * hx + fabs(by) * hy + fabs(bz) * hz;
		float inversetextent = textent > 0.0f ? 1.0f / textent : 0.0f;
		float inversebextent = bextent > 0.0f ? 1.0f / bextent : 0.0f;

		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];

			float px = vertex->x - cx, py = vertex->y - cy, pz = vertex->z - cz;
			float u = ((px * tx + py * ty + pz * tz) * inversetextent + 1) / 2;
			float v = ((px * bx + py * by + pz * bz) * inversebextent + 1) / 2;

			model->uv[model->uvs].u = u * model->texmapwidth;
			model->uv[model->uvs].v = v * model->texmapheight;
			face->uv[j] = model->uvs;
			model->uvs++;
		}
	}

	RETRO_InitializeFaceTangents(model);
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
void RETRO_Load3DModelFrames(Model3D *model, const char *pattern, int frames)
{
	if (frames <= 0) {
		RETRO_RageQuit("An animation needs at least one frame: %s\n", pattern);
	}

	free(model->frame);
	model->frame = (float *)malloc((size_t)frames * model->vertices * 3 * sizeof(float));
	if (model->frame == NULL) {
		RETRO_RageQuit("Cannot allocate animation memory\n");
	}
	model->frames = frames;

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
// run over the result before it is drawn
//
void RETRO_MorphModel(float u, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	if (model->frames == 0) {
		RETRO_RageQuit("RETRO_MorphModel needs an animation, load one with RETRO_Load3DModel\n");
	}

	float f = CLAMP01(u) * (model->frames - 1);
	int a = f;
	// u = 1 lands on the last pose with nothing past it to mix toward, and s is
	// zero there, so b carries no weight and only has to stay in range
	int b = MIN(a + 1, model->frames - 1);
	float s = f - a;

	const float *from = &model->frame[(size_t)a * model->vertices * 3];
	const float *to = &model->frame[(size_t)b * model->vertices * 3];

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].x = from[i * 3] * (1.0f - s) + to[i * 3] * s;
		model->vertex[i].y = from[i * 3 + 1] * (1.0f - s) + to[i * 3 + 1] * s;
		model->vertex[i].z = from[i * 3 + 2] * (1.0f - s) + to[i * 3 + 2] * s;
	}
}

//
// Load a model, scaling its 0 to 1 UVs into texels, and with it the animation
// that poses it, if it is given one
//
// Nothing downstream converts them: a drawer indexes the texture with what UV holds, so
// they have to be texels by the time it gets there, and this is where that happens. The
// map they are scaled into is RETRO_TEXMAP_SIZE square, which is what texmapwidth and
// texmapheight start out as, and no demo has yet given a model a texture of another size.
//
// The animation is a printf pattern and a frame count, handed on to
// RETRO_Load3DModelFrames, and the file named here is the model those poses are
// read against: it is what fixes the topology and how many vertices a pose has
// to name. A model with no animation is loaded exactly as it was before there
// were any, since the pattern defaults to none
//
Model3D *RETRO_Load3DModel(const char *filename, const char *animation = NULL, int frames = 0)
{
	Model3D *model = RETRO_Allocate3DModel();

	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	int vertices = 0, uvs = 0, normals = 0, faces = 0;

	// Check before writing: overflow walks into the next list in this struct.
	char row[128];
	while (fscanf(fp, "%127s", row) != EOF) {
		if (strcmp(row, "v") == 0) { // Load vertices
			if (vertices >= RETRO_MAX_VERTICES) {
				RETRO_RageQuit("Too many vertices to fit the vertex list: %s\n", filename);
			}
			if (fscanf(fp, "%f %f %f\n", &model->vertex[vertices].x, &model->vertex[vertices].y, &model->vertex[vertices].z) != 3) {
				RETRO_RageQuit("Cannot read vertex, expected three floats: %s\n", filename);
			}
			vertices++;
		} else if (strcmp(row, "vt") == 0) { // Load UV coordinates
			if (uvs >= RETRO_MAX_UVS) {
				RETRO_RageQuit("Too many UV coordinates to fit the UV list: %s\n", filename);
			}
			if (fscanf(fp, "%f %f\n", &model->uv[uvs].u, &model->uv[uvs].v) != 2) {
				RETRO_RageQuit("Cannot read UV coordinate, expected two floats: %s\n", filename);
			}
			model->uv[uvs].u *= RETRO_TEXMAP_SIZE;
			model->uv[uvs].v *= RETRO_TEXMAP_SIZE;
			uvs++;
		} else if (strcmp(row, "vn") == 0) { // Load normals
			if (normals >= RETRO_MAX_NORMALS) {
				RETRO_RageQuit("Too many normals to fit the normal list: %s\n", filename);
			}
			if (fscanf(fp, "%f %f %f\n", &model->normal[normals].x, &model->normal[normals].y, &model->normal[normals].z) != 3) {
				RETRO_RageQuit("Cannot read normal, expected three floats: %s\n", filename);
			}
			// A file's vn need not be unit, and everything downstream assumes it is
			float length = sqrt(model->normal[normals].x * model->normal[normals].x +
								model->normal[normals].y * model->normal[normals].y +
								model->normal[normals].z * model->normal[normals].z);
			float inverselength = length > 0.0f ? 1.0f / length : 0.0f;
			model->normal[normals].x *= inverselength;
			model->normal[normals].y *= inverselength;
			model->normal[normals].z *= inverselength;
			normals++;
		} else if (strcmp(row, "f") == 0) {
			if (faces >= RETRO_MAX_FACES) {
				RETRO_RageQuit("Too many faces to fit the face list: %s\n", filename);
			}

			// int, not unsigned: %d writes an int, and an index of 0 in the file
			// would wrap on the -1 below before the range check ever saw it
			int vertex[4], uv[4], normal[4];
			int matches = fscanf(fp, "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n", &vertex[0], &uv[0], &normal[0], &vertex[1], &uv[1], &normal[1], &vertex[2], &uv[2], &normal[2], &vertex[3], &uv[3], &normal[3]);

			// Whole triples only: matches / 3 would keep a short face whose leftover
			// zeros pass the bounds check.
			if (matches != 9 && matches != 12) {
				RETRO_RageQuit("Cannot read face, expected three or four v/uv/n triples: %s\n", filename);
			}

			model->face[faces].vertices = matches / 3;

			// Store vertex indices to face
			for (int i = 0; i < model->face[faces].vertices; i++) {
				model->face[faces].vertex[i] = vertex[i] - 1;
				model->face[faces].uv[i] = uv[i] - 1;
				model->face[faces].vertexnormal[i] = normal[i] - 1;
			}
			faces++;
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

//	printf("Vertices: %i\n", vertices);
//	printf("Vertex UV coords: %i\n", uvs);
//	printf("Normals: %i\n", normals);
//	printf("Faces: %i\n", faces);

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

void RETRO_Save3DModel(const char *filename, Model3D *model)
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
		fprintf(fp, "v %f %f %f\n", model->vertex[i].x, model->vertex[i].y, model->vertex[i].z);
	}

	// Save UV coordinates
	for (int i = 0; i < model->uvs; i++) {
		fprintf(fp, "vt %f %f\n", model->uv[i].u, model->uv[i].v);
	}

	// Save normals
	for (int i = 0; i < model->normals; i++) {
		fprintf(fp, "vn %f %f %f\n", model->normal[i].x, model->normal[i].y, model->normal[i].z);
	}

	// Save faces
	for (int i = 0; i < model->faces; i++) {
		if (model->face[i].vertices == 3) {
			fprintf(fp, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", model->face[i].vertex[0] + 1, model->face[i].uv[0] + 1, model->face[i].vertexnormal[0] + 1,
														  model->face[i].vertex[1] + 1, model->face[i].uv[1] + 1, model->face[i].vertexnormal[1] + 1,
														  model->face[i].vertex[2] + 1, model->face[i].uv[2] + 1, model->face[i].vertexnormal[2] + 1);
		} if (model->face[i].vertices == 4) {
			fprintf(fp, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n", model->face[i].vertex[0] + 1, model->face[i].uv[0] + 1, model->face[i].vertexnormal[0] + 1,
																   model->face[i].vertex[1] + 1, model->face[i].uv[1] + 1, model->face[i].vertexnormal[1] + 1,
																   model->face[i].vertex[2] + 1, model->face[i].uv[2] + 1, model->face[i].vertexnormal[2] + 1,
																   model->face[i].vertex[3] + 1, model->face[i].uv[3] + 1, model->face[i].vertexnormal[3] + 1);
		}
	}

	fclose(fp);
}

#endif
