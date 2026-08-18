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

void RETRO_RenderDotModel(Model3D *model)
{
	for (int i = 0; i < model->vertices; i++) {
		if (model->vertex[i].q > 0.0f) {
			RETRO_PutPixel(model->vertex[i].sx, model->vertex[i].sy, model->c);
		}
	}
}

void RETRO_RenderWireModel(Model3D *model, bool hiddenlines, bool fire)
{
	if (hiddenlines) {
		RETRO_SortVisibleFaces(model);
	} else {
		RETRO_SortAllFaces(model);
	}

	for (int i = 0; i < model->visiblefaces; i++) {
		Face *face = &model->face[model->visibleface[i]];
		int color = model->c + face->c;

		for (int j = 0; j < face->vertices; j++) {
			Vertex *p1 = &model->vertex[face->vertex[j]];
			Vertex *p2 = &model->vertex[face->vertex[(j + 1) % face->vertices]];
			if (fire) {
				RETRO_DrawFireLine(p1->sx, p1->sy, p2->sx, p2->sy, color, model->cintensity);
			} else {
				RETRO_DrawLine(p1->sx, p1->sy, p2->sx, p2->sy, color);
			}
		}
	}
}

void RETRO_RenderFlatModel(Model3D *model, bool shaded)
{
	RETRO_SortVisibleFaces(model);

	for (int i = 0; i < model->visiblefaces; i++) {
		Face *face = &model->face[model->visibleface[i]];
		PolygonPoint points[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			points[j].x = model->vertex[face->vertex[j]].sx;
			points[j].y = model->vertex[face->vertex[j]].sy;
			points[j].q = model->vertex[face->vertex[j]].q;
		}

		int color = model->c + face->c;
		if (shaded) {
			// One lambert per face: color = c + intensity * ShadeFromLambert(N · L).
			float lint = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
			float shade = RETRO_ShadeFromLambert(lint);
			int cmin = model->c;
			int cmax = model->c + face->c + model->cintensity;
			color = CLAMP(model->c + face->c + shade * model->cintensity, cmin, cmax);
		}
		RETRO_DrawFlatPolygon(points, face->vertices, color);
	}
}

void RETRO_RenderGlenzModel(Model3D *model, RETRO_POLY_SHADE shadertype)
{
	RETRO_SortAllFaces(model);

	for (int i = 0; i < model->visiblefaces; i++) {
		Face *face = &model->face[model->visibleface[i]];
		PolygonPoint points[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			points[j].x = model->vertex[face->vertex[j]].sx;
			points[j].y = model->vertex[face->vertex[j]].sy;
		}
		int shade;
		if (shadertype == RETRO_SHADE_FLAT) {
			// Glenz shades into a gradient, whose brightness rises linearly with
			// the color index, so the lambert term is already the shade to use.
			// No RETRO_ShadeFromLambert here: that only undoes the angle spacing
			// of a phong ramp, and applying it to a gradient would bend a
			// correct falloff out of shape
			float lint = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
			if (face->visible == false) lint /= 2;
			int cmin = model->c;
			int cmax = model->c + face->c + model->cintensity;
			shade = CLAMP(model->c + face->c + lint * model->cintensity, cmin, cmax);
		} else {
			// Unshaded Glenz draws both sides of every face. The winding test stored
			// in visible selects the front or back palette contribution, and zero
			// makes that side fully transparent. Each contribution is added to the
			// framebuffer by RETRO_DrawGlenzPolygon.
			int color = face->visible ? face->c : face->backc;
			if (color == 0) continue;
			shade = model->c + color;
		}
		RETRO_DrawGlenzPolygon(points, face->vertices, shade);
	}
}

void RETRO_RenderGouraudModel(Model3D *model)
{
	RETRO_SortVisibleFaces(model);

	for (int i = 0; i < model->visiblefaces; i++) {
		Face *face = &model->face[model->visibleface[i]];
		int cmin = model->c;
		int cmax = model->c + face->c + model->cintensity;
		PolygonPoint points[RETRO_MAX_FACEVERTICES];

		for (int j = 0; j < face->vertices; j++) {
			points[j].x = model->vertex[face->vertex[j]].sx;
			points[j].y = model->vertex[face->vertex[j]].sy;
			points[j].q = model->vertex[face->vertex[j]].q;
			float lint = RETRO_DotProduct(model->normal[face->normal[j]], RETRO_Render.lightsource);
			points[j].c = CLAMP(model->c + face->c + RETRO_ShadeFromLambert(lint) * model->cintensity, cmin, cmax);
		}
		RETRO_DrawGouraudPolygon(points, face->vertices);
	}
}

void RETRO_RenderPhongModel(Model3D *model)
{
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
			points[j].q = vertex->q;
			// n * q; interpolating and renormalising is the same direction as /q.
			points[j].nx = model->normal[face->normal[j]].rnx * vertex->q;
			points[j].ny = model->normal[face->normal[j]].rny * vertex->q;
			points[j].nz = model->normal[face->normal[j]].rnz * vertex->q;
		}
		RETRO_DrawPhongPolygon(points, face->vertices, light);
	}
}

