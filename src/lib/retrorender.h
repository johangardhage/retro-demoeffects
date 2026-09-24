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
#include "retropalette.h"
#include "retroshadetable.h"
#include "retrogfx.h"

enum RETRO_POLY_TYPE {
	RETRO_POLY_DOT,
	RETRO_POLY_WIREFRAME,
	RETRO_POLY_HIDDENLINE,
	RETRO_POLY_FLAT,
	RETRO_POLY_GLENZ,
	RETRO_POLY_GOURAUD,
	RETRO_POLY_PHONG,
	RETRO_POLY_MATCAP,		// a canned lighting response, looked up by screen-facing normal; see RETRO_POLY_ENVIRONMENT for the true reflection this is not
	RETRO_POLY_TEXTURE,
	RETRO_POLY_ENVIRONMENT,	// a true Blinn/Newell reflection map, sampled by the reflected view ray
	RETRO_POLY_SHADER		// each pixel handed to model->shader as a Fragment; face normals with RETRO_SHADE_FLAT
};

enum RETRO_POLY_SHADE {
	RETRO_SHADE_NONE,
	RETRO_SHADE_TABLE,
	RETRO_SHADE_WIREFIRE,
	RETRO_SHADE_FLAT,
	RETRO_SHADE_GOURAUD,
	RETRO_SHADE_ENVIRONMENT,	// RETRO_POLY_TEXTURE combined with a reflection map
	RETRO_SHADE_MATCAP			// RETRO_POLY_TEXTURE combined with a lighting map; RETRO_POLY_MATCAP needs no shadertype of its own
};

inline struct {
	UnitVector lightsource;
} RETRO_Render;

// Where a surface must face to catch the light, given at whatever scale is
// convenient and stored unit, like every other UnitVector. Only the direction is
// held so far, so the source has no position yet: it can be pointed, not moved.
inline void RETRO_InitializeLightSource(float x, float y, float z)
{
	RETRO_Render.lightsource = RETRO_LightSource(x, y, z);
}

// A light whose direction turns in a circle over time - x and y sweeping at
// radius while z stays fixed - in whatever space shading happens in, the
// same one RETRO_InitializeLightSource takes its direction in. Unlike that
// one, which sets the light once at startup, this is meant to be called
// every frame, so it writes RETRO_Render.lightsource directly rather than
// naming itself after a step that only runs once. It also returns the
// direction alongside setting it, since a caller that derives more than
// shading from the light - a planar shadow's cast direction, say - needs the
// same vector rather than a second one left to drift out of sync with it
inline vec3 RETRO_RotateLightSource(double time, float speed, float radius, float z)
{
	double angle = time * speed;
	vec3 lightsource = { (float)(radius * cos(angle)), (float)(radius * sin(angle)), z };
	RETRO_Render.lightsource = RETRO_LightSource(lightsource.x, lightsource.y, lightsource.z);
	return lightsource;
}

