//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMODEL_H_
#define _RETROMODEL_H_

// Bump height H is the height difference that tilts a normal all the way to
// grazing. The gradient across two texels, divided by the surface they span
// and by H, is the tilt, so a larger H reads shallower. A metal env map turns
// a small tilt into a different color, so it wants a shallower H; a shade
// table has only the material ramp to spend, so it takes the deeper default.
// Below about 24 the mask's bump map breaks a highlight up rather than
// roughening it.
#define RETRO_BUMP_HEIGHT 32
#define RETRO_ENVMAP_SIZE 256

// The loader scales a model's UVs by this, so they come out as texels of a map this
// wide, and the renderers stride the texture and bump maps by it. All three have to agree.
#define RETRO_TEXMAP_SIZE 256

#define RETRO_MAX_VERTICES 1000
#define RETRO_MAX_UVS 1000
#define RETRO_MAX_NORMALS 1500 // a two sided mesh needs one per side of a vertex
#define RETRO_MAX_FACES 2000
#define RETRO_MAX_FACEVERTICES 5

struct Vertex {
	float x, y, z;				// Original coordinates
	float rx, ry, rz;			// Rotated coordinates
	float sx, sy, q;			// Screen coordinates and reciprocal projection depth
};

struct UV {
	float u, v;					// Original UV coordinates
};

struct Normal {
	float nx, ny, nz, nn;		// Original normal coordinates
	float rnx, rny, rnz;		// Rotated normal coordinates
};

struct Face {
	int vertices;						// Number of vertices in face
	int vertex[RETRO_MAX_FACEVERTICES];	// Index of vertices in face
	int uv[RETRO_MAX_FACEVERTICES];		// Index of UV coordinates in face
	int normal[RETRO_MAX_FACEVERTICES];	// Index of normals in face
	int c;								// Front-facing color
	int backc;							// Back-facing color; zero makes that side transparent
	Normal facenormal;					// Face normal
	Normal tangent;						// Surface direction of +u, for bump mapping
	Normal bitangent;					// and of +v
	bool visible;						// Passes the near-plane and front-facing tests
	float z;							// Z center, used for Quicksort
};

struct Model3D {
	int faces;								// Number of total faces
	int vertices;							// Number of total vertices
	int uvs;								// Number of total uv coordinates
	int normals;							// Number of total vertex normals
	Face face[RETRO_MAX_FACES];				// Face list
	Vertex vertex[RETRO_MAX_VERTICES];		// Vertex list
	UV uv[RETRO_MAX_UVS];					// UV list
	Normal normal[RETRO_MAX_NORMALS];		// Normal list
	int visiblefaces;						// Number of visible faces
	int visibleface[RETRO_MAX_FACES];		// Visible faces
	float matrix[3][3];						// Rotation matrix
	int c;									// Base color
	int cintensity;							// Color intensity
	unsigned char *texmap = NULL;			// Texture
	int texmapwidth = RETRO_TEXMAP_SIZE;	// Texture width, which is also the space the UVs are in
	int texmapheight = RETRO_TEXMAP_SIZE;	// Texture height
	unsigned char *shadetable = NULL;		// Texture lighting table
	unsigned char *envmap = NULL;			// Environment texture
	int envmapwidth = RETRO_ENVMAP_SIZE;		// Environment texture width
	int envmapheight = RETRO_ENVMAP_SIZE;		// Environment texture height
	int envmapintensity = 128;				// How far from the texture's middle a grazing normal reaches
	unsigned char *bumpmap = NULL;			// Bump texture
	int bumpmapwidth = RETRO_TEXMAP_SIZE;	// Bump texture width, which need not match the texture's
	int bumpmapheight = RETRO_TEXMAP_SIZE;	// Bump texture height
	int bumpheight = RETRO_BUMP_HEIGHT;		// Height difference that tilts a normal to grazing
};

struct {
	Model3D *model = NULL;
} RETRO_Model;

Model3D *RETRO_Get3DModel(void)
{
	return RETRO_Model.model;
}

// Area-weighted average of the adjacent face normals (Hearn & Baker / Foley).
// The unnormalized face normal has length 2 * area, so adding it weights by area.
// Needs face normals first. For a cube at the origin this is the same direction
// as the vertex position; for a general mesh it is not.
void RETRO_InitializeVertexNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->vertices; i++) {
		model->normal[i].nx = 0;
		model->normal[i].ny = 0;
		model->normal[i].nz = 0;
	}

	for (int i = 0; i < model->faces; i++) {
		float nx = model->face[i].facenormal.nx;
		float ny = model->face[i].facenormal.ny;
		float nz = model->face[i].facenormal.nz;

		for (int j = 0; j < model->face[i].vertices; j++) {
			int v = model->face[i].vertex[j];
			model->normal[v].nx += nx;
			model->normal[v].ny += ny;
			model->normal[v].nz += nz;
		}
	}

	for (int i = 0; i < model->vertices; i++) {
		model->normal[i].nn = sqrt(model->normal[i].nx * model->normal[i].nx + model->normal[i].ny * model->normal[i].ny + model->normal[i].nz * model->normal[i].nz);
	}

	model->normals = model->vertices;

	for (int i = 0; i < model->faces; i++) {
		for (int j = 0; j < model->face[i].vertices; j++) {
			model->face[i].normal[j] = model->face[i].vertex[j];
		}
	}
}

