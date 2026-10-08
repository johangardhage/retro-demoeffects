//
// Quake map viewer
//
// Quake's start map, walked through freely and drawn in software from the
// BSP file with its palette and colormap. The view starts where the map's
// info_player_start entity puts the player, at Quake's eye height.
//
// Each frame the camera's leaf is found by walking the BSP tree, and that
// leaf's potentially visible set, run-length encoded one bit per leaf, names
// the leaves worth drawing. Their faces that turn toward the camera are
// listed once each and drawn near to far, by the distance to their first
// corner, so the depth test turns hidden pixels away before they are shaded.
// Each face's corners are taken into the camera's frame, the face is clipped
// to the sides of the view and the near plane and projected through the
// lens, and what is left goes to the library's shader drawer, which colors
// each pixel where 1/depth is greater than the depth buffer holds.
//
// Texture coordinates come from each face's two texture axes and are
// interpolated over depth, so they stay correct in perspective. A face takes one
// mip level, picked from the most texels per pixel along any of its edges.
// Its lightmap holds one luxel per 16 texels and up to four light styles,
// each scaled by its animated brightness, and the summed light picks one of
// the colormap's 64 shades of the texel; palette entries 224-255 are
// fullbright and keep their color. Liquids ripple with a sine warp, "+0" to
// "+9" textures animate at 10 frames per second, and the sky is two layers
// scrolling at different speeds, looked up along the view ray through each
// pixel. A texture missing from the map is drawn as Quake draws it, a
// checkerboard.
//
// W/S or Up/Down move, Left/Right turn and A/D strafe. The mouse looks
// around and Page Up/Page Down pitch.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#define RETRO_HEIGHT 200

#include <ctype.h> // tolower
#include <float.h> // FLT_MAX
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromouse.h"
#include "lib/retrocamera.h"
#include "lib/retrobsp.h"
#include "lib/retromath.h"

#define MOVEMENT_SPEED		480.0f	// World units per second
#define TURN_SPEED			radians(180)	// Radians per second, from the arrow keys
#define PITCH_SPEED			radians(60)		// Radians per second, from Page Up/Page Down
#define MOUSE_SENSITIVITY	radians(0.3)	// Radians per mouse step
#define EYE_HEIGHT			22.0f	// World units from the player's origin up to the eye, as in Quake
#define ANIMATION_RATE		10.0	// Frames per second of animated textures and light styles
#define LIGHT_STYLES		256		// Light styles a face can name

// One mip level of a texture: 8-bit palette indices, read in place from the
// BSP or the checkerboard
struct MipLevel
{
	int width;
	int height;
	unsigned char *pixels;
};

// A BSP texture: its four mip levels, its kind, and its "+0".."+9" animation
struct Texture
{
	const char *name;		// Empty for a missing texture
	MipLevel levels[MIPLEVELS];
	bool issky;				// Drawn as the scrolling sky
	bool isturbulent;		// A liquid, sampled with a rippling sine warp
	int frames;				// Frames in its animation; 0 or 1 is not animated
	int frame[10];			// Texture index of each frame, -1 for a gap
};

// A corner of a face
struct Corner
{
	vec3 point;
	vec2 uv;				// Texture coordinates, over the texture's size
	vec2 lightuv;			// Lightmap coordinates, in luxels
};

// A face's lightmap: one block of width x height luxels for each light style
// that lights it, one after the other
struct Lightmap
{
	int width;
	int height;
	int styles;						// Blocks; 0 draws the face full-bright
	const unsigned char *style;		// The light style of each block
	const unsigned char *samples;	// The blocks, in the BSP's lighting lump
};

// What the shader colors a face's pixels with
struct FaceShading
{
	const Texture *texture;
	const Lightmap *lightmap;	// NULL draws full-bright
	int miplevel;
};