inline void RETRO_RenderDotModel(Model3D *model, bool shaded, bool onlyvisible = false, ClipRect clip = {})
{
	// How far past the silhouette, in the same units as facing below, a
	// vertex fades in over instead of popping straight to full color. The
	// winding test already dropped the back faces; this band sits on the
	// survivors so a face that has just crossed into view does not appear
	// at full color in one frame. A flat mesh's vertices share one face's
	// normal, so without this every vertex on a face would cross together.
	const float RETRO_DOT_FADE = 0.15f;

	// A vertex has no winding of its own. Visibility is whether any of the
	// faces that meet there is front-facing, by the same screen-space cross
	// RETRO_SortFaces already uses for every other renderer. The vertex-
	// normal z test is not that test under a pinhole: a side face can have
	// N.z slightly toward the camera while its projected winding is still
	// back-facing, and those vertices stamp through the cube.
	bool visible[RETRO_MAX_VERTICES];
	if (onlyvisible) {
		for (int i = 0; i < model->vertices; i++) {
			visible[i] = false;
		}
		RETRO_SortFaces(false, model);
		for (int i = 0; i < model->drawfaces; i++) {
			Face *face = &model->face[model->drawface[i]];
			for (int j = 0; j < face->vertices; j++) {
				visible[face->vertex[j]] = true;
			}
		}
	}

	// A split mesh stores each hard edge once per face. Those copies share a
	// position, and the side-face one is unlit under a headlight, so stamping
	// both would leave the floor on the lit rim. Among copies that would
	// draw, keep the more-facing one. Sharing a position groups them under a
	// lexicographic sort, so the copies of one vertex end up adjacent and
	// the search for them is O(n log n) rather than every vertex against
	// every other.
	bool drop[RETRO_MAX_VERTICES];
	float lambert[RETRO_MAX_VERTICES];
	if (shaded) {
		for (int i = 0; i < model->vertices; i++) {
			drop[i] = false;
			lambert[i] = RETRO_RotatedDot(model->normal[i], RETRO_Render.lightsource);
		}

		int order[RETRO_MAX_VERTICES];
		int eligible = 0;
		for (int i = 0; i < model->vertices; i++) {
			if ((onlyvisible && !visible[i]) || model->vertex[i].q <= 0.0f) {
				continue;
			}
			order[eligible++] = i;
		}
		if (eligible > 1) {
			RETRO_QuickSort(order, 0, eligible - 1, RETRO_VertexPosBefore, model);
		}
		for (int k = 0; k < eligible; ) {
			int best = order[k];
			int next = k + 1;
			while (next < eligible && model->vertex[order[next]].pos == model->vertex[best].pos) {
				if (lambert[order[next]] > lambert[best]) {
					drop[best] = true;
					best = order[next];
				} else {
					drop[order[next]] = true;
				}
				next++;
			}
			k = next;
		}
	}

	for (int i = 0; i < model->vertices; i++) {
		if (onlyvisible && !visible[i]) {
			continue;
		}
		if (shaded && drop[i]) {
			continue;
		}
		if (model->vertex[i].q > 0.0f) {
			float fade = 1.0f;
			if (onlyvisible) {
				float facing = -model->normal[i].rdir.z;
				if (facing > 0.0f) {
					fade = CLAMP01(facing / RETRO_DOT_FADE);
				}
			}
			int color = model->c;
			if (shaded) {
				// A dot has no face of its own to shade with, only the vertex
				// normal RETRO_InitializeVertexNormals averaged in from the
				// faces around it.
				int cstart = model->c;
				int cend = model->c + model->shades;
				color = CLAMP(model->c + RETRO_ShadeFromLambert(lambert[i]) * model->shades, cstart, cend);
			}
			if (fade < 1.0f) {
				// No alpha to blend with the background, so fade toward
				// model->c - the ramp's own visible floor - instead. Scaling
				// the index straight down toward 0 rounds a dot already near
				// the floor to the background color well before the
				// silhouette actually cuts it, which is indistinguishable
				// from the pop this was meant to soften.
				color = lround(model->c + fade * (color - model->c));
			}
			int x = (int)model->vertex[i].spos.x;
			int y = (int)model->vertex[i].spos.y;
			if (x >= clip.x0 && x < clip.x1 && y >= clip.y0 && y < clip.y1) {
				RETRO_PutPixel(x, y, color);
			}
		}
	}
}

inline void RETRO_RenderWireModel(Model3D *model, bool hiddenlines, bool fire, ClipRect clip = {})
{
	// Hidden lines means only the front faces are drawn; without it the back
	// ones are drawn too, so they go into the list as well.
	RETRO_SortFaces(!hiddenlines, model);

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		int color = model->c + face->c;

		for (int j = 0; j < face->vertices; j++) {
			Vertex *p1 = &model->vertex[face->vertex[j]];
			Vertex *p2 = &model->vertex[face->vertex[(j + 1) % face->vertices]];
			if (fire) {
				RETRO_DrawFireLine(p1->spos.x, p1->spos.y, p2->spos.x, p2->spos.y, color, model->shades, { clip.x0, clip.x1, clip.y0, clip.y1 });
			} else {
				RETRO_DrawLine(p1->spos.x, p1->spos.y, p2->spos.x, p2->spos.y, color, { clip.x0, clip.x1, clip.y0, clip.y1 });
			}
		}
	}
}

