//
// Retro graphics library
//
// Quake .map files, in the standard format: a list of entities, each a set of
// keys and values, the first of them, the worldspawn, holding the brushes the
// world is built of. A brush is a convex solid, the inside of a set of planes,
// each written as three points on it and the texture its face is drawn with.
//
// A compiled BSP keeps its entities in the same text, without the brushes,
// which have become its tree and faces.
//
// The coordinates are the file's own, z up. Nothing here knows what an
// entity's classname or a texture's name means; that is the reader's.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROMAP_H_
#define _RETROMAP_H_

#include "retro.h"
#include "retrovector.h"

#define RETRO_MAP_NAME 128				// The longest key, value or texture name, with its terminator
#define RETRO_MAP_MAX_KEYS 16			// Keys an entity may have
#define RETRO_MAP_MAX_ENTITIES 512
#define RETRO_MAP_MAX_BRUSHES 1024
#define RETRO_MAP_MAX_PLANES 16			// Planes a brush may have
#define RETRO_MAP_MAX_FACE (RETRO_MAP_MAX_PLANES + 4)	// Corners a face may have: a square cut once by each other plane

//
// One plane of a brush
//
// The three points are as the file gives them. The normal is
// (point[0] - point[1]) x (point[2] - point[1]), which by the format's
// convention points out of the brush, so the brush is where every plane has
// dot(normal, p) at most distance.
//
struct RETRO_MapPlane {
	vec3 point[3];
	char texture[RETRO_MAP_NAME];
	float offset[2];		// Texture alignment: shift, rotation and scale
	float rotation;
	float scale[2];
	vec3 normal;			// Unit, out of the brush
	float distance;			// dot(normal, any point on the plane)
};

struct RETRO_MapBrush {
	int planes;
	RETRO_MapPlane plane[RETRO_MAP_MAX_PLANES];
};

// An entity's keys and values, and the run of the map's brushes that are its
struct RETRO_MapEntity {
	int keys;
	char key[RETRO_MAP_MAX_KEYS][RETRO_MAP_NAME];
	char value[RETRO_MAP_MAX_KEYS][RETRO_MAP_NAME];
	int firstbrush;
	int brushes;
};

struct RETRO_Map {
	int entities;
	RETRO_MapEntity entity[RETRO_MAP_MAX_ENTITIES];
	int brushes;
	RETRO_MapBrush brush[RETRO_MAP_MAX_BRUSHES];
};

// *******************************************************************
// Reading
// *******************************************************************

//
// The tokens of a .map: braces, parentheses, quoted strings and bare words,
// with // comments skipped
//
struct RETRO_MapReader {
	const char *text;
	const char *filename;
	char token[RETRO_MAP_NAME];
	bool quoted;
};

// The next token, or false at the end of the text
inline bool RETRO_NextMapToken(RETRO_MapReader *reader)
{
	const char *p = reader->text;
	for (;;) {
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
		if (p[0] == '/' && p[1] == '/') {
			while (*p != 0 && *p != '\n') p++;
			continue;
		}
		break;
	}
	if (*p == 0) {
		reader->text = p;
		return false;
	}

	int length = 0;
	reader->quoted = *p == '"';
	if (reader->quoted) {
		p++;
		while (*p != 0 && *p != '"') {
			if (length == RETRO_MAP_NAME - 1) RETRO_RageQuit("Map string longer than %d characters: %s\n", RETRO_MAP_NAME - 1, reader->filename);
			reader->token[length++] = *p++;
		}
		if (*p != '"') RETRO_RageQuit("Map string not closed: %s\n", reader->filename);
		p++;
	} else if (strchr("{}()", *p)) {
		reader->token[length++] = *p++;
	} else {
		while (*p != 0 && !strchr(" \t\r\n{}()\"", *p)) {
			if (length == RETRO_MAP_NAME - 1) RETRO_RageQuit("Map word longer than %d characters: %s\n", RETRO_MAP_NAME - 1, reader->filename);
			reader->token[length++] = *p++;
		}
	}
	reader->token[length] = 0;
	reader->text = p;
	return true;
}

// The next token, which has to be there
inline const char *RETRO_ExpectMapToken(RETRO_MapReader *reader)
{
	if (!RETRO_NextMapToken(reader)) RETRO_RageQuit("Map ends too early: %s\n", reader->filename);
	return reader->token;
}

// The next token as a number
inline float RETRO_ExpectMapNumber(RETRO_MapReader *reader)
{
	const char *token = RETRO_ExpectMapToken(reader);
	char *end;
	float number = strtof(token, &end);
	if (end == token || *end != 0) RETRO_RageQuit("Map expected a number, read \"%s\": %s\n", token, reader->filename);
	return number;
}