static RETRO_BSP Map;
static Texture *Textures;
static Corner *Corners;					// Every face's corners, at their index in the BSP's edge list
static Lightmap *Lightmaps;				// One per face
static int *VisibleFaces;				// The faces drawn this frame, near to far
static float *FaceDistance;				// Squared distance from the camera to each face's first corner
static bool *Listed;					// Whether a face is in this frame's list yet
static unsigned char LightTable[256 * 256];	// The shade of each color at each light level
static unsigned char Checkerboard[16 * 16 + 8 * 8 + 4 * 4 + 2 * 2];	// The missing texture's mip levels
static float LightScale[LIGHT_STYLES];	// Each light style's brightness now, 1 being normal
static double Time;						// Seconds since the start, which the animations run on
static RETRO_Camera Camera;
static RETRO_Look Look;				// The heading about the up axis, and the look up or down

//
// The sky's two layers along the view ray through a pixel. The texture holds
// them side by side, the clouds on the left and the solid back on the right;
// the back scrolls slowly and the clouds twice as fast over it.
//
static unsigned char SampleSky(const Texture *texture, int x, int y)
{
	const MipLevel *mip = &texture->levels[0];

	// Project the ray onto Quake's sky dome: flatten z, then scale so the
	// horizontal direction wraps across one 128-texel layer
	vec3 dir = RETRO_ViewRay(&Camera, { x + 0.5f, y + 0.5f });
	dir.z *= 3.0f;
	float scale = (6.0f * 63.0f) / length(dir);
	int layer = mip->width / 2;

	float back = (float)(Time * 8.0);
	int backx = WRAP((int)floorf(back + dir.x * scale), layer);
	int backy = WRAP((int)floorf(back + dir.y * scale), mip->height);
	float front = (float)(Time * 16.0);
	int frontx = WRAP((int)floorf(front + dir.x * scale), layer);
	int fronty = WRAP((int)floorf(front + dir.y * scale), mip->height);

	unsigned char cloud = mip->pixels[frontx + fronty * mip->width];
	return cloud != 0 ? cloud : mip->pixels[layer + backx + backy * mip->width];
}

//
// The texel at texture coordinates, which liquids ripple with a sine warp
//
static unsigned char SampleTexture(const Texture *texture, int level, vec2 uv)
{
	const MipLevel *mip = &texture->levels[level];
	float s = uv.x * (float)mip->width;
	float t = uv.y * (float)mip->height;
	if (texture->isturbulent) {
		float time = (float)Time;
		float warps = sinf((t + time * 96.0f) * 0.125f) * 4.0f;
		float warpt = sinf((s + time * 80.0f) * 0.125f) * 4.0f;
		s += warps;
		t += warpt;
	}

	int x = WRAP((int)floorf(s), mip->width);
	int y = WRAP((int)floorf(t), mip->height);
	return mip->pixels[x + y * mip->width];
}

//
// The light at lightmap coordinates: the nearest luxel of each block, scaled
// by its style's brightness now and summed
//
static unsigned char SampleLightmap(const Lightmap *lightmap, vec2 lightuv)
{
	if (lightmap->styles == 0) {
		return 255;
	}

	int s = (int)floorf(clamp(lightuv.x, 0.0f, (float)(lightmap->width - 1)) + 0.5f);
	int t = (int)floorf(clamp(lightuv.y, 0.0f, (float)(lightmap->height - 1)) + 0.5f);
	const unsigned char *sample = lightmap->samples + s + t * lightmap->width;

	float light = 0.0f;
	for (int i = 0; i < lightmap->styles; i++, sample += lightmap->width * lightmap->height) {
		light += (float)*sample * LightScale[lightmap->style[i]];
	}
	return (unsigned char)MIN((int)(light + 0.5f), 255);
}