// The sign a face's normals are shaded with. A model that is not two sided has
// only front faces in the draw list, so this is 1 for every one of them and the
// shading is untouched; on a two sided model the face turned away is lit by the
// reverse of its normal, which is the direction that side of the surface
// actually points. Reversing the normal is the same as negating the lambert it
// produces, so a scalar is all that has to be carried
inline float RETRO_FaceSide(Face *face)
{
	return face->frontfacing ? 1.0f : -1.0f;
}

inline void RETRO_RenderFlatModel(Model3D *model, bool shaded, ClipRect clip = {})
{
	RETRO_SortFaces(model->twosided, model);

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			point[j].pos = model->vertex[face->vertex[j]].spos;
			point[j].q = model->vertex[face->vertex[j]].q;
		}

		int color = model->c + face->c;
		if (shaded) {
			// One lambert per face: color = c + face.c + ShadeFromLambert(N · L) * shades.
			float lambert = RETRO_FaceSide(face) * RETRO_RotatedDot(face->facenormal, RETRO_Render.lightsource);
			int cstart = model->c;
			int cend = model->c + face->c + model->shades;
			color = CLAMP(model->c + face->c + RETRO_ShadeFromLambert(lambert) * model->shades, cstart, cend);
		}
		RETRO_DrawFlatPolygon(point, face->vertices, color, clip);
	}
}

inline void RETRO_RenderGlenzModel(Model3D *model, RETRO_POLY_SHADE shadertype, ClipRect clip = {})
{
	RETRO_SortFaces(true, model);

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			point[j].pos = model->vertex[face->vertex[j]].spos;
		}
		const GlenzLighting &lighting = model->glenzlighting;
		int color;
		if (shadertype == RETRO_SHADE_FLAT) {
			float light = RETRO_RotatedDot(face->facenormal, RETRO_Render.lightsource);
			if (model->twosided) light = CLAMP01(RETRO_FaceSide(face) * light);
			float strength = face->frontfacing ? 1.0f : lighting.backstrength;
			int offset = model->twosided && !face->frontfacing ? face->backc : face->c;
			int shades = MAX(model->shades, 1);
			int basecolor = model->c + offset;
			// Signed lighting retains the historical full-range falloff below
			// face.c. Two-sided materials use their own half-open shade ramp,
			// including a zero offset as a valid dark material.
			int range = model->twosided ? shades - 1 : model->shades;
			int mincolor = model->twosided ? basecolor : model->c;
			int maxcolor = basecolor + (model->twosided ? shades : model->shades);
			double contribution = range * strength * (lighting.diffuse * light + lighting.highlight * pow(MAX(0.0, (double)light), (double)lighting.exponent));
			color = CLAMP(basecolor + contribution, mincolor, maxcolor);
		} else {
			// Unshaded Glenz draws both sides of every face. The winding test stored
			// in frontfacing selects the front or back palette contribution, and zero
			// makes that side fully transparent. Each contribution is added to the
			// framebuffer by RETRO_DrawGlenzPolygon.
			int offset = face->frontfacing ? face->c : face->backc;
			if (offset == 0) continue;
			color = model->c + offset;
		}
		RETRO_DrawGlenzPolygon(point, face->vertices, color, lighting.colormax, clip);
	}
}

