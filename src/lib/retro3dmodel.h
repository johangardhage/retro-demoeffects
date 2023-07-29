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

#define RETRO_MAXVERTICES 500
#define RETRO_MAXFACES 1000
#define RETRO_MAXFACEVERTICES 5

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
	RETRO_SHADE_FLAT
} RETRO_POLY_SHADE;

struct Vertex {
	int x, y, z;			// Original coordinates
	float rx, ry, rz;		// Rotated coordinates
	float nx, ny, nz, nn;	// Normal coordinates
	float rnx, rny, rnz;	// Rotated normal coordinates
	float sx, sy;			// Screen coordinates
};

struct Face {
	int vertices;						// Number of vertices in face
	int vertex[RETRO_MAXFACEVERTICES];	// Vertices in face
	int u[RETRO_MAXFACEVERTICES];		// U coordinates
	int v[RETRO_MAXFACEVERTICES];		// V coordinates
	int c;								// Color
	float nx, ny, nz, nn;				// Normal coordinates
	float rnx, rny, rnz;				// Rotated normal coordinates
	float znm;							// Z
	int z;								// Z center, used for Quicksort
};

struct Model3D {
	int vertices;						// Number of total vertices
	int faces;							// Number of total faces
	Vertex vertex[RETRO_MAXVERTICES];	// Vertex list
	Face face[RETRO_MAXFACES];			// Face list
	int visiblefaces;					// Number of visible faces
	int visibleface[RETRO_MAXFACES];	// Visible faces
	float matrix[3][3];					// Rotation matrix
	int c;								// Base color
	int cintensity;						// Color intensity
	unsigned char* texture;				// Texture
};

struct LightSourceType {
	float x, y, z;			// Original coordinates
	float nx, ny, nz, nn;	// Rotated coordinates
};

LightSourceType LightSource;

Model3D *RETRO_3dmodel = NULL;

Model3D *RETRO_Get3DModel(void)
{
	return RETRO_3dmodel;
}

void RETRO_RenderModel(RETRO_POLY_TYPE type, RETRO_POLY_SHADE shade = RETRO_SHADE_NONE, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	if (type == RETRO_POLY_DOT) {
		for (int i = 0; i < model->vertices; i++) {
			RETRO_PutPixel(model->vertex[i].sx, model->vertex[i].sy, model->c);
		}
	} else if (type == RETRO_POLY_WIREFRAME) {
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
	} else if (type == RETRO_POLY_WIREFIRE) {
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
	} else if (type == RETRO_POLY_HIDDENLINE) {
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
	} else if (type == RETRO_POLY_FLAT) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int color = model->c + face->c;
			float lint = 0;
			if (shade == RETRO_SHADE_FLAT) {
				lint = (face->rnx * LightSource.nx + face->rny * LightSource.ny + face->rnz * LightSource.nz) / (face->nn * LightSource.nn);
				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				color = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawFlatPolygon(points, face->vertices, color);
		}
	} else if (type == RETRO_POLY_GLENZ) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int color = model->c + face->c;
			float lint = 0;
			if (shade == RETRO_SHADE_FLAT) {
				if (face->znm > 0) {
					lint = (face->rnx * LightSource.nx + face->rny * LightSource.ny + face->rnz * LightSource.nz) / (face->nn * LightSource.nn);
				} else {
					lint = (face->rnx * LightSource.nx + face->rny * LightSource.ny + face->rnz * LightSource.nz / 2) / (face->nn * LightSource.nn);
				}
				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				color = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawGlenzPolygon(points, face->vertices, color);
		}
	} else if (type == RETRO_POLY_GOURAUD) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;

				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				float lint = (model->vertex[face->vertex[j]].rnx * LightSource.nx + model->vertex[face->vertex[j]].rny * LightSource.ny + model->vertex[face->vertex[j]].rnz * LightSource.nz) / (model->vertex[face->vertex[j]].nn * LightSource.nn);
				points[j].c = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawGouraudPolygon(points, face->vertices);
		}
	} else if (type == RETRO_POLY_PHONG) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
				points[j].nx = model->vertex[face->vertex[j]].rnx;
				points[j].ny = model->vertex[face->vertex[j]].rny;
				points[j].nz = model->vertex[face->vertex[j]].rnz;
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
	} else if (type == RETRO_POLY_TEXTURE) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
				points[j].u = face->u[j];
				points[j].v = face->v[j];
			}

			RETRO_DrawTexturePolygon(points, face->vertices, model->texture);
		}
	} else if (type == RETRO_POLY_ENVIRONMENT) {
		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
				points[j].u = model->c + model->cintensity * model->vertex[face->vertex[j]].rnx / model->vertex[face->vertex[j]].nn;
				points[j].v = model->c + model->cintensity * model->vertex[face->vertex[j]].rny / model->vertex[face->vertex[j]].nn;
			}

			RETRO_DrawTexturePolygon(points, face->vertices, model->texture);
		}
	}
}