void RETRO_RenderTextureModel(Model3D *model, RETRO_POLY_SHADE shadertype)
{
	RETRO_SortVisibleFaces(model);
	unsigned char *shadetable = model->shadetable;
	bool lightingmap = shadertype == RETRO_SHADE_PHONG;
	bool envmapshading = shadertype == RETRO_SHADE_ENVIRONMENT || lightingmap;
	bool bumpmapping = model->bumpmap != NULL;

	// A bump is lit by the dot product of a tilted normal with the light, so the
	// light is needed as a direction rather than as the shade it lands on
	float inverselightlength = RETRO_Render.lightsource.nn > 0.0f ? 1.0f / RETRO_Render.lightsource.nn : 0.0f;
	float lightx = RETRO_Render.lightsource.rnx * inverselightlength;
	float lighty = RETRO_Render.lightsource.rny * inverselightlength;
	float lightz = RETRO_Render.lightsource.rnz * inverselightlength;

	for (int i = 0; i < model->visiblefaces; i++) {
		Face *face = &model->face[model->visibleface[i]];
		// Rotated with the model, since the bump tilts along the surface's u and v
		TangentFrame frame = { face->tangent.rnx, face->tangent.rny, face->tangent.rnz,
							   face->bitangent.rnx, face->bitangent.rny, face->bitangent.rnz };
		PolygonPoint points[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			points[j].x = vertex->sx;
			points[j].y = vertex->sy;
			points[j].q = vertex->q;
			points[j].u = model->uv[face->uv[j]].u;
			points[j].v = model->uv[face->uv[j]].v;
			if (envmapshading) {
				Normal *normal = &model->normal[face->normal[j]];
				// A lighting map interpolates n*q (perspective-correct). A
				// reflection map interpolates the unit normal, since the lookup
				// is of direction, not of a quantity that varies with depth.
				float normalscale = lightingmap ? vertex->q : (normal->nn > 0.0f ? 1.0f / normal->nn : 0.0f);
				points[j].nx = normal->rnx * normalscale;
				points[j].ny = normal->rny * normalscale;
				points[j].nz = normal->rnz * normalscale;
			}
		}
		if (shadertype == RETRO_SHADE_NONE) {
			RETRO_DrawTexMapPolygon(points, face->vertices, model->texmap, model->texmapwidth, model->texmapheight);
		} else if (shadertype == RETRO_SHADE_TABLE) {
			// Texture mapped through the shade table at a fixed light level, with
			// no light source involved. face->c offsets it per face, so a model
			// can carry its own baked lighting
			int shade = model->c + face->c;
			if (bumpmapping) {
				// Same drawer as flat+bump: one shade and one face normal. The
				// tilt only moves the baked shade, so a flat patch stays at that
				// level.
				float inversefacenormallength = face->facenormal.nn > 0.0f ? 1.0f / face->facenormal.nn : 0.0f;
				for (int j = 0; j < face->vertices; j++) {
					points[j].c = shade;
					points[j].nx = face->facenormal.rnx * inversefacenormallength;
					points[j].ny = face->facenormal.rny * inversefacenormallength;
					points[j].nz = face->facenormal.rnz * inversefacenormallength;
				}
				RETRO_DrawTexMapBumpPolygon(points, face->vertices, model->texmap, model->bumpmap, model->bumpheight, shadetable, lightx, lighty, lightz, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
			} else {
				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, shadetable, shade, false, model->envmapintensity, model->envmapwidth, model->envmapheight, model->texmapwidth, model->texmapheight);
			}
		} else if (shadertype == RETRO_SHADE_FLAT) {
			int shade = model->c + face->c;
			float lint = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
			shade = CLAMP128(shade + RETRO_ShadeFromLambert(lint) * RETRO_SHADES);
			if (bumpmapping) {
				// A flat shaded face carries one shade and one normal over all of
				// it, which the bump mapper draws as every vertex holding both
				float inversefacenormallength = face->facenormal.nn > 0.0f ? 1.0f / face->facenormal.nn : 0.0f;
				for (int j = 0; j < face->vertices; j++) {
					points[j].c = shade;
					points[j].nx = face->facenormal.rnx * inversefacenormallength;
					points[j].ny = face->facenormal.rny * inversefacenormallength;
					points[j].nz = face->facenormal.rnz * inversefacenormallength;
				}
				RETRO_DrawTexMapBumpPolygon(points, face->vertices, model->texmap, model->bumpmap, model->bumpheight, shadetable, lightx, lighty, lightz, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
			} else {
				RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, shadetable, shade, false, model->envmapintensity, model->envmapwidth, model->envmapheight, model->texmapwidth, model->texmapheight);
			}
		} else if (shadertype == RETRO_SHADE_GOURAUD) {
			for (int j = 0; j < face->vertices; j++) {
				Normal *normal = &model->normal[face->normal[j]];
				float lint = RETRO_DotProduct(*normal, RETRO_Render.lightsource);
				points[j].c = CLAMP128(model->c + face->c + RETRO_ShadeFromLambert(lint) * RETRO_SHADES);
				if (bumpmapping) {
					float inversenormallength = normal->nn > 0.0f ? 1.0f / normal->nn : 0.0f;
					points[j].nx = normal->rnx * inversenormallength;
					points[j].ny = normal->rny * inversenormallength;
					points[j].nz = normal->rnz * inversenormallength;
				}
			}
			if (bumpmapping) {
				RETRO_DrawTexMapBumpPolygon(points, face->vertices, model->texmap, model->bumpmap, model->bumpheight, shadetable, lightx, lighty, lightz, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
			} else {
				RETRO_DrawTexMapGouraudPolygon(points, face->vertices, model->texmap, model->texmapwidth, model->texmapheight, shadetable);
			}
		} else if (envmapshading && !bumpmapping) {
			RETRO_DrawTexMapEnvMapPolygon(points, face->vertices, model->texmap, model->envmap, shadetable, 0, lightingmap, model->envmapintensity, model->envmapwidth, model->envmapheight, model->texmapwidth, model->texmapheight);
		} else if (envmapshading) {
			RETRO_DrawTexMapEnvMapBumpPolygon(points, face->vertices, model->texmap, model->envmap, model->bumpmap, model->bumpheight, shadetable, lightingmap, frame, model->envmapintensity, model->envmapwidth, model->envmapheight, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
		}
	}
}

void RETRO_RenderEnvironmentModel(Model3D *model, RETRO_POLY_SHADE shadertype)
{
	RETRO_SortVisibleFaces(model);
	bool lightingmap = shadertype == RETRO_SHADE_PHONG;
	bool bumpmapping = model->bumpmap != NULL;

	for (int i = 0; i < model->visiblefaces; i++) {
		Face *face = &model->face[model->visibleface[i]];
		TangentFrame frame = { face->tangent.rnx, face->tangent.rny, face->tangent.rnz,
							   face->bitangent.rnx, face->bitangent.rny, face->bitangent.rnz };
		PolygonPoint points[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			Normal *normal = &model->normal[face->normal[j]];
			points[j].x = vertex->sx;
			points[j].y = vertex->sy;
			points[j].q = vertex->q;
			if (bumpmapping) {
				points[j].u = model->uv[face->uv[j]].u;
				points[j].v = model->uv[face->uv[j]].v;
			}
			float normalscale = lightingmap ? vertex->q : (normal->nn > 0.0f ? 1.0f / normal->nn : 0.0f);
			points[j].nx = normal->rnx * normalscale;
			points[j].ny = normal->rny * normalscale;
			points[j].nz = normal->rnz * normalscale;
		}
		if (bumpmapping) {
			RETRO_DrawEnvMapBumpPolygon(points, face->vertices, model->envmap, model->bumpmap, model->bumpheight, lightingmap, frame, model->envmapintensity, model->envmapwidth, model->envmapheight, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
		} else {
			RETRO_DrawEnvMapPolygon(points, face->vertices, model->envmap, lightingmap, model->envmapintensity, model->envmapwidth, model->envmapheight);
		}
	}
}

void RETRO_RenderModel(RETRO_POLY_TYPE rendertype, RETRO_POLY_SHADE shadertype = RETRO_SHADE_NONE, Model3D *model = NULL)
{
	model = model ? model : RETRO_Get3DModel();
	if (model == NULL) return;

	// One model, one depth range. Glenz is exempt: it adds palette indices, so
	// it depends on the order the sort gives it.
	RETRO_ClearDepthBuffer();

	switch (rendertype) {
	case RETRO_POLY_DOT:
		RETRO_RenderDotModel(model);
		break;
	case RETRO_POLY_WIREFRAME:
		RETRO_RenderWireModel(model, false, shadertype == RETRO_SHADE_WIREFIRE);
		break;
	case RETRO_POLY_HIDDENLINE:
		RETRO_RenderWireModel(model, true, shadertype == RETRO_SHADE_WIREFIRE);
		break;
	case RETRO_POLY_FLAT:
		RETRO_RenderFlatModel(model, shadertype == RETRO_SHADE_FLAT);
		break;
	case RETRO_POLY_GLENZ:
		RETRO_RenderGlenzModel(model, shadertype);
		break;
	case RETRO_POLY_GOURAUD:
		RETRO_RenderGouraudModel(model);
		break;
	case RETRO_POLY_PHONG:
		RETRO_RenderPhongModel(model);
		break;
	case RETRO_POLY_TEXTURE:
		RETRO_RenderTextureModel(model, shadertype);
		break;
	case RETRO_POLY_ENVIRONMENT:
		RETRO_RenderEnvironmentModel(model, shadertype);
		break;
	}
}

void RETRO_Deinitialize_3D(void)
{
	if (RETRO_Model.model) free(RETRO_Model.model);
}

#endif