inline void RETRO_RenderGouraudModel(Model3D *model, ClipRect clip = {})
{
	RETRO_SortFaces(model->twosided, model);

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		float side = RETRO_FaceSide(face);
		int cstart = model->c;
		int cend = model->c + face->c + model->shades;
		PolygonPoint point[RETRO_MAX_FACEVERTICES];

		for (int j = 0; j < face->vertices; j++) {
			point[j].pos = model->vertex[face->vertex[j]].spos;
			point[j].q = model->vertex[face->vertex[j]].q;
			float lambert = side * RETRO_RotatedDot(model->normal[face->vertexnormal[j]], RETRO_Render.lightsource);
			point[j].c = CLAMP(model->c + face->c + RETRO_ShadeFromLambert(lambert) * model->shades, cstart, cend);
		}
		RETRO_DrawGouraudPolygon(point, face->vertices, clip);
	}
}

inline void RETRO_RenderPhongModel(Model3D *model, ClipRect clip = {})
{
	RETRO_SortFaces(model->twosided, model);

	PhongLight light;
	light.dir = RETRO_Render.lightsource.rdir;
	light.shades = model->shades;

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		// The ramp the face is shaded in, as in the flat and gouraud renderers,
		// so one model can carry a material per face
		light.c = model->c + face->c;
		float side = RETRO_FaceSide(face);
		PolygonPoint point[RETRO_MAX_FACEVERTICES];

		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			point[j].pos = vertex->spos;
			point[j].q = vertex->q;
			// n * q; interpolating and renormalising is the same direction as /q.
			float normalscale = side * vertex->q;
			UnitVector *normal = &model->normal[face->vertexnormal[j]];
			point[j].n = normal->rdir * normalscale;
		}
		RETRO_DrawPhongPolygon(point, face->vertices, light, clip);
	}
}

inline void RETRO_RenderTextureModel(Model3D *model, RETRO_POLY_SHADE shadertype, ClipRect clip = {})
{
	RETRO_SortFaces(model->twosided, model);
	// The model's table is the shading-palette shape: a texture drawn from a
	// palette built for shading, with the whole ramp under each of its colors.
	// A texture that is a picture in its own palette has the other shape, and
	// says so; see ShadeTable.
	ShadeTable shadetable = { model->shadetable, RETRO_TEXTURE_COLORS, RETRO_SHADES };
	bool lightingmap = shadertype == RETRO_SHADE_MATCAP;
	bool envmapshading = shadertype == RETRO_SHADE_ENVIRONMENT || lightingmap;
	bool bumpmapping = model->bumpmap != NULL;
	// How far up the shade table one unit of lambert carries a face: the model's
	// own share of it, or the whole of it when the model names none. Unlike a
	// palette ramp, which a demo has to lay down before anything can index it,
	// the table is always RETRO_SHADES tall.
	int shades = model->shades ? model->shades : RETRO_SHADES;

	// A bump is lit by the dot product of a tilted normal with the light, so the
	// light is needed as a direction rather than as the shade it lands on
	vec3 light = RETRO_Render.lightsource.rdir;

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		float side = RETRO_FaceSide(face);
		// Rotated with the model, since the bump tilts along the surface's u and v.
		// The frame is left as the front's even on the side turned away, because
		// RETRO_BumpNormal reprojects the tangent onto whatever normal it is
		// handed and picks the bitangent that keeps the UV handedness
		TangentFrame frame = { face->tangent.rdir, face->bitangent.rdir };
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			point[j].pos = vertex->spos;
			point[j].q = vertex->q;
			point[j].uv = model->uv[face->uv[j]];
			if (envmapshading) {
				UnitVector *normal = &model->normal[face->vertexnormal[j]];
				// A lighting map interpolates n*q (perspective-correct). A
				// reflection map takes the unit normal as it stands, since the
				// lookup is of direction, not of a quantity that varies with depth.
				float normalscale = side * (lightingmap ? vertex->q : 1.0f);
				point[j].n = normal->rdir * normalscale;
			}
		}
		if (shadertype == RETRO_SHADE_NONE) {
			RETRO_DrawTexMapPolygon(point, face->vertices, model->texmap, model->texmapwidth, model->texmapheight, false, clip);
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
					point[j].n = face->facenormal.rdir * side;
				}
				RETRO_DrawTexMapBumpPolygon(point, face->vertices, model->texmap, model->bumpmap, model->bumpgrazing, shadetable, shades, light, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight, clip);
			} else {
				RETRO_DrawTexMapEnvMapPolygon(point, face->vertices, model->texmap, model->envmap, shadetable, shade, false, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight, clip);
			}
		} else if (shadertype == RETRO_SHADE_FLAT) {
			int shade = model->c + face->c;
			float lambert = side * RETRO_RotatedDot(face->facenormal, RETRO_Render.lightsource);
			shade = CLAMP128(shade + RETRO_ShadeFromLambert(lambert) * shades);
			if (bumpmapping) {
				// A flat shaded face carries one shade and one normal over all of
				// it, which the bump mapper draws as every vertex holding both
				for (int j = 0; j < face->vertices; j++) {
					point[j].c = shade;
					point[j].n = face->facenormal.rdir * side;
				}
				RETRO_DrawTexMapBumpPolygon(point, face->vertices, model->texmap, model->bumpmap, model->bumpgrazing, shadetable, shades, light, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight, clip);
			} else {
				RETRO_DrawTexMapEnvMapPolygon(point, face->vertices, model->texmap, model->envmap, shadetable, shade, false, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight, clip);
			}
		} else if (shadertype == RETRO_SHADE_GOURAUD) {
			for (int j = 0; j < face->vertices; j++) {
				UnitVector *normal = &model->normal[face->vertexnormal[j]];
				float lambert = side * RETRO_RotatedDot(*normal, RETRO_Render.lightsource);
				point[j].c = CLAMP128(model->c + face->c + RETRO_ShadeFromLambert(lambert) * shades);
				if (bumpmapping) {
					point[j].n = normal->rdir * side;
				}
			}
			if (bumpmapping) {
				RETRO_DrawTexMapBumpPolygon(point, face->vertices, model->texmap, model->bumpmap, model->bumpgrazing, shadetable, shades, light, frame, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight, clip);
			} else {
				RETRO_DrawTexMapGouraudPolygon(point, face->vertices, model->texmap, model->texmapwidth, model->texmapheight, shadetable, false, clip);
			}
		} else if (envmapshading && !bumpmapping) {
			RETRO_DrawTexMapEnvMapPolygon(point, face->vertices, model->texmap, model->envmap, shadetable, 0, lightingmap, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight, clip);
		} else if (envmapshading) {
			RETRO_DrawTexMapEnvMapBumpPolygon(point, face->vertices, model->texmap, model->envmap, model->bumpmap, model->bumpgrazing, shadetable, lightingmap, frame, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight, clip);
		}
	}
}