//
// Color one pixel of a face. A sky texel comes from the view ray through the
// pixel; any other texture is sampled at the pixel's perspective-correct
// coordinates and lit through the face's lightmap.
//
static unsigned char ShadeFace(const Fragment &fragment)
{
	const FaceShading *shading = (const FaceShading *)fragment.data;
	if (shading->texture->issky) {
		return SampleSky(shading->texture, fragment.x, fragment.y);
	}

	unsigned char color = SampleTexture(shading->texture, shading->miplevel, fragment.uv);
	if (shading->lightmap) {
		color = LightTable[color * 256 + SampleLightmap(shading->lightmap, fragment.lightuv)];
	}
	return color;
}

//
// The frame of a texture's animation showing now. A gap in a sequence shows
// the "+0" frame.
//
static const Texture *AnimationFrame(const Texture *texture)
{
	if (texture->frames <= 1) {
		return texture;
	}
	int frame = texture->frame[(int)(Time * ANIMATION_RATE) % texture->frames];
	return frame < 0 ? texture : &Textures[frame];
}

//
// One mip level for a whole face, so it does not change across the face: the
// one whose texels match the pixels along the edge with the most texels per
// pixel. Each level halves the texels, so that is log2 of the most, which is
// at least 1.
//
static int FaceMipLevel(const Texture *texture, const Corner *corners, const RETRO_CameraVertex *vertex, int count)
{
	const RETRO_CameraLens &lens = Camera.lens;
	float most = 1.0f;
	for (int i = 0; i < count; i++) {
		int j = (i + 1) % count;
		if (vertex[i].eye.z < lens.nearplane || vertex[j].eye.z < lens.nearplane) {
			continue;
		}

		float pixels = length(RETRO_ProjectViewPoint(lens, vertex[j].eye).pos - RETRO_ProjectViewPoint(lens, vertex[i].eye).pos);
		if (pixels < 1.0f) {
			continue;
		}

		float du = (corners[j].uv.x - corners[i].uv.x) * (float)texture->levels[0].width;
		float dv = (corners[j].uv.y - corners[i].uv.y) * (float)texture->levels[0].height;
		float texels = sqrtf(du * du + dv * dv);
		most = MAX(most, texels / pixels);
	}

	return MIN((int)floorf(log2f(most)), MIPLEVELS - 1);
}

//
// Draw one face. Sky and liquids are flagged TEX_SPECIAL: they are drawn
// full-bright and unmipped.
//
static void DrawFace(int index)
{
	const dface_t *face = Map.face(index);
	const texinfo_t *info = Map.textureinfo(face->texinfo);
	const Corner *corners = &Corners[face->firstedge];

	// The corners in the camera's frame
	RETRO_CameraVertex vertex[RETRO_CAMERA_MAX_POLYGON];
	for (int i = 0; i < face->numedges; i++) {
		vertex[i] = {};
		vertex[i].eye = RETRO_ViewPoint(&Camera, corners[i].point);
		vertex[i].uv = corners[i].uv;
		vertex[i].lightuv = corners[i].lightuv;
	}

	bool special = (info->flags & TEX_SPECIAL) != 0;
	FaceShading shading;
	shading.texture = AnimationFrame(&Textures[info->miptex]);
	shading.lightmap = special ? NULL : &Lightmaps[index];
	shading.miplevel = special ? 0 : FaceMipLevel(shading.texture, corners, vertex, face->numedges);

	// Clip the face to the view and shade what is left. The shader drawer takes
	// the coordinates times q, and reads neither a normal nor a view-space
	// point from them.
	PolygonPoint point[RETRO_CAMERA_MAX_POLYGON + RETRO_CAMERA_CLIP_PLANES] = {};
	int count = RETRO_ClipProjectViewPolygon(Camera.lens, vertex, face->numedges, point);
	for (int i = 0; i < count; i++) {
		point[i].uv = point[i].uv * point[i].q;
		point[i].lightuv = point[i].lightuv * point[i].q;
	}
	RETRO_DrawShaderPolygon(point, count, {}, ShadeFace, &shading);
}

