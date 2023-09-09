//
// Retro graphics library
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETRORENDER_H_
#define _RETRORENDER_H_

#include "retropoly.h"
#include "retrogfx.h"
#include "retrocolor.h"
#include "retro3dmodel.h"

int RETRO_UVlookup[256];

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
				lint = (face->rnx * RETRO_lightsource.rnx + face->rny * RETRO_lightsource.rny + face->rnz * RETRO_lightsource.rnz) / (face->nn * RETRO_lightsource.nn);
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
					lint = (face->rnx * RETRO_lightsource.rnx + face->rny * RETRO_lightsource.rny + face->rnz * RETRO_lightsource.rnz) / (face->nn * RETRO_lightsource.nn);
				} else {
					lint = (face->rnx * RETRO_lightsource.rnx + face->rny * RETRO_lightsource.rny + face->rnz * RETRO_lightsource.rnz / 2) / (face->nn * RETRO_lightsource.nn);
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
				float lint = (model->normal[face->normal[j]].rnx * RETRO_lightsource.rnx +
							  model->normal[face->normal[j]].rny * RETRO_lightsource.rny +
							  model->normal[face->normal[j]].rnz * RETRO_lightsource.rnz) / (model->normal[face->normal[j]].nn * RETRO_lightsource.nn);
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
			light.nx = RETRO_lightsource.rnx;
			light.ny = RETRO_lightsource.rny;
			light.nz = RETRO_lightsource.rnz;
			light.nn = RETRO_lightsource.nn;
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
				float lint = (face->rnx * RETRO_lightsource.rnx + face->rny * RETRO_lightsource.rny + face->rnz * RETRO_lightsource.rnz) / (face->nn * RETRO_lightsource.nn);
				shade = CLAMP128(shade + lint * 128);
				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, RETRO_Color.shadetable, shade);
			} else if (shadertype == RETRO_SHADE_GOURAUD) {
				for (int j = 0; j < face->vertices; j++) {
					float lint = (model->normal[face->normal[j]].rnx * RETRO_lightsource.rnx +
								  model->normal[face->normal[j]].rny * RETRO_lightsource.rny +
								  model->normal[face->normal[j]].rnz * RETRO_lightsource.rnz) / (model->normal[face->normal[j]].nn * RETRO_lightsource.nn);
					points[j].c = CLAMP128(model->c + face->c + lint * 128);;
				}
				RETRO_DrawTexMapGouraudPolygon(points, face->vertices, model->texmap, RETRO_Color.shadetable);
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