Model3D *RETRO_CreateCube3Model()
{
	Model3D *model = new Model3D();
	RETRO_3dmodel = model;

	model->vertices = 8;
	model->faces = 12;

	int vertex[model->vertices][3] = {{100, 100, 100}, {100, 100, -100}, {100, -100, 100}, {100, -100, -100}, {-100, 100, 100}, {-100, 100, -100}, {-100, -100, 100}, {-100, -100, -100}};
	int face[model->faces][3] = {{5, 1, 3}, {7, 5, 3}, {1, 0, 2}, {3, 1, 2}, {0, 4, 6}, {2, 0, 6}, {4, 5, 7}, {6, 4, 7}, {4, 0, 1}, {5, 4, 1}, {7, 3, 2}, {6, 7, 2}};
	int faceu[model->faces][3] = {{0, 127, 127}, {0, 0, 127}, {0, 127, 127}, {0, 0, 127}, {0, 127, 127}, {0, 0, 127}, {0, 127, 127}, {0, 0, 127}, {0, 127, 127}, {0, 0, 127}, {0, 127, 127}, {0, 0, 127}};
	int facev[model->faces][3] = {{0, 0, 127}, {127, 0, 127}, {0, 0, 127}, {127, 0, 127}, {0, 0, 127}, {127, 0, 127}, {0, 0, 127}, {127, 0, 127}, {0, 0, 127}, {127, 0, 127}, {0, 0, 127}, {127, 0, 127}};

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].x = vertex[i][0];
		model->vertex[i].y = vertex[i][1];
		model->vertex[i].z = vertex[i][2];
	}

	for (int i = 0; i < model->faces; i++) {
		model->face[i].vertices = 3;
		model->face[i].vertex[0] = face[i][0];
		model->face[i].vertex[1] = face[i][1];
		model->face[i].vertex[2] = face[i][2];
		model->face[i].u[0] = faceu[i][0];
		model->face[i].u[1] = faceu[i][1];
		model->face[i].u[2] = faceu[i][2];
		model->face[i].v[0] = facev[i][0];
		model->face[i].v[1] = facev[i][1];
		model->face[i].v[2] = facev[i][2];
	}

	return model;
}

Model3D *RETRO_CreateCube4Model()
{
	Model3D *model = new Model3D();
	RETRO_3dmodel = model;

	model->vertices = 8;
	model->faces = 6;

	int vertex[model->vertices][3] = {{100, 100, 100}, {100, 100, -100}, {100, -100, 100}, {100, -100, -100}, {-100, 100, 100}, {-100, 100, -100}, {-100, -100, 100}, {-100, -100, -100}};
	int face[model->faces][4] = {{4, 0, 1, 5}, {1, 0, 2, 3}, {5, 1, 3, 7}, {4, 5, 7, 6}, {0, 4, 6, 2}, {3, 2, 6, 7}};

	for (int i = 0; i < model->vertices; i++) {
		model->vertex[i].x = vertex[i][0];
		model->vertex[i].y = vertex[i][1];
		model->vertex[i].z = vertex[i][2];
	}

	for (int i = 0; i < model->faces; i++) {
		model->face[i].vertices = 4;
		model->face[i].vertex[0] = face[i][0];
		model->face[i].vertex[1] = face[i][1];
		model->face[i].vertex[2] = face[i][2];
		model->face[i].vertex[3] = face[i][3];
	}

	return model;
}

