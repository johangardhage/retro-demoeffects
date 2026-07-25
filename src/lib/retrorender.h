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

void RETRO_PrepareMappedFace(Model3D *model, Face *face, PolygonPoint *points)
{
	for (int i = 0; i < face->vertices; i++) {
		Vertex *vertex = &model->vertex[face->vertex[i]];
		Normal *normal = &model->normal[face->normal[i]];
		float inverseNormalLength = normal->nn > 0.0f ? 1.0f / normal->nn : 0.0f;

		points[i].x = vertex->sx;
		points[i].y = vertex->sy;
		points[i].q = vertex->q;
		points[i].u = model->uv[face->uv[i]].u;
		points[i].v = model->uv[face->uv[i]].v;
		points[i].e = model->c + model->cintensity * normal->rnx * inverseNormalLength;
		points[i].w = model->c + model->cintensity * normal->rny * inverseNormalLength;
	}
}

void RETRO_RenderModel(RETRO_POLY_TYPE rendertype, RETRO_POLY_SHADE shadertype = RETRO_SHADE_NONE, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();
	if (model == NULL) return;

	// Render with dots
	if (rendertype == RETRO_POLY_DOT) {
		for (int i = 0; i < model->vertices; i++) {
			if (model->vertex[i].q > 0.0f) {
				RETRO_PutPixel(model->vertex[i].sx, model->vertex[i].sy, model->c);
			}
		}
	}

	// Render with wireframe or hidden lines
	else if (rendertype == RETRO_POLY_WIREFRAME || rendertype == RETRO_POLY_HIDDENLINE) {
		if (rendertype == RETRO_POLY_WIREFRAME) {
			RETRO_SortAllFaces(model);
		} else {
			RETRO_SortVisibleFaces(model);
		}

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			int color = model->c + face->c;
			for (int j = 0; j < face->vertices; j++) {
				Vertex *p1 = &model->vertex[face->vertex[j]];
				Vertex *p2 = &model->vertex[face->vertex[(j + 1) % face->vertices]];

				if (shadertype == RETRO_SHADE_WIREFIRE) {
					RETRO_DrawFireLine(p1->sx, p1->sy, p2->sx, p2->sy, color, model->cintensity);
				} else {
					RETRO_DrawLine(p1->sx, p1->sy, p2->sx, p2->sy, color);
				}
			}
		}
	}

	// Render with flat shading
	else if (rendertype == RETRO_POLY_FLAT) {
		RETRO_SortVisibleFaces(model);

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[RETRO_MAX_FACEVERTICES];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;
			}

			int color = model->c + face->c;
			if (shadertype == RETRO_SHADE_FLAT) {
				float lint = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
				int cmin = model->c;
				int cmax = model->c + face->c + model->cintensity;
				color = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawFlatPolygon(points, face->vertices, color);
		}
	}

	// Render with glenz shading
	else if (rendertype == RETRO_POLY_GLENZ) {
		RETRO_SortAllFaces(model);

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[RETRO_MAX_FACEVERTICES];
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
		RETRO_SortVisibleFaces(model);

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];
			int cmin = model->c;
			int cmax = model->c + face->c + model->cintensity;

			PolygonPoint points[RETRO_MAX_FACEVERTICES];
			for (int j = 0; j < face->vertices; j++) {
				points[j].x = model->vertex[face->vertex[j]].sx;
				points[j].y = model->vertex[face->vertex[j]].sy;

				float lint = RETRO_DotProduct(model->normal[face->normal[j]], RETRO_Render.lightsource);
				points[j].c = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
			}

			RETRO_DrawGouraudPolygon(points, face->vertices);
		}
	}

	// Render with phong shading
	else if (rendertype == RETRO_POLY_PHONG) {
		RETRO_SortVisibleFaces(model);

		LightSourcePoint light;
		light.nx = RETRO_Render.lightsource.rnx;
		light.ny = RETRO_Render.lightsource.rny;
		light.nz = RETRO_Render.lightsource.rnz;
		light.nn = RETRO_Render.lightsource.nn;
		light.c = model->c;
		light.cintensity = model->cintensity;

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[RETRO_MAX_FACEVERTICES];
			for (int j = 0; j < face->vertices; j++) {
				Vertex *vertex = &model->vertex[face->vertex[j]];
				points[j].x = vertex->sx;
				points[j].y = vertex->sy;
				points[j].nx = model->normal[face->normal[j]].rnx * vertex->q;
				points[j].ny = model->normal[face->normal[j]].rny * vertex->q;
				points[j].nz = model->normal[face->normal[j]].rnz * vertex->q;
			}

			RETRO_DrawPhongPolygon(points, face->vertices, light);
		}
	}

	// Render with texture mapping
	else if (rendertype == RETRO_POLY_TEXTURE) {
		RETRO_SortVisibleFaces(model);

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[RETRO_MAX_FACEVERTICES];
			RETRO_PrepareMappedFace(model, face, points);

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
					points[j].c = CLAMP128(model->c + face->c + lint * 128);
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
		RETRO_SortVisibleFaces(model);

		for (int i = 0; i < model->visiblefaces; i++) {
			Face *face = &model->face[model->visibleface[i]];

			PolygonPoint points[RETRO_MAX_FACEVERTICES];
			RETRO_PrepareMappedFace(model, face, points);
			if (model->bumpmap) {
				RETRO_DrawEnvMapBumpPolygon(points, face->vertices, model->envmap, model->bumpmap);
			} else {
				RETRO_DrawEnvMapPolygon(points, face->vertices, model->envmap);
			}
		}
	}
}

void RETRO_Deinitialize_3D(void)
{
	if (RETRO_Model.model) free(RETRO_Model.model);
}

#endif
