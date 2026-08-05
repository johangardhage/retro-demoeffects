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
#define RETRO_MAX_NORMALS 1000
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

	// Save header
	fprintf(fp, "o %s\n", filename);

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