//
// The brightness of every light style now. Quake's patterns hold one letter
// for every tenth of a second, from 'a' dark through 'm' normal to 'z'
// bright; the styles past them are steady. Each letter is a step of 22 from
// 'a', which makes 'm' 264, the normal brightness.
//
static void UpdateLightStyles(void)
{
	static const char *patterns[] = {
		"m",
		"mmnmmommommnonmmonqnmmo",
		"abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba",
		"mmmmmaaaaammmmmaaaaaabcdefgabcdefg",
		"mamamamamama",
		"jklmnopqrstuvwxyzyxwvutsrqponmlkj",
		"nmonqnmomnmomomno",
		"mmmaaaabcdefgmmmmaaaammmaamm",
		"mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa",
		"aaaaaaaazzzzzzzz",
		"mmamammmmammamamaaamammma",
		"abcdefghijklmnopqrrqponmlkjihgfedcba",
		"mmnnmmnnnmmnn",
		"kmjmlnklkj",
		"mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmaaaaaaaazzzzzzzz",
	};

	int step = (int)(Time * ANIMATION_RATE);
	int count = (int)(sizeof(patterns) / sizeof(patterns[0]));
	for (int style = 0; style < LIGHT_STYLES; style++) {
		const char *pattern = style < count ? patterns[style] : "m";
		int value = (pattern[step % (int)strlen(pattern)] - 'a') * 22;
		LightScale[style] = (float)value / 264.0f;
	}
}

// True when the camera is on the side of a face that is drawn: the side of
// its plane the normal points to, or the other one when the face is flagged
static bool FacesCamera(const dface_t *face)
{
	const dplane_t *plane = Map.plane(face->planenum);
	float distance = dot(plane->normal, Camera.pos) - plane->dist;
	return face->side ? distance <= 0.0f : distance >= 0.0f;
}

//
// Draw what the camera can see: the faces of the leaves its leaf may see that
// turn toward it, each once, near to far. Leaves share the faces on their
// borders.
//
static void DrawScene(void)
{
	UpdateLightStyles();
	RETRO_ClearDepthBuffer();
	memset(Listed, 0, Map.numfaces() * sizeof(bool));

	int leaves[MAX_MAP_LEAFS];
	int numleaves = RETRO_BSPVisibleLeaves(&Map, RETRO_BSPFindLeaf(&Map, Camera.pos), leaves);
	int count = 0;
	for (int i = 0; i < numleaves; i++) {
		const dleaf_t *leaf = Map.leaf(leaves[i]);
		for (int j = 0; j < leaf->nummarksurfaces; j++) {
			int index = Map.marksurface(leaf->firstmarksurface + j);
			const dface_t *face = Map.face(index);
			if (Listed[index] || !FacesCamera(face)) {
				continue;
			}
			Listed[index] = true;
			vec3 delta = Corners[face->firstedge].point - Camera.pos;
			FaceDistance[index] = dot(delta, delta);
			VisibleFaces[count++] = index;
		}
	}

	// Faces at the same distance go in face order
	RETRO_SortIndices(VisibleFaces, FaceDistance, count);
	for (int i = 0; i < count; i++) {
		DrawFace(VisibleFaces[i]);
	}
}