// A point, as ( x y z )
inline vec3 RETRO_ExpectMapPoint(RETRO_MapReader *reader)
{
	if (strcmp(RETRO_ExpectMapToken(reader), "(") != 0) RETRO_RageQuit("Map expected ( before a point: %s\n", reader->filename);
	vec3 p;
	p.x = RETRO_ExpectMapNumber(reader);
	p.y = RETRO_ExpectMapNumber(reader);
	p.z = RETRO_ExpectMapNumber(reader);
	if (strcmp(RETRO_ExpectMapToken(reader), ")") != 0) RETRO_RageQuit("Map expected ) after a point: %s\n", reader->filename);
	return p;
}

// A brush, after its opening brace: planes up to the closing one
inline void RETRO_ReadMapBrush(RETRO_MapReader *reader, RETRO_MapBrush *brush)
{
	brush->planes = 0;
	for (;;) {
		const char *token = RETRO_ExpectMapToken(reader);
		if (strcmp(token, "}") == 0) break;
		if (strcmp(token, "(") != 0) RETRO_RageQuit("Map expected a plane in a brush, read \"%s\": %s\n", token, reader->filename);
		if (brush->planes == RETRO_MAP_MAX_PLANES) RETRO_RageQuit("Map brush has more than %d planes: %s\n", RETRO_MAP_MAX_PLANES, reader->filename);

		// The ( was taken already, so the first point is read by hand
		RETRO_MapPlane &plane = brush->plane[brush->planes++];
		plane.point[0].x = RETRO_ExpectMapNumber(reader);
		plane.point[0].y = RETRO_ExpectMapNumber(reader);
		plane.point[0].z = RETRO_ExpectMapNumber(reader);
		if (strcmp(RETRO_ExpectMapToken(reader), ")") != 0) RETRO_RageQuit("Map expected ) after a point: %s\n", reader->filename);
		plane.point[1] = RETRO_ExpectMapPoint(reader);
		plane.point[2] = RETRO_ExpectMapPoint(reader);

		token = RETRO_ExpectMapToken(reader);
		if (strcmp(token, "[") == 0) RETRO_RageQuit("Map is in the Valve 220 format, which is not read: %s\n", reader->filename);
		strcpy(plane.texture, token);
		plane.offset[0] = RETRO_ExpectMapNumber(reader);
		plane.offset[1] = RETRO_ExpectMapNumber(reader);
		plane.rotation = RETRO_ExpectMapNumber(reader);
		plane.scale[0] = RETRO_ExpectMapNumber(reader);
		plane.scale[1] = RETRO_ExpectMapNumber(reader);

		plane.normal = normalize(cross(plane.point[0] - plane.point[1], plane.point[2] - plane.point[1]));
		plane.distance = dot(plane.normal, plane.point[0]);
	}
	if (brush->planes < 4) RETRO_RageQuit("Map brush has fewer than 4 planes: %s\n", reader->filename);
}

//
// Read a map from its text
//
// The entities in order, each key and value kept and each brush appended to
// the map's list, so an entity's brushes are one run of it. filename is
// what the errors call the text. Freed with RETRO_FreeMap.
//
inline RETRO_Map *RETRO_ParseMap(const char *text, const char *filename)
{
	RETRO_Map *map = (RETRO_Map *)malloc(sizeof(RETRO_Map));
	if (map == NULL) RETRO_RageQuit("Cannot allocate map memory\n");
	map->entities = 0;
	map->brushes = 0;

	RETRO_MapReader reader = { text, filename, {}, false };
	while (RETRO_NextMapToken(&reader)) {
		if (strcmp(reader.token, "{") != 0) RETRO_RageQuit("Map expected { to open an entity, read \"%s\": %s\n", reader.token, filename);
		if (map->entities == RETRO_MAP_MAX_ENTITIES) RETRO_RageQuit("Map has more than %d entities: %s\n", RETRO_MAP_MAX_ENTITIES, filename);
		RETRO_MapEntity &entity = map->entity[map->entities++];
		entity.keys = 0;
		entity.firstbrush = map->brushes;
		entity.brushes = 0;

		for (;;) {
			const char *token = RETRO_ExpectMapToken(&reader);
			if (strcmp(token, "}") == 0) break;
			if (strcmp(token, "{") == 0) {
				if (map->brushes == RETRO_MAP_MAX_BRUSHES) RETRO_RageQuit("Map has more than %d brushes: %s\n", RETRO_MAP_MAX_BRUSHES, filename);
				RETRO_ReadMapBrush(&reader, &map->brush[map->brushes++]);
				entity.brushes++;
				continue;
			}
			if (!reader.quoted) RETRO_RageQuit("Map expected a quoted key, read \"%s\": %s\n", token, filename);
			if (entity.keys == RETRO_MAP_MAX_KEYS) RETRO_RageQuit("Map entity has more than %d keys: %s\n", RETRO_MAP_MAX_KEYS, filename);
			strcpy(entity.key[entity.keys], token);
			RETRO_ExpectMapToken(&reader);
			if (!reader.quoted) RETRO_RageQuit("Map expected a quoted value, read \"%s\": %s\n", reader.token, filename);
			strcpy(entity.value[entity.keys], reader.token);
			entity.keys++;
		}
	}

	return map;
}