// N = (v0 - v1) × (v0 - v2), the geometric normal of the first triangle of the face.
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

		model->face[i].facenormal.nx = y1 * z2 - z1 * y2;
		model->face[i].facenormal.ny = z1 * x2 - x1 * z2;
		model->face[i].facenormal.nz = x1 * y2 - y1 * x2;

		model->face[i].facenormal.nn = sqrt(model->face[i].facenormal.nx * model->face[i].facenormal.nx +
										    model->face[i].facenormal.ny * model->face[i].facenormal.ny +
											model->face[i].facenormal.nz * model->face[i].facenormal.nz);
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

		float inversenormallength = face->facenormal.nn > 0.0f ? 1.0f / face->facenormal.nn : 0.0f;
		float nx = face->facenormal.nx * inversenormallength;
		float ny = face->facenormal.ny * inversenormallength;
		float nz = face->facenormal.nz * inversenormallength;

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

		face->tangent.nx = tx;
		face->tangent.ny = ty;
		face->tangent.nz = tz;
		face->tangent.nn = 1.0f;
		face->bitangent.nx = bx;
		face->bitangent.ny = by;
		face->bitangent.nz = bz;
		face->bitangent.nn = 1.0f;
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

		float inversenormallength = face->facenormal.nn > 0.0f ? 1.0f / face->facenormal.nn : 0.0f;
		float nx = face->facenormal.nx * inversenormallength;
		float ny = face->facenormal.ny * inversenormallength;
		float nz = face->facenormal.nz * inversenormallength;

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
// Load a model, scaling its UVs by scale to turn them into texels
//
// A model whose UVs run 0 to 1 wants the texture's width here. One that already carries
// texels wants 1. Either way the texture's own size is texmapwidth, which is a separate
// fact about the image rather than about the model.
//
Model3D *RETRO_Load3DModel(const char *filename, int scale = RETRO_TEXMAP_SIZE)
{
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
	model->envmapintensity = RETRO_ENVMAP_SIZE / 2;
	model->bumpheight = RETRO_BUMP_HEIGHT;
	RETRO_Model.model = model;

	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	int vertices = 0, uvs = 0, normals = 0, faces = 0;

	char row[128];
	while (fscanf(fp, "%s", row) != EOF) {
		if (strcmp(row, "v") == 0) { // Load vertices
			fscanf(fp, "%f %f %f\n", &model->vertex[vertices].x, &model->vertex[vertices].y, &model->vertex[vertices].z);
			vertices++;
		} else if (strcmp(row, "vt") == 0) { // Load UV coordinates
			fscanf(fp, "%f %f\n", &model->uv[uvs].u, &model->uv[uvs].v);
			model->uv[uvs].u *= scale;
			model->uv[uvs].v *= scale;
			uvs++;
		} else if (strcmp(row, "vn") == 0) { // Load normals
			fscanf(fp, "%f %f %f\n", &model->normal[normals].nx, &model->normal[normals].ny, &model->normal[normals].nz);
			// Calculate normal length
			model->normal[normals].nn = sqrt(model->normal[normals].nx * model->normal[normals].nx +
											 model->normal[normals].ny * model->normal[normals].ny +
											 model->normal[normals].nz * model->normal[normals].nz);
			normals++;
		} else if (strcmp(row, "f") == 0) {
			unsigned int vertex[4], uv[4], normal[4];
			int matches = fscanf(fp, "%d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n", &vertex[0], &uv[0], &normal[0], &vertex[1], &uv[1], &normal[1], &vertex[2], &uv[2], &normal[2], &vertex[3], &uv[3], &normal[3]);
			model->face[faces].vertices = matches / 3;

			// Store vertex indices to face
			for (int i = 0; i < model->face[faces].vertices; i++) {
				model->face[faces].vertex[i] = vertex[i] - 1;
				model->face[faces].uv[i] = uv[i] - 1;
				model->face[faces].normal[i] = normal[i] - 1;
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
		fprintf(fp, "vn %f %f %f\n", model->normal[i].nx, model->normal[i].ny, model->normal[i].nz);
	}

	// Save faces
	for (int i = 0; i < model->faces; i++) {
		if (model->face[i].vertices == 3) {
			fprintf(fp, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", model->face[i].vertex[0] + 1, model->face[i].uv[0] + 1, model->face[i].normal[0] + 1,
														  model->face[i].vertex[1] + 1, model->face[i].uv[1] + 1, model->face[i].normal[1] + 1,
														  model->face[i].vertex[2] + 1, model->face[i].uv[2] + 1, model->face[i].normal[2] + 1);
		} if (model->face[i].vertices == 4) {
			fprintf(fp, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d\n", model->face[i].vertex[0] + 1, model->face[i].uv[0] + 1, model->face[i].normal[0] + 1,
																   model->face[i].vertex[1] + 1, model->face[i].uv[1] + 1, model->face[i].normal[1] + 1,
																   model->face[i].vertex[2] + 1, model->face[i].uv[2] + 1, model->face[i].normal[2] + 1,
																   model->face[i].vertex[3] + 1, model->face[i].uv[3] + 1, model->face[i].normal[3] + 1);
		}
	}

	fclose(fp);
}

#endif