//
// Find each texture's mip levels in the BSP and its kind from its name, then
// gather each "+0".."+9" sequence's frames on its "+0" texture. A missing
// texture, or a slot without texel data, gets Quake's 16x16 checkerboard.
//
static void DecodeTextures(void)
{
	MipLevel missing[MIPLEVELS];
	for (int level = 0, offset = 0; level < MIPLEVELS; level++) {
		int size = 16 >> level;
		missing[level] = { size, size, Checkerboard + offset };
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				Checkerboard[offset++] = (x < size / 2) != (y < size / 2) ? 0 : 255;
			}
		}
	}

	int count = Map.numtextures();
	Textures = new Texture [count];
	for (int i = 0; i < count; i++) {
		Texture *texture = &Textures[i];
		*texture = {};
		for (int frame = 0; frame < 10; frame++) {
			texture->frame[frame] = -1;
		}

		const miptex_t *miptex = Map.miptexture(i);
		if (!miptex || !miptex->name[0] || miptex->offsets[0] == 0) {
			texture->name = "";
			for (int level = 0; level < MIPLEVELS; level++) {
				texture->levels[level] = missing[level];
			}
			continue;
		}

		const char *name = texture->name = miptex->name;
		texture->issky = tolower(name[0]) == 's' && tolower(name[1]) == 'k' && tolower(name[2]) == 'y';
		texture->isturbulent = name[0] == '*';
		for (int level = 0; level < MIPLEVELS; level++) {
			texture->levels[level] = { (int)miptex->width >> level, (int)miptex->height >> level, (unsigned char *)miptex + miptex->offsets[level] };
		}
	}

	for (int i = 0; i < count; i++) {
		const char *base = Textures[i].name;
		if (base[0] != '+' || base[1] != '0') {
			continue;
		}
		for (int j = 0; j < count; j++) {
			const char *name = Textures[j].name;
			if (name[0] != '+' || !isdigit(name[1]) || strncmp(base + 2, name + 2, 14) != 0) {
				continue;
			}
			int frame = name[1] - '0';
			Textures[i].frame[frame] = j;
			Textures[i].frames = MAX(Textures[i].frames, frame + 1);
		}
	}
}

//
// Take each face's corners out of the BSP, with their texture and lightmap
// coordinates, and find its lightmap
//
static void DecodeFaces(void)
{
	int count = Map.numfaces();
	Corners = new Corner [Map.numsurfedges()];
	Lightmaps = new Lightmap [count];
	VisibleFaces = new int [count];
	FaceDistance = new float [count];
	Listed = new bool [count];

	for (int i = 0; i < count; i++) {
		const dface_t *face = Map.face(i);
		if (face->numedges > RETRO_CAMERA_MAX_POLYGON) {
			RETRO_RageQuit("Map face has more than %d corners\n", RETRO_CAMERA_MAX_POLYGON);
		}

		const texinfo_t *info = Map.textureinfo(face->texinfo);
		float width = (float)Textures[info->miptex].levels[0].width;
		float height = (float)Textures[info->miptex].levels[0].height;

		// The corners are taken in reverse of the edge list's order. A positive
		// entry uses its edge forward, from the start vertex, and a negative one
		// reversed, from the end vertex. Texel coordinates run along the two
		// texture axes.
		Corner *corner = &Corners[face->firstedge];
		vec2 lowest = { FLT_MAX, FLT_MAX };
		vec2 highest = { -FLT_MAX, -FLT_MAX };
		for (int j = 0; j < face->numedges; j++) {
			int edge = Map.surfedge(face->firstedge + face->numedges - 1 - j);
			int vertex = edge >= 0 ? Map.edge(edge)->v[0] : Map.edge(-edge)->v[1];
			corner[j].point = Map.vertex(vertex)->point;
			float s = dot(info->vecs[0].axis, corner[j].point) + info->vecs[0].offset;
			float t = dot(info->vecs[1].axis, corner[j].point) + info->vecs[1].offset;
			corner[j].uv = { s / width, t / height };
			corner[j].lightuv = { s, t };
			lowest = min(lowest, corner[j].lightuv);
			highest = max(highest, corner[j].lightuv);
		}

		// The lightmap spans the texel bounds at one luxel per 16 texels, and
		// has a block for each light style up to the first 255
		int lights = (int)floorf(lowest.x / 16.0f);
		int lightt = (int)floorf(lowest.y / 16.0f);
		Lightmap *lightmap = &Lightmaps[i];
		lightmap->width = (int)ceilf(highest.x / 16.0f) - lights + 1;
		lightmap->height = (int)ceilf(highest.y / 16.0f) - lightt + 1;
		lightmap->style = face->styles;
		lightmap->samples = Map.lightmap(face->lightofs);
		lightmap->styles = 0;
		while (lightmap->samples && lightmap->styles < MAXLIGHTMAPS && face->styles[lightmap->styles] != 255) {
			lightmap->styles++;
		}

		for (int j = 0; j < face->numedges; j++) {
			corner[j].lightuv = (corner[j].lightuv - vec2{ (float)(lights * 16), (float)(lightt * 16) }) / 16.0f;
		}
	}
}

