//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETRORENDER_H_
#define _RETRORENDER_H_

#include "retromodel.h"
#include "retropoly.h"
#include "retromath.h"
#include "retrocolor.h"
#include "retrogfx.h"

enum RETRO_POLY_TYPE {
	RETRO_POLY_DOT,
	RETRO_POLY_WIREFRAME,
	RETRO_POLY_HIDDENLINE,
	RETRO_POLY_FLAT,
	RETRO_POLY_GLENZ,
	RETRO_POLY_GOURAUD,
	RETRO_POLY_PHONG,
	RETRO_POLY_TEXTURE,
	RETRO_POLY_ENVIRONMENT
};

enum RETRO_POLY_SHADE {
	RETRO_SHADE_NONE,
	RETRO_SHADE_TABLE,
	RETRO_SHADE_WIREFIRE,
	RETRO_SHADE_FLAT,
	RETRO_SHADE_GOURAUD,
	RETRO_SHADE_ENVIRONMENT,
	RETRO_SHADE_PHONG
};

struct {
	Normal lightsource;
	int uvlookup[256];
} RETRO_Render;

void RETRO_InitializeLightSource(float x, float y, float z)
{
	RETRO_Render.lightsource.nx = x;
	RETRO_Render.lightsource.ny = y;
	RETRO_Render.lightsource.nz = z;

	// Calculate the length of the vector
	RETRO_Render.lightsource.nn = sqrt(x * x + y * y + z * z);

	// Rotate it once
	RETRO_RotateNormal(&RETRO_Render.lightsource, 0, 0, 0);
}

void RETRO_RenderModel(RETRO_POLY_TYPE rendertype, RETRO_POLY_SHADE shadertype = RETRO_SHADE_NONE, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();

	// Render with dots
	if (rendertype == RETRO_POLY_DOT) {
		for (int i = 0; i < model->vertices; i++) {
			RETRO_PutPixel(model->vertex[i].sx, model->vertex[i].sy, model->c);
		}
	}

	// Render with wireframe
	else if (rendertype == RETRO_POLY_WIREFRAME) {
		RETRO_SortAllFaces();

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

				if (shadertype == RETRO_SHADE_WIREFIRE) {
					RETRO_DrawFireLine(p1->x, p1->y, p2->x, p2->y, color, model->cintensity);
				} else {
					RETRO_DrawLine(p1->x, p1->y, p2->x, p2->y, color);
				}
			}
		}
	}

	// Render with hidden line
	else if (rendertype == RETRO_POLY_HIDDENLINE) {
		RETRO_SortVisibleFaces();

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

				if (shadertype == RETRO_SHADE_WIREFIRE) {
					RETRO_DrawFireLine(p1->x, p1->y, p2->x, p2->y, color, model->cintensity);
				} else {
					RETRO_DrawLine(p1->x, p1->y, p2->x, p2->y, color);
				}
			}
		}
	}

	// Render with flat shading
	else if (rendertype == RETRO_POLY_FLAT) {
		RETRO_SortVisibleFaces();

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int color = model->c + face->c;
			if (shadertype == RETRO_SHADE_FLAT) {
				float lint = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				color = CLAMP(model->c + face->c + lint * model->cintensity - model->c, cmin, cmax);
			}

			RETRO_DrawFlatPolygon(points, face->vertices, color);
		}
	}

	// Render with glenz shading
	else if (rendertype == RETRO_POLY_GLENZ) {
		RETRO_SortAllFaces();

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int shade = model->c + face->c;
			if (shadertype == RETRO_SHADE_FLAT) {
				float lint = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
				if (face->visible == false) {
					lint /= 2;
				}
				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				shade = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawGlenzPolygon(points, face->vertices, shade);
		}
	}

	// Render with gouraud shading
	else if (rendertype == RETRO_POLY_GOURAUD) {
		RETRO_SortVisibleFaces();

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[face->vertices];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;

				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				float lint = RETRO_DotProduct(model->normal[face->normal[j]], RETRO_Render.lightsource);
				points[j].c = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawGouraudPolygon(points, face->vertices);
		}
	}

	// Render with phong shading
	else if (rendertype == RETRO_POLY_PHONG) {
		RETRO_SortVisibleFaces();

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
			light.nx = RETRO_Render.lightsource.rnx;
			light.ny = RETRO_Render.lightsource.rny;
			light.nz = RETRO_Render.lightsource.rnz;
			light.nn = RETRO_Render.lightsource.nn;
			light.c = model->c;
			light.cintensity = model->cintensity;
			RETRO_DrawPhongPolygon(points, face->vertices, light);
		}
	}

	// Render with texture mapping
	else if (rendertype == RETRO_POLY_TEXTURE) {
		RETRO_SortVisibleFaces();

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

			// No shading
			if (shadertype == RETRO_SHADE_NONE) {
				RETRO_DrawTexMapPolygon(points, face->vertices, model->texmap);
			}

			// Use color from shade table
			else if (shadertype == RETRO_SHADE_TABLE) {
				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, RETRO_Color.shadetable, shade);
			}

			// Flat shading
			else if (shadertype == RETRO_SHADE_FLAT) {
				float lint = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
				shade = CLAMP128(shade + lint * 128);
				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, RETRO_Color.shadetable, shade);
			}

			// Gouraud shading
			else if (shadertype == RETRO_SHADE_GOURAUD) {
				for (int j = 0; j < face->vertices; j++) {
					float lint = RETRO_DotProduct(model->normal[face->normal[j]], RETRO_Render.lightsource);
					points[j].c = CLAMP128(model->c + face->c + lint * 128);;
				}
				RETRO_DrawTexMapGouraudPolygon(points, face->vertices, model->texmap, RETRO_Color.shadetable);
			}

			// Environment shading
			else if (shadertype == RETRO_SHADE_ENVIRONMENT && model->bumpmap == NULL) {
				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, RETRO_Color.shadetable);
			}

			// Environment shading with bump mapping
			else if (shadertype == RETRO_SHADE_ENVIRONMENT && model->bumpmap) {
				RETRO_DrawTexMapEnvMapBumpPolygon(points, face->vertices, model->texmap, model->envmap, model->bumpmap, RETRO_Color.shadetable);
			}
		}
	}

	// Render with environment mapping
	else if (rendertype == RETRO_POLY_ENVIRONMENT) {
		RETRO_SortVisibleFaces();

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

void RETRO_Initialize_3D(void)
{
	// Create arc cosine correction lookup
	for (int i = -128; i < 128; i++) {
		RETRO_Render.uvlookup[i + 128] = 255.0 * asin(i / 128.0) / M_PI + 127;
	}
}

void RETRO_Deinitialize_3D(void)
{
	if (RETRO_Model.model) free(RETRO_Model.model);
}

#endif
