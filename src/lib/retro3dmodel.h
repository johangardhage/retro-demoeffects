//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETRO3DMODEL_H_
#define _RETRO3DMODEL_H_

#include "retro.h"
#include "retropoly.h"
#include "retrogfx.h"
#include "retrocolor.h"

#define RETRO_MAX_VERTICES 1000
#define RETRO_MAX_UVS 1000
#define RETRO_MAX_NORMALS 1000
#define RETRO_MAX_FACES 2000
#define RETRO_MAX_FACEVERTICES 5

typedef enum {
	RETRO_POLY_DOT,
	RETRO_POLY_WIREFRAME,
	RETRO_POLY_WIREFIRE,
	RETRO_POLY_HIDDENLINE,
	RETRO_POLY_FLAT,
	RETRO_POLY_GLENZ,
	RETRO_POLY_GOURAUD,
	RETRO_POLY_PHONG,
	RETRO_POLY_TEXTURE,
	RETRO_POLY_ENVIRONMENT
} RETRO_POLY_TYPE;

typedef enum {
	RETRO_SHADE_NONE,
	RETRO_SHADE_TABLE,
	RETRO_SHADE_FLAT,
	RETRO_SHADE_GOURAUD,
	RETRO_SHADE_ENVIRONMENT,
	RETRO_SHADE_PHONG
} RETRO_POLY_SHADE;

struct Vertex {
	float x, y, z;				// Original coordinates
	float rx, ry, rz;			// Rotated coordinates
	float sx, sy;				// Screen coordinates
};

struct UV {
	float u, v;					// Original UV coordinates
};

struct VertexNormal {
	float nx, ny, nz, nn;		// Original normal coordinates
	float rnx, rny, rnz;		// Rotated normal coordinates
};

struct Face {
	int vertices;						// Number of vertices in face
	int vertex[RETRO_MAX_FACEVERTICES];	// Index of vertices in face
	int uv[RETRO_MAX_FACEVERTICES];		// Index of UV coordinates in face
	int normal[RETRO_MAX_FACEVERTICES];	// Index of normals in face
	int c;								// Color
	float nx, ny, nz;					// Normal coordinates
	float rnx, rny, rnz, nn;			// Rotated normal coordinates
	float znm;							// Z
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
	VertexNormal normal[RETRO_MAX_NORMALS];	// Normal list
	int visiblefaces;						// Number of visible faces
	int visibleface[RETRO_MAX_FACES];		// Visible faces
	float matrix[3][3];						// Rotation matrix
	int c;									// Base color
	int cintensity;							// Color intensity
	unsigned char *texmap = NULL;			// Texture
	unsigned char *envmap = NULL;			// Environment texture
	unsigned char *bumpmap = NULL;			// Bump texture
};

struct LightSourceType {
	float x, y, z;			// Original coordinates
	float nx, ny, nz, nn;	// Rotated coordinates
};

int RETRO_UVlookup[256];

LightSourceType LightSource;

Model3D *RETRO_3dmodel = NULL;

Model3D *RETRO_Get3DModel(void)
{
	return RETRO_3dmodel;
}