// True reflection: the map holds a picture of the surroundings, sampled by
// the ray the visible surface point reflects towards. That ray times q is
// interpolated (perspective-correct) and renormalising it is the same
// direction as dividing by q first, the trick RETRO_RenderPhongModel uses;
// skipping it (interpolating the ray alone, affinely in screen space) is only
// close on a face so gently curved that its corners barely differ, and comes apart on
// a coarse, foreshortened face, where screen-space and surface-space pull far
// enough apart that the sampled direction swings wildly pixel to pixel. See
// RETRO_RenderMatcapModel for the lighting-map counterpart this is not.
//
// The map is baked for one view ray, I0 = (0, 0, 1), down the axis, and by
// default every pixel is looked up by its normal as though seen along it.
// With envmapperspective a vertex off the axis is instead seen along its
// own I, from the eye at (0, 0, -eye), and reflects
//
//   R = I - 2(N·I)N
//
// which is what each vertex passes, for RETRO_GetReflectionMapCoordinates to
// look up, at a second square root a pixel. On a curved surface that differs
// little from reflecting I0, which is why it is not the default; past the
// silhouette the two part ways, see RETRO_GetReflectionMapCoordinates. On a flat
// face it is the whole picture: every corner shares the one N, only I
// differs, and the face shows the slice of the room a plane mirror there
// would. R is linear in I, and I left unnormalized, times q, is linear in
// screen space, so R q interpolates exactly across a flat face and a straight
// edge in the room stays straight in the mirror.
//
// A bump map tilts a normal, not a ray, so the bumped path is handed the
// normal that reflects I0 to R instead, the half-way vector N' = (R - I0)
// normalized, with R unit. That is exact at the corners and bends in between.
inline vec3 RETRO_ReflectionVector(vec3 n, vec3 rpos, float eye)
{
	vec3 i = eye > 0.0f ? vec3{ rpos.x, rpos.y, rpos.z + eye } : vec3{ 0.0f, 0.0f, 1.0f };
	return i - n * (2.0f * dot(n, i));
}