//
// Turn and move with the keyboard and mouse, then draw the scene
//
void DEMO_Render(RETRO_Time time)
{
	float dt = (float)time.delta;
	float walk = 0;
	float strafe = 0;
	if (RETRO_KeyState(SDL_SCANCODE_W) || RETRO_KeyState(SDL_SCANCODE_UP)) walk += MOVEMENT_SPEED * dt;
	if (RETRO_KeyState(SDL_SCANCODE_S) || RETRO_KeyState(SDL_SCANCODE_DOWN)) walk -= MOVEMENT_SPEED * dt;
	if (RETRO_KeyState(SDL_SCANCODE_D)) strafe += MOVEMENT_SPEED * dt;
	if (RETRO_KeyState(SDL_SCANCODE_A)) strafe -= MOVEMENT_SPEED * dt;
	float turn = 0;
	float tilt = 0;
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) turn += TURN_SPEED * dt;
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) turn -= TURN_SPEED * dt;
	if (RETRO_KeyState(SDL_SCANCODE_PAGEUP)) tilt += PITCH_SPEED * dt;
	if (RETRO_KeyState(SDL_SCANCODE_PAGEDOWN)) tilt -= PITCH_SPEED * dt;
	RETRO_TurnLook(&Look, turn, tilt);
	RETRO_MouseLook(&Look);

	// Aim from a level frame facing +x; Quake's z is up
	RETRO_AimLook(&Camera, Look, { 0, -1, 0 }, { 0, 0, -1 }, { 1, 0, 0 });

	// Walking flies along the view, and strafing is level
	RETRO_MoveCamera(&Camera, walk, strafe);

	Time = time.total;
	DrawScene();
}

//
// Load the map and its palette, start where it puts the player, and take out
// what drawing it needs
//
void DEMO_Initialize(void)
{
	Map = RETRO_LoadBSP("assets/quake_start.bsp", "assets/quake_palette.lmp", "assets/quake_colormap.lmp");
	RETRO_SetPalette(Map.palette);

	// A map's angle is counterclockwise from +x, the way the heading turns.
	// The lens defaults to the whole screen, centered; a focal length of half
	// the screen's width is a view 90 degrees across.
	const RETRO_MapEntity *start = RETRO_FindMapEntity(Map.entities, "classname", "info_player_start");
	if (start == NULL) {
		RETRO_RageQuit("Map has no info_player_start\n");
	}
	RETRO_InitializeCamera(&Camera, RETRO_MapVector(*start, "origin") + vec3{ 0.0f, 0.0f, EYE_HEIGHT });
	Camera.lens.focalx = RETRO_WIDTH / 2.0f;
	Camera.lens.focaly = RETRO_WIDTH / 2.0f;
	Look.yaw = radians(RETRO_MapNumber(*start, "angle"));
	Look.sensitivity = MOUSE_SENSITIVITY;
	Look.maxpitch = radians(90);
	RETRO_SetMouseMode(true);

	// Each color shaded by each light level, from the colormap's 64 rows,
	// brightest first. Palette entries 224-255 are fullbright and keep their
	// color.
	for (int color = 0; color < 256; color++) {
		for (int light = 0; light < 256; light++) {
			int row = 63 - (light >> 2);
			LightTable[color * 256 + light] = color >= 224 ? color : Map.colormap[row * 256 + color];
		}
	}

	DecodeTextures();
	DecodeFaces();
}

void DEMO_Deinitialize(void)
{
	RETRO_FreeBSP(&Map);
	delete[] Textures;
	delete[] Corners;
	delete[] Lightmaps;
	delete[] VisibleFaces;
	delete[] FaceDistance;
	delete[] Listed;
}