void RETRO_RenderModel(RETRO_POLY_TYPE rendertype, RETRO_POLY_SHADE shadertype = RETRO_SHADE_NONE, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	if (rendertype == RETRO_POLY_DOT) {
		for (int i = 0; i < model->vertices; i++) {
			RETRO_PutPixel(model->vertex[i].sx, model->vertex[i].sy, model->c);
		}
	} else if (rendertype == RETRO_POLY_WIREFRAME) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int color = model->c + face->c;

			for (int j = 0; j < face->vertices; j++) {
				PolygonPoint *p1 = points + j;
				PolygonPoint *p2 = points + (j + 1) % face->vertices;
				RETRO_DrawLine(p1->x, p1->y, p2->x, p2->y, color);
			}
		}
	} else if (rendertype == RETRO_POLY_WIREFIRE) {
		unsigned char color[RETRO_WIDTH];
		for (int i = 0; i < RETRO_WIDTH; i++) {
			color[i] = model->c + RANDOM(model->cintensity);
		}

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			for (int j = 0; j < face->vertices; j++) {
				PolygonPoint *p1 = points + j;
				PolygonPoint *p2 = points + (j + 1) % face->vertices;
				RETRO_DrawLine2(p1->x, p1->y, p2->x, p2->y, color);
			}
		}
	} else if (rendertype == RETRO_POLY_HIDDENLINE) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int color = model->c + face->c;

			for (int j = 0; j < face->vertices; j++) {
				PolygonPoint *p1 = points + j;
				PolygonPoint *p2 = points + (j + 1) % face->vertices;
				RETRO_DrawLine(p1->x, p1->y, p2->x, p2->y, color);
			}
		}
	} else if (rendertype == RETRO_POLY_FLAT) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int color = model->c + face->c;
			float lint = 0;
			if (shadertype == RETRO_SHADE_FLAT) {
				lint = (face->rnx * LightSource.nx + face->rny * LightSource.ny + face->rnz * LightSource.nz) / (face->nn * LightSource.nn);
				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				color = CLAMP(model->c + face->c + lint * model->cintensity - model->c, cmin, cmax);
			}

			RETRO_DrawFlatPolygon(points, face->vertices, color);
		}
	} else if (rendertype == RETRO_POLY_GLENZ) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int shade = model->c + face->c;
			float lint = 0;
			if (shadertype == RETRO_SHADE_FLAT) {
				if (face->znm > 0) {
					lint = (face->rnx * LightSource.nx + face->rny * LightSource.ny + face->rnz * LightSource.nz) / (face->nn * LightSource.nn);
				} else {
					lint = (face->rnx * LightSource.nx + face->rny * LightSource.ny + face->rnz * LightSource.nz / 2) / (face->nn * LightSource.nn);
				}
				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				shade = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawGlenzPolygon(points, face->vertices, shade);
		}
	} else if (rendertype == RETRO_POLY_GOURAUD) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;

				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				float lint = (model->normal[face->normal[j]].rnx * LightSource.nx + model->normal[face->normal[j]].rny * LightSource.ny + model->normal[face->normal[j]].rnz * LightSource.nz) / (model->normal[face->normal[j]].nn * LightSource.nn);
				points[j].c = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawGouraudPolygon(points, face->vertices);
		}
	} else if (rendertype == RETRO_POLY_PHONG) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
				points[j].nx = model->normal[face->normal[j]].rnx;
				points[j].ny = model->normal[face->normal[j]].rny;
				points[j].nz = model->normal[face->normal[j]].rnz;
			}

			LightSourcePoint light;
			light.nx = LightSource.nx;
			light.ny = LightSource.ny;
			light.nz = LightSource.nz;
			light.nn = LightSource.nn;
			light.c = model->c;
			light.cintensity = model->cintensity;
			RETRO_DrawPhongPolygon(points, face->vertices, light);
		}
	} else if (rendertype == RETRO_POLY_TEXTURE) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
				points[j].u = model->uv[face->uv[j]].u;
				points[j].v = model->uv[face->uv[j]].v;
				points[j].e = model->c + model->cintensity * model->normal[face->normal[j]].rnx / model->normal[face->normal[j]].nn;
				points[j].w = model->c + model->cintensity * model->normal[face->normal[j]].rny / model->normal[face->normal[j]].nn;
			}

			int shade = model->c + face->c;
			if (shadertype == RETRO_SHADE_NONE) {
				RETRO_DrawTexMapPolygon(points, face->vertices, model->texmap);
			} if (shadertype == RETRO_SHADE_TABLE) {
				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, RETRO_Color.shadetable, shade);
			} else if (shadertype == RETRO_SHADE_FLAT) {
				float lint = 0;
				if (face->znm > 0) {
					lint = (face->rnx * LightSource.nx + face->rny * LightSource.ny + face->rnz * LightSource.nz) / (face->nn * LightSource.nn);
				} else {
					lint = (face->rnx * LightSource.nx + face->rny * LightSource.ny + face->rnz * LightSource.nz / 2) / (face->nn * LightSource.nn);
				}
				shade = CLAMP128(shade + lint * 128);

				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, RETRO_Color.shadetable, shade);
			} else if (shadertype == RETRO_SHADE_ENVIRONMENT && model->bumpmap == NULL) {
				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, RETRO_Color.shadetable);
			} else if (shadertype == RETRO_SHADE_ENVIRONMENT && model->bumpmap) {
				RETRO_DrawTexMapEnvMapBumpPolygon(points, face->vertices, model->texmap, model->envmap, model->bumpmap, RETRO_Color.shadetable);
			}
		}
	} else if (rendertype == RETRO_POLY_ENVIRONMENT) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
				points[j].u = model->uv[face->uv[j]].u;
				points[j].v = model->uv[face->uv[j]].v;
				points[j].e = model->c + model->cintensity * model->normal[face->normal[j]].rnx / model->normal[face->normal[j]].nn;
				points[j].w = model->c + model->cintensity * model->normal[face->normal[j]].rny / model->normal[face->normal[j]].nn;
			}
			if (model->bumpmap) {
				RETRO_DrawEnvMapBumpPolygon(points, face->vertices, model->envmap, model->bumpmap);
			} else {
				RETRO_DrawEnvMapPolygon(points, face->vertices, model->envmap);
			}
		}
	}
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
		model->face[i].nx = y1 * z2 - z1 * y2;
		model->face[i].ny = z1 * x2 - x1 * z2;
		model->face[i].nz = x1 * y2 - y1 * x2;

		// Calculate the length of the normal (used to scale it to a unit normal)
		model->face[i].nn = sqrt(model->face[i].nx * model->face[i].nx + model->face[i].ny * model->face[i].ny + model->face[i].nz * model->face[i].nz);
	}
}

Model3D *RETRO_Load3DModel(const char *filename, int scale = 256)
{
	Model3D *model = (Model3D *)malloc(sizeof(Model3D));
	if (model == NULL) {
		RETRO_RageQuit("Cannot allocate 3D model memory\n", "");
	}
	RETRO_3dmodel = model;

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

void RETRO_Initialize_3D(void)
{
	// Create arc cosine correction lookup
	for (int i = -128; i < 128; i++) {
		RETRO_UVlookup[i + 128] = 255.0 * asin(i / 128.0) / M_PI + 127;
	}
}

void RETRO_Deinitialize_3D(void)
{
	if (RETRO_3dmodel) free(RETRO_3dmodel);
}

#endif
