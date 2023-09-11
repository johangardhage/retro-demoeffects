//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMODEL_H_
#define _RETROMODEL_H_

#define RETRO_MAX_VERTICES 1000
#define RETRO_MAX_UVS 1000
#define RETRO_MAX_NORMALS 1000
#define RETRO_MAX_FACES 2000
#define RETRO_MAX_FACEVERTICES 5

struct Vertex {
	float x, y, z;				// Original coordinates
	float rx, ry, rz;			// Rotated coordinates
	float sx, sy;				// Screen coordinates
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
	int c;								// Color
	Normal facenormal;					// Face normal
	bool visible;						// Is the face visible?
	int z;								// Z center, used for Quicksort
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
	unsigned char *envmap = NULL;			// Environment texture
	unsigned char *bumpmap = NULL;			// Bump texture
};

struct {
	Model3D *model = NULL;
} RETRO_Model;

Model3D *RETRO_Get3DModel(void)
{
	return RETRO_Model.model;
}

void RETRO_InitializeVertexNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	// Use vertex values for normal
	for (int i = 0; i < model->vertices; i++) {
		model->normal[i].nx = model->vertex[i].x;
		model->normal[i].ny = model->vertex[i].y;
		model->normal[i].nz = model->vertex[i].z;

		// Calculate the length of the normal (used to scale it to a unit normal)
		model->normal[i].nn = sqrt(model->normal[i].nx * model->normal[i].nx + model->normal[i].ny * model->normal[i].ny + model->normal[i].nz * model->normal[i].nz);
	}

	model->normals = model->vertices;

	// Use vertex index for normal
	for (int i = 0; i < model->faces; i++) {
		for (int j = 0; j < model->face[i].vertices; j++) {
			model->face[i].normal[j] = model->face[i].vertex[j];
		}
	}
}

void RETRO_InitializeFaceNormals(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	for (int i = 0; i < model->faces; i++) {
		// Calculate vectors
		float x1 = model->vertex[model->face[i].vertex[0]].x - model->vertex[model->face[i].vertex[1]].x;
		float y1 = model->vertex[model->face[i].vertex[0]].y - model->vertex[model->face[i].vertex[1]].y;
		float z1 = model->vertex[model->face[i].vertex[0]].z - model->vertex[model->face[i].vertex[1]].z;
		float x2 = model->vertex[model->face[i].vertex[0]].x - model->vertex[model->face[i].vertex[2]].x;
		float y2 = model->vertex[model->face[i].vertex[0]].y - model->vertex[model->face[i].vertex[2]].y;
		float z2 = model->vertex[model->face[i].vertex[0]].z - model->vertex[model->face[i].vertex[2]].z;

		// Calculate normal (using cross product)
		model->face[i].facenormal.nx = y1 * z2 - z1 * y2;
		model->face[i].facenormal.ny = z1 * x2 - x1 * z2;
		model->face[i].facenormal.nz = x1 * y2 - y1 * x2;

		// Calculate the length of the normal (used to scale it to a unit normal)
		model->face[i].facenormal.nn = sqrt(model->face[i].facenormal.nx * model->face[i].facenormal.nx +
										    model->face[i].facenormal.ny * model->face[i].facenormal.ny +
											model->face[i].facenormal.nz * model->face[i].facenormal.nz);
	}
}

Model3D *RETRO_Load3DModel(const char *filename, int scale = 256)
{
	Model3D *model = (Model3D *)malloc(sizeof(Model3D));
	if (model == NULL) {
		RETRO_RageQuit("Cannot allocate 3D model memory\n");
	}
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

	// Initialize vertex normals
	if (model->normals == 0) {
		RETRO_InitializeVertexNormals(model);
	}

	// Initialize face normals
	RETRO_InitializeFaceNormals(model);

	return model;
}

#endif
