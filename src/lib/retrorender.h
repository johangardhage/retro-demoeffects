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
	Direction lightsource;
} RETRO_Render;

// Where a surface must face to catch the light, given at whatever scale is
// convenient and stored unit, like every other Direction. Only the direction is
// held so far, so the source has no position yet: it can be pointed, not moved.
void RETRO_InitializeLightSource(float x, float y, float z)
{
	float length = sqrt(x * x + y * y + z * z);
	float inverselength = length > 0.0f ? 1.0f / length : 0.0f;

	RETRO_Render.lightsource.x = x * inverselength;
	RETRO_Render.lightsource.y = y * inverselength;
	RETRO_Render.lightsource.z = z * inverselength;

	// Rotate it once
	RETRO_RotateDirection(&RETRO_Render.lightsource, 0, 0, 0);
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
	// Hidden lines means only the front faces are drawn; without it the back
	// ones are drawn too, so they go into the list as well.
	RETRO_SortFaces(model, !hiddenlines);

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		int color = model->c + face->c;

		for (int j = 0; j < face->vertices; j++) {
			Vertex *p1 = &model->vertex[face->vertex[j]];
			Vertex *p2 = &model->vertex[face->vertex[(j + 1) % face->vertices]];
			if (fire) {
				RETRO_DrawFireLine(p1->sx, p1->sy, p2->sx, p2->sy, color, model->shades);
			} else {
				RETRO_DrawLine(p1->sx, p1->sy, p2->sx, p2->sy, color);
			}
		}
	}
}

void RETRO_RenderFlatModel(Model3D *model, bool shaded)
{
	RETRO_SortFaces(model);

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			point[j].x = model->vertex[face->vertex[j]].sx;
			point[j].y = model->vertex[face->vertex[j]].sy;
			point[j].q = model->vertex[face->vertex[j]].q;
		}

		int color = model->c + face->c;
		if (shaded) {
			// One lambert per face: color = c + face.c + ShadeFromLambert(N · L) * shades.
			float lambert = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
			int cstart = model->c;
			int cend = model->c + face->c + model->shades;
			color = CLAMP(model->c + face->c + RETRO_ShadeFromLambert(lambert) * model->shades, cstart, cend);
		}
		RETRO_DrawFlatPolygon(point, face->vertices, color);
	}
}

void RETRO_RenderGlenzModel(Model3D *model, RETRO_POLY_SHADE shadertype)
{
	RETRO_SortFaces(model, true);

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			point[j].x = model->vertex[face->vertex[j]].sx;
			point[j].y = model->vertex[face->vertex[j]].sy;
		}
		int color;
		if (shadertype == RETRO_SHADE_FLAT) {
			// Glenz shades into a gradient, whose brightness rises linearly with
			// the color index, so the lambert term is already the shade to use.
			// No RETRO_ShadeFromLambert here: that only undoes the angle spacing
			// of a phong ramp, and applying it to a gradient would bend a
			// correct falloff out of shape
			float lambert = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
			if (face->frontfacing == false) lambert /= 2;
			// The floor is the model's base, not the face's. A back face carries a
			// negative lambert and is meant to run down out of its own ramp into
			// the black beneath it - half the faces drawn here do - so raising it
			// to model->c + face->c would flatten every one of them to that entry.
			int cstart = model->c;
			int cend = model->c + face->c + model->shades;
			color = CLAMP(model->c + face->c + lambert * model->shades, cstart, cend);
		} else {
			// Unshaded Glenz draws both sides of every face. The winding test stored
			// in frontfacing selects the front or back palette contribution, and zero
			// makes that side fully transparent. Each contribution is added to the
			// framebuffer by RETRO_DrawGlenzPolygon.
			int offset = face->frontfacing ? face->c : face->backc;
			if (offset == 0) continue;
			color = model->c + offset;
		}
		RETRO_DrawGlenzPolygon(point, face->vertices, color);
	}
}

void RETRO_RenderGouraudModel(Model3D *model)
{
	RETRO_SortFaces(model);

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		int cstart = model->c;
		int cend = model->c + face->c + model->shades;
		PolygonPoint point[RETRO_MAX_FACEVERTICES];

		for (int j = 0; j < face->vertices; j++) {
			point[j].x = model->vertex[face->vertex[j]].sx;
			point[j].y = model->vertex[face->vertex[j]].sy;
			point[j].q = model->vertex[face->vertex[j]].q;
			float lambert = RETRO_DotProduct(model->normal[face->vertexnormal[j]], RETRO_Render.lightsource);
			point[j].c = CLAMP(model->c + face->c + RETRO_ShadeFromLambert(lambert) * model->shades, cstart, cend);
		}
		RETRO_DrawGouraudPolygon(point, face->vertices);
	}
}