Model3D *RETRO_Load3DModel(const char *filename)
{
	Model3D *model = new Model3D();
	RETRO_3dmodel = model;

	FILE *fp = fopen(filename, "rt");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	int offset = 0;
	char chain[200];
	while (fgets(chain, 100, fp)) {
		if (!strncmp(chain, "Vertex", 6)) {
			if (strncmp(chain, "Vertex list", 11)) {
				float x, y, z;
				int i = 0;

				while (chain[i] != 'X') {
					i++;
				}
				i += 2;
				while (chain[i] == ' ') {
					i++;
				}
				sscanf(chain + i, "%f", &x);

				while (chain[i] != 'Y') {
					i++;
				}
				i += 2;
				while (chain[i] == ' ') {
					i++;
				}
				sscanf(chain + i, "%f", &y);

				while (chain[i] != 'Z') {
					i++;
				}
				i += 2;
				while (chain[i] == ' ') {
					i++;
				}
				sscanf(chain + i, "%f", &z);

				model->vertex[model->vertices].x = x / 16; // Shrink a bit to allow fixed math
				model->vertex[model->vertices].y = y / 16; // Shrink a bit to allow fixed math
				model->vertex[model->vertices].z = z / 16; // Shrink a bit to allow fixed math
				model->vertices++;
			}
		} else if (!strncmp(chain, "Face", 4)) {
			if (strncmp(chain, "Face list", 9)) {
				char vertex[50];
				int i = 0;
				int j = 0;

				while (chain[i] != 'A') {
					i++;
				}
				i += 2;
				j = i;
				while (chain[j] != ' ') {
					j++;
				}
				strncpy(vertex, chain + i, j - i);
				vertex[j - i] = '\0';
				model->face[model->faces].vertex[0] = atoi(vertex) + offset;

				while (chain[i] != 'B') {
					i++;
				}
				i += 2;
				j = i;
				while (chain[j] != ' ') {
					j++;
				}
				strncpy(vertex, chain + i, j - i);
				vertex[j - i] = '\0';
				model->face[model->faces].vertex[1] = atoi(vertex) + offset;

				while (chain[i] != 'C') {
					i++;
				}
				i += 2;
				j = i;
				while (chain[j] != ' ') {
					j++;
				}
				strncpy(vertex, chain + i, j - i);
				vertex[j - i] = '\0';
				model->face[model->faces].vertex[2] = atoi(vertex) + offset;

				model->face[model->faces].vertices = 3;
				model->faces++;
			}
		} else if (!strncmp(chain, "Named object", 12)) {
			offset = model->vertices;
		}
	}

	fclose(fp);

	return model;
}

Model3D *RETRO_LoadV10Model(const char *filename)
{
	Model3D *model = new Model3D();
	RETRO_3dmodel = model;

	FILE *fp = fopen(filename, "rt");
	if (fp == NULL) {
		RETRO_RageQuit("Cannot open file: %s\n", filename);
	}

	short int temp;

	fread(&temp, sizeof(short int), 1, fp);
	model->vertices = temp;

	for (int i = 0; i < model->vertices; i++) {
		fread(&temp, sizeof(short int), 1, fp);
		model->vertex[i].x = temp;
		fread(&temp, sizeof(short int), 1, fp);
		model->vertex[i].y = temp;
		fread(&temp, sizeof(short int), 1, fp);
		model->vertex[i].z = temp;

		model->vertex[i].x /= 2;
		model->vertex[i].y /= 2;
		model->vertex[i].z /= -2;
	}

	fread(&temp, sizeof(short int), 1, fp);
	model->faces = temp;

	for (int i = 0; i < model->faces; i++) {
		fread(&temp, sizeof(short int), 1, fp);
		model->face[i].vertices = temp;

		for (int j = 0; j < model->face[i].vertices; j++) {
			fread(&temp, sizeof(short int), 1, fp);
			model->face[i].vertex[j] = temp;
		}

		fread(&temp, sizeof(short int), 1, fp);
		model->face[i].c = temp;
	}

	fclose(fp);

	return model;
}

void RETRO_TriangularizeModel(Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	Face newface[RETRO_MAXFACES];
	int newfaces = 0;

	printf("Faces: %d\n", model->faces);
	for (int i = 0; i < model->faces; i++) {
		printf("face%d: ", i);
		for (int j = 0; j < model->face[i].vertices; j++) {
			printf("%d ", model->face[i].vertex[j]);
		}
		printf("\n");
	}

	for (int i = 0; i < model->faces; i++) {
		Face *face = &model->face[i];

		// Loop through all vertices in polygon and create new faces
		for (int j = 0; j < face->vertices - 2; j++) {
			newface[newfaces].vertices = 3;
			newface[newfaces].vertex[0] = face->vertex[0];
			newface[newfaces].vertex[1] = face->vertex[j + 1];
			newface[newfaces].vertex[2] = face->vertex[j + 2];
			newface[newfaces].u[0] = face->u[0];
			newface[newfaces].u[1] = face->u[j + 1];
			newface[newfaces].u[2] = face->u[j + 2];
			newface[newfaces].v[0] = face->v[0];
			newface[newfaces].v[1] = face->v[j + 1];
			newface[newfaces].v[2] = face->v[j + 2];
			newface[newfaces].c = face->c;
			newfaces++;
		}
	}

	model->faces = newfaces;
	for (int i = 0; i < model->faces; i++) {
		model->face[i] = newface[i];
	}

	printf("New faces: %d\n", model->faces);
	for (int i = 0; i < model->faces; i++) {
		printf("face%d: %d %d %d\n", i, model->face[i].vertex[0], model->face[i].vertex[1], model->face[i].vertex[2]);
	}
}

void RETRO_Deinitialize_3D()
{
	if (RETRO_3dmodel) delete RETRO_3dmodel;
}

#endif