//
// Load a .map file
//
inline RETRO_Map *RETRO_LoadMap(const char *filename)
{
	char *text = (char *)RETRO_LoadFile(filename);
	RETRO_Map *map = RETRO_ParseMap(text, filename);
	free(text);
	return map;
}

inline void RETRO_FreeMap(RETRO_Map *map)
{
	free(map);
}

// *******************************************************************
// Entities
// *******************************************************************

// An entity's value for a key, or null if it has none
inline const char *RETRO_MapValue(const RETRO_MapEntity &entity, const char *key)
{
	for (int i = 0; i < entity.keys; i++) {
		if (strcmp(entity.key[i], key) == 0) return entity.value[i];
	}
	return NULL;
}

// An entity's value for a key as a number, or fallback if it has none
inline float RETRO_MapNumber(const RETRO_MapEntity &entity, const char *key, float fallback = 0.0f)
{
	const char *value = RETRO_MapValue(entity, key);
	return value ? strtof(value, NULL) : fallback;
}

// An entity's value for a key as three numbers, such as an origin, or zero
inline vec3 RETRO_MapVector(const RETRO_MapEntity &entity, const char *key)
{
	vec3 v = {};
	const char *value = RETRO_MapValue(entity, key);
	if (value) sscanf(value, "%f %f %f", &v.x, &v.y, &v.z);
	return v;
}

// The first entity whose key has a value, such as the one a target names,
// or null
inline const RETRO_MapEntity *RETRO_FindMapEntity(const RETRO_Map *map, const char *key, const char *value)
{
	for (int i = 0; i < map->entities; i++) {
		const char *v = RETRO_MapValue(map->entity[i], key);
		if (v && strcmp(v, value) == 0) return &map->entity[i];
	}
	return NULL;
}

// *******************************************************************
// Brushes
// *******************************************************************

//
// The polygon of one of a brush's faces
//
// A square on the face's plane, centered on the brush and larger than it, is
// cut by every other plane, keeping the part inside, as a polygon is cut at a
// plane in Sutherland-Hodgman. The square is wound counterclockwise seen from
// outside, and cutting keeps the winding. Returns the corners written, up to
// RETRO_MAP_MAX_FACE; fewer than three is a plane that does not touch the
// brush.
//
inline int RETRO_MapBrushFace(const RETRO_MapBrush &brush, int face, vec3 *corner)
{
	// The brush's middle and size, from the points its planes are written with
	vec3 middle = {};
	for (int i = 0; i < brush.planes; i++) {
		for (int j = 0; j < 3; j++) {
			middle += brush.plane[i].point[j];
		}
	}
	middle = middle * (1.0f / (brush.planes * 3));
	float size = 1.0f;
	for (int i = 0; i < brush.planes; i++) {
		for (int j = 0; j < 3; j++) {
			size = MAX(size, length(brush.plane[i].point[j] - middle));
		}
	}
	size *= 2;

	// right x up is the normal, so the square's corners in the order below
	// wind counterclockwise looking against it
	const RETRO_MapPlane &plane = brush.plane[face];
	vec3 n = plane.normal;
	vec3 origin = middle - n * (dot(n, middle) - plane.distance);
	vec3 up = fabsf(n.z) > 0.9f ? vec3{ 1, 0, 0 } : vec3{ 0, 0, 1 };
	up = normalize(up - n * dot(up, n));
	vec3 right = cross(up, n);

	vec3 polygon[2][RETRO_MAP_MAX_FACE];
	polygon[0][0] = origin - right * size - up * size;
	polygon[0][1] = origin + right * size - up * size;
	polygon[0][2] = origin + right * size + up * size;
	polygon[0][3] = origin - right * size + up * size;
	int count = 4, from = 0;
	for (int i = 0; i < brush.planes && count >= 3; i++) {
		if (i == face) continue;
		const RETRO_MapPlane &cut = brush.plane[i];
		vec3 *in = polygon[from], *out = polygon[1 - from];
		int points = 0;
		for (int j = 0; j < count; j++) {
			vec3 a = in[j], b = in[(j + 1) % count];
			float da = dot(cut.normal, a) - cut.distance;
			float db = dot(cut.normal, b) - cut.distance;
			if (da <= 0) out[points++] = a;
			if ((da <= 0) != (db <= 0) && points < RETRO_MAP_MAX_FACE) out[points++] = mix(a, b, da / (da - db));
		}
		count = MIN(points, RETRO_MAP_MAX_FACE);
		from = 1 - from;
	}

	for (int i = 0; i < count; i++) {
		corner[i] = polygon[from][i];
	}
	return count;
}

#endif