void RETRO_RenderPhongModel(Model3D *model)
{
	RETRO_SortFaces(model);

	PhongLight light;
	light.x = RETRO_Render.lightsource.rx;
	light.y = RETRO_Render.lightsource.ry;
	light.z = RETRO_Render.lightsource.rz;
	light.shades = model->shades;

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		// The ramp the face is shaded in, as in the flat and gouraud renderers,
		// so one model can carry a material per face
		light.c = model->c + face->c;
		PolygonPoint point[RETRO_MAX_FACEVERTICES];

		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			point[j].x = vertex->sx;
			point[j].y = vertex->sy;
			point[j].q = vertex->q;
			// n * q; interpolating and renormalising is the same direction as /q.
			point[j].nx = model->normal[face->vertexnormal[j]].rx * vertex->q;
			point[j].ny = model->normal[face->vertexnormal[j]].ry * vertex->q;
			point[j].nz = model->normal[face->vertexnormal[j]].rz * vertex->q;
		}
		RETRO_DrawPhongPolygon(point, face->vertices, light);
	}
}

void RETRO_RenderTextureModel(Model3D *model, RETRO_POLY_SHADE shadertype)
{
	RETRO_SortFaces(model);
	unsigned char *shadetable = model->shadetable;
	bool lightingmap = shadertype == RETRO_SHADE_PHONG;
	bool envmapshading = shadertype == RETRO_SHADE_ENVIRONMENT || lightingmap;
	bool bumpmapping = model->bumpmap != NULL;
	// How far up the shade table one unit of lambert carries a face: the model's
	// own share of it, or the whole of it when the model names none. Unlike a
	// palette ramp, which a demo has to lay down before anything can index it,
	// the table is always RETRO_SHADES tall.
	int shades = model->shades ? model->shades : RETRO_SHADES;

	// A bump is lit by the dot product of a tilted normal with the light, so the
	// light is needed as a direction rather than as the shade it lands on
	float lightx = RETRO_Render.lightsource.rx;
	float lighty = RETRO_Render.lightsource.ry;
	float lightz = RETRO_Render.lightsource.rz;

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		// Rotated with the model, since the bump tilts along the surface's u and v
		TangentFrame frame = { face->tangent.rx, face->tangent.ry, face->tangent.rz,
							   face->bitangent.rx, face->bitangent.ry, face->bitangent.rz };
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			point[j].x = vertex->sx;
			point[j].y = vertex->sy;
			point[j].q = vertex->q;
			point[j].u = model->uv[face->uv[j]].u;
			point[j].v = model->uv[face->uv[j]].v;
			if (envmapshading) {
				Direction *normal = &model->normal[face->vertexnormal[j]];
				// A lighting map interpolates n*q (perspective-correct). A
				// reflection map takes the unit normal as it stands, since the
				// lookup is of direction, not of a quantity that varies with depth.
				float normalscale = lightingmap ? vertex->q : 1.0f;
				point[j].nx = normal->rx * normalscale;
				point[j].ny = normal->ry * normalscale;
				point[j].nz = normal->rz * normalscale;
			}
		}
		if (shadertype == RETRO_SHADE_NONE) {
			RETRO_DrawTexMapPolygon(point, face->vertices, model->texmap, model->texmapwidth, model->texmapheight);
		} else if (shadertype == RETRO_SHADE_TABLE) {
			// Texture mapped through the shade table at a fixed light level, with
			// no light source involved. face->c offsets it per face, so a model
			// can carry its own baked lighting, and the sum is a shade like any
			// other, so it is held to the ramp
			int shade = CLAMP128(model->c + face->c);
			if (bumpmapping) {
				// Same drawer as flat+bump: one shade and one face normal. The
				// tilt only moves the baked shade, so a flat patch stays at that
				// level.
				for (int j = 0; j < face->vertices; j++) {
					point[j].c = shade;
					point[j].nx = face->facenormal.rx;
					point[j].ny = face->facenormal.ry;
					point[j].nz = face->facenormal.rz;
				}
				RETRO_DrawTexMapBumpPolygon(point, face->vertices, model->texmap, model->bumpmap, model->bumpgrazing, shadetable, shades, lightx, lighty, lightz, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
			} else {
				RETRO_DrawTexMapEnvMapPolygon(point, face->vertices, model->texmap, model->envmap, shadetable, shade, false, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight);
			}
		} else if (shadertype == RETRO_SHADE_FLAT) {
			int shade = model->c + face->c;
			float lambert = RETRO_DotProduct(face->facenormal, RETRO_Render.lightsource);
			shade = CLAMP128(shade + RETRO_ShadeFromLambert(lambert) * shades);
			if (bumpmapping) {
				// A flat shaded face carries one shade and one normal over all of
				// it, which the bump mapper draws as every vertex holding both
				for (int j = 0; j < face->vertices; j++) {
					point[j].c = shade;
					point[j].nx = face->facenormal.rx;
					point[j].ny = face->facenormal.ry;
					point[j].nz = face->facenormal.rz;
				}
				RETRO_DrawTexMapBumpPolygon(point, face->vertices, model->texmap, model->bumpmap, model->bumpgrazing, shadetable, shades, lightx, lighty, lightz, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
			} else {
				RETRO_DrawTexMapEnvMapPolygon(point, face->vertices, model->texmap, model->envmap, shadetable, shade, false, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight);
			}
		} else if (shadertype == RETRO_SHADE_GOURAUD) {
			for (int j = 0; j < face->vertices; j++) {
				Direction *normal = &model->normal[face->vertexnormal[j]];
				float lambert = RETRO_DotProduct(*normal, RETRO_Render.lightsource);
				point[j].c = CLAMP128(model->c + face->c + RETRO_ShadeFromLambert(lambert) * shades);
				if (bumpmapping) {
					point[j].nx = normal->rx;
					point[j].ny = normal->ry;
					point[j].nz = normal->rz;
				}
			}
			if (bumpmapping) {
				RETRO_DrawTexMapBumpPolygon(point, face->vertices, model->texmap, model->bumpmap, model->bumpgrazing, shadetable, shades, lightx, lighty, lightz, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
			} else {
				RETRO_DrawTexMapGouraudPolygon(point, face->vertices, model->texmap, model->texmapwidth, model->texmapheight, shadetable);
			}
		} else if (envmapshading && !bumpmapping) {
			RETRO_DrawTexMapEnvMapPolygon(point, face->vertices, model->texmap, model->envmap, shadetable, 0, lightingmap, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight);
		} else if (envmapshading) {
			RETRO_DrawTexMapEnvMapBumpPolygon(point, face->vertices, model->texmap, model->envmap, model->bumpmap, model->bumpgrazing, shadetable, lightingmap, frame, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
		}
	}
}

void RETRO_RenderEnvironmentModel(Model3D *model, RETRO_POLY_SHADE shadertype)
{
	RETRO_SortFaces(model);
	bool lightingmap = shadertype == RETRO_SHADE_PHONG;
	bool bumpmapping = model->bumpmap != NULL;

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		TangentFrame frame = { face->tangent.rx, face->tangent.ry, face->tangent.rz,
							   face->bitangent.rx, face->bitangent.ry, face->bitangent.rz };
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			Direction *normal = &model->normal[face->vertexnormal[j]];
			point[j].x = vertex->sx;
			point[j].y = vertex->sy;
			point[j].q = vertex->q;
			if (bumpmapping) {
				point[j].u = model->uv[face->uv[j]].u;
				point[j].v = model->uv[face->uv[j]].v;
			}
			float normalscale = lightingmap ? vertex->q : 1.0f;
			point[j].nx = normal->rx * normalscale;
			point[j].ny = normal->ry * normalscale;
			point[j].nz = normal->rz * normalscale;
		}
		if (bumpmapping) {
			RETRO_DrawEnvMapBumpPolygon(point, face->vertices, model->envmap, model->bumpmap, model->bumpgrazing, lightingmap, frame, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight);
		} else {
			RETRO_DrawEnvMapPolygon(point, face->vertices, model->envmap, lightingmap, model->envmapwidth, model->envmapheight, model->envmapradius);
		}
	}
}

// One model, one depth range, so the depth buffer is cleared here by default.
// A demo drawing several models that interleave passes cleardepth false and
// clears once a frame itself, or each model would erase the depth of the ones
// before it. Glenz is exempt either way: it adds palette indices, so it
// depends on the order the sort gives it.
void RETRO_RenderModel(RETRO_POLY_TYPE rendertype, RETRO_POLY_SHADE shadertype = RETRO_SHADE_NONE, Model3D *model = NULL, bool cleardepth = true)
{
	model = model ? model : RETRO_Get3DModel();
	if (model == NULL) return;

	if (cleardepth) {
		RETRO_ClearDepthBuffer();
	}

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
	for (int i = 0; i < RETRO_MAX_MODELS; i++) {
		RETRO_Free3DModel(i);
	}
}

#endif