inline vec3 RETRO_ReflectionNormal(vec3 n, vec3 rpos, float eye)
{
	return RETRO_ReflectionHalfway(RETRO_ReflectionVector(n, rpos, eye), n);
}

inline void RETRO_RenderEnvironmentModel(Model3D *model, ClipRect clip = {})
{
	RETRO_SortFaces(model->twosided, model);
	bool bumpmapping = model->bumpmap != NULL;

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		float side = RETRO_FaceSide(face);
		TangentFrame frame = { face->tangent.rdir, face->bitangent.rdir };
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			UnitVector *normal = &model->normal[face->vertexnormal[j]];
			point[j].pos = vertex->spos;
			point[j].q = vertex->q;
			if (bumpmapping) {
				point[j].uv = model->uv[face->uv[j]];
			}
			if (!model->envmapperspective) {
				point[j].n = normal->rdir * (side * vertex->q);
			} else if (bumpmapping) {
				point[j].n = RETRO_ReflectionNormal(normal->rdir * side, vertex->rpos, model->eye) * vertex->q;
			} else {
				point[j].n = RETRO_ReflectionVector(normal->rdir * side, vertex->rpos, model->eye) * vertex->q;
			}
		}
		if (bumpmapping) {
			RETRO_DrawEnvMapBumpPolygon(point, face->vertices, model->envmap, model->bumpmap, model->bumpgrazing, false, frame, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight, clip);
		} else {
			RETRO_DrawEnvMapPolygon(point, face->vertices, model->envmap, false, model->envmapperspective, model->envmapwidth, model->envmapheight, model->envmapradius, clip);
		}
	}
}

// Per-pixel shading by the model's own function: each pixel is described as
// a Fragment, and model->shader returns its colour. That reaches what no
// fixed drawer does, tracing a reflected ray into a scene for one, at the
// cost of a call a pixel. flat gives every pixel of a face the face's own
// normal; otherwise the vertex normals are interpolated across it. The eye
// is at (0, 0, -eye).
inline void RETRO_RenderShaderModel(Model3D *model, bool flat, ClipRect clip = {})
{
	if (model->shader == NULL) return;

	RETRO_SortFaces(model->twosided, model);
	vec3 eye = { 0.0f, 0.0f, -model->eye };

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		float side = RETRO_FaceSide(face);
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			vec3 normal = flat ? face->facenormal.rdir : model->normal[face->vertexnormal[j]].rdir;
			point[j].pos = vertex->spos;
			point[j].q = vertex->q;
			point[j].n = normal * (side * vertex->q);
			point[j].p = vertex->rpos * vertex->q;
			point[j].uv = model->uvs > 0 ? model->uv[face->uv[j]] * vertex->q : vec2{ 0.0f, 0.0f };
		}
		RETRO_DrawShaderPolygon(point, face->vertices, eye, model->shader, clip);
	}
}

// Matcap: the map holds a canned lighting response, sampled by screen-facing
// normal. n * q is interpolated (perspective-correct) and renormalising it
// is the same direction as dividing by q, the same trick RETRO_RenderPhongModel
// uses for a true per-pixel normal.
inline void RETRO_RenderMatcapModel(Model3D *model, ClipRect clip = {})
{
	RETRO_SortFaces(model->twosided, model);
	bool bumpmapping = model->bumpmap != NULL;

	for (int i = 0; i < model->drawfaces; i++) {
		Face *face = &model->face[model->drawface[i]];
		float side = RETRO_FaceSide(face);
		TangentFrame frame = { face->tangent.rdir, face->bitangent.rdir };
		PolygonPoint point[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			Vertex *vertex = &model->vertex[face->vertex[j]];
			UnitVector *normal = &model->normal[face->vertexnormal[j]];
			point[j].pos = vertex->spos;
			point[j].q = vertex->q;
			if (bumpmapping) {
				point[j].uv = model->uv[face->uv[j]];
			}
			point[j].n = normal->rdir * (side * vertex->q);
		}
		if (bumpmapping) {
			RETRO_DrawEnvMapBumpPolygon(point, face->vertices, model->envmap, model->bumpmap, model->bumpgrazing, true, frame, model->envmapwidth, model->envmapheight, model->envmapradius, model->texmapwidth, model->texmapheight, model->bumpmapwidth, model->bumpmapheight, clip);
		} else {
			RETRO_DrawEnvMapPolygon(point, face->vertices, model->envmap, true, false, model->envmapwidth, model->envmapheight, model->envmapradius, clip);
		}
	}
}

// One model, one depth range, so the depth buffer is cleared here by default.
// A demo drawing several models that interleave passes cleardepth false and
// clears once a frame itself, or each model would erase the depth of the ones
// before it. Glenz is exempt either way: it adds palette indices, so it
// depends on the order the sort gives it.
//
// clip restricts the draw to a horizontal band of the screen, for a demo
// that gives different rows their own renderer. The range is passed into
// the drawers (RETRO_ScanTriangle, RETRO_RenderDotModel, RETRO_RenderWireModel),
// so nothing outside the band is touched and a caller stacking several bands
// needs no backup/restore of its own between them.
inline void RETRO_RenderModel(RETRO_POLY_TYPE rendertype, RETRO_POLY_SHADE shadertype = RETRO_SHADE_NONE, Model3D *model = NULL, bool cleardepth = true, ClipRect clip = {})
{
	model = model ? model : RETRO_Get3DModel();
	if (model == NULL) return;

	if (cleardepth) {
		RETRO_ClearDepthBuffer();
	}

	switch (rendertype) {
	case RETRO_POLY_DOT:
		RETRO_RenderDotModel(model, shadertype == RETRO_SHADE_FLAT, false, clip);
		break;
	case RETRO_POLY_WIREFRAME:
		RETRO_RenderWireModel(model, false, shadertype == RETRO_SHADE_WIREFIRE, clip);
		break;
	case RETRO_POLY_HIDDENLINE:
		RETRO_RenderWireModel(model, true, shadertype == RETRO_SHADE_WIREFIRE, clip);
		break;
	case RETRO_POLY_FLAT:
		RETRO_RenderFlatModel(model, shadertype == RETRO_SHADE_FLAT, clip);
		break;
	case RETRO_POLY_GLENZ:
		RETRO_RenderGlenzModel(model, shadertype, clip);
		break;
	case RETRO_POLY_GOURAUD:
		RETRO_RenderGouraudModel(model, clip);
		break;
	case RETRO_POLY_PHONG:
		RETRO_RenderPhongModel(model, clip);
		break;
	case RETRO_POLY_MATCAP:
		RETRO_RenderMatcapModel(model, clip);
		break;
	case RETRO_POLY_TEXTURE:
		RETRO_RenderTextureModel(model, shadertype, clip);
		break;
	case RETRO_POLY_ENVIRONMENT:
		RETRO_RenderEnvironmentModel(model, clip);
		break;
	case RETRO_POLY_SHADER:
		RETRO_RenderShaderModel(model, shadertype == RETRO_SHADE_FLAT, clip);
		break;
	}
}

inline void RETRO_Initialize_3D(void)
{
}

inline void RETRO_Deinitialize_3D(void)
{
	for (int i = 0; i < RETRO_MAX_MODELS; i++) {
		RETRO_Free3DModel(i);
	}
}

#endif
