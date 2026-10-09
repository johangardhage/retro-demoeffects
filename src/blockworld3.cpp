//
// Block world 3
//
// blockworld2.cpp's world brought to life: walk, sprint and sneak over its
// hills, swim its sea or fly over it, break and place its blocks, and watch
// its water flow, its sand fall and its blocks cast shadows.
//
// The world, its textures and its colors are blockworld.cpp's, twice as
// wide and with a floor of bedrock under the dirt, and so is the drawing:
// one ray per pixel walked through the grid cell by cell and fogged toward
// the horizon. The camera is the player's eye, EYE_HEIGHT above their feet,
// turned by the mouse: yaw about the vertical and pitch up or down, short
// of straight. The mouse is read once a frame, not once a simulation step,
// so turning is as smooth as the display.
//
// The player is a box PLAYER_RADIUS to each side and PLAYER_HEIGHT tall
// that every solid block stops; air and water do not. Each step moves the
// box along x, then z, then y, and a move that ends overlapping a block is
// pushed back to that block's face, so the player slides along a wall
// rather than stopping dead. Gravity pulls the box down at GRAVITY until a
// block stops it, and only then can it jump, rising at JUMP_SPEED: high
// enough to climb one block, not two. In water gravity is weak, W and S
// swim along the view, up or down as it is tilted, holding the player up
// while they do, and Space swims up instead of jumping. Swimming rises no
// higher than the middle of the body at the surface, where the player
// floats, and while the feet are in water the player moves at SWIM_SPEED,
// swimming or not. Swimming into a wall with Space held climbs it at
// CLIMB_SPEED while the feet are in the water or just above it, and stops
// as soon as the wall is cleared, so the player steps out onto the first
// block high enough. A wall is climbed only if the player has room to stand
// on it a block above the water's surface, CLIMB_REACH past it, so against
// a higher one Space only floats them at the surface. Left Ctrl sprints,
// SPRINT_FACTOR times as fast. Space twice within DOUBLE_TAP takes off:
// flying, gravity lets go, W and S fly along the view, up or down as it is
// tilted, Space climbs and Left Shift sinks at FLY_CLIMB_SPEED, and coming
// down onto the ground lands. Left Shift on the ground sneaks: SNEAK_FACTOR
// times as fast, with the eye lowered to SNEAK_EYE_HEIGHT, and a step that
// would leave no solid block under any part of the box is not taken, so the
// player can lean out over an edge but not walk off it. In water Left Shift
// sinks instead.
//
// Water and sand move. A changed block queues itself and its six
// neighbors to be looked at again, so only what a change can reach is ever
// looked at. Every WATER_DELAY the queued water falls into air below it,
// or failing that spreads into air beside it, and never runs dry, as the
// sea it comes from does not: a hole dug beside the sea fills. Every
// SAND_DELAY the queued sand with air or water below it drops a cell,
// unless the player stands there, and leaves air behind: water it sinks
// through closes over it, and no water is ever carried up above the sea.
//
// The block under the crosshair is found with the same walk as the
// picture, cast from the eye along the view out to REACH, passing water and
// not seeing through leaves. The left button breaks it unless it is
// bedrock, and the right button places the selected block in the cell the
// ray left to reach it, unless that would put the block inside the player.
// Either acts at once on a click and again every REPEAT_DELAY while the
// button is held. The outline is drawn where a face's own coordinates come
// within OUTLINE_WIDTH pixels of its edge, a pixel at depth z along the
// view spanning z / F of a block, so the line stays one pixel wide however
// close the block is.
//
// Under water the walk passes water for the picture too, with the fog
// drawn in, from UNDERWATER_FOG_START to UNDERWATER_DISTANCE, and turned
// the water's blue, dimmed by the light of the hour, so the player sees
// through it. A ray that rises out of the water into air stops there, so
// the surface shows from below, lit as a top. The palette is laid out anew
// under water: each color's entries, six face directions of two fog steps
// on land, become two lights of UNDERWATER_FOG_SHADES steps, tops at their
// own light and the rest at the mean of the four sides', since the sun's
// direction barely shows under water but a fog that close needs every
// step it can get.
//
// The sun is a square in the sky, as blocky as the world, and goes round
// once every DAY_LENGTH seconds on a tilted circle: up out of the horizon
// along e, over the top at SUN_NOON_ELEVATION along n, down on the far
// side, and under the world for the night,
//
//   s = cos θ e + sin θ n
//
// with e and n level and tilted directions square to each other, so s
// stays a unit vector. The day keeps a clock of its own, which runs
// FAST_TIME times as fast while T is held. The moon is a smaller square at
// −s. A ray that reaches the sky is measured in the frame of a body at c,
// with a = normalize(y × c) level across it and c × a upright along it,
//
//   u = |d · a| / (d · c),  v = |d · (c × a)| / (d · c)
//
// the tangents of the angles off its center along each edge, and is in the
// body where both are under its size. The squares stand level with the
// horizon; the sun never reaches straight up, where y × c would vanish.
// Both show wherever the sky's color does, where a ray reaches nothing or
// only land fogged out of sight, below the horizon too. The world ends
// short of the horizon, so a sun cut off there would set in mid-air; this
// way it sets behind the last of the sea or land that can be seen.
//
// Stars are STAR_COUNT fixed directions spread evenly over the sphere,
// each drawn as the one pixel it projects to, so a star moves with the view
// without ever going missing or doubling. They show only where a ray
// reaches the sky, and their color fades in from the zenith's as the light
// goes, so they come out at dusk.
//
// Clouds are a flat layer at CLOUD_HEIGHT, above anything the player can
// build but not out of reach of flight. A ray meets it at
//
//   t = (CLOUD_HEIGHT − o.y) / d.y
//
// in front of the eye where t > 0, and shows it there if nothing the ray
// reaches is nearer, so from above the clouds cover the land. The point,
// shifted along x by the drift, falls in a cell of CLOUD_CELL blocks that
// is cloud where value noise over the cells is above CLOUD_COVER, so the
// cells clump into blocky clouds. They reach out to CLOUD_DISTANCE and
// cover the stars, the sun and the moon. Their color goes from NIGHT_CLOUD
// to DAY_CLOUD with the daylight, and takes half the horizon's sunset glow.
//
// The light goes by the sun's height h as the eye sees it: its angle above
// the level, plus the dip of the world's edge, which lies MAX_DISTANCE away
// and sinks below the level the higher the eye stands over the sea,
//
//   h = sin(asin(s.y) + atan((o.y − SEA_LEVEL) / MAX_DISTANCE))
//
// so that from a hilltop the sun sets later, behind the edge it is seen to
// sink behind, and the hill keeps its light the while. The direction of
// the light stays the sun's own, so the shadow rays of the valleys run
// into the land below the hill and they go dark first.
//
// Night falls through the palette, rewritten every frame: the colors keep
// their indices and only what the indices show changes. The light is mixed
// from NIGHT_LIGHT to full as h rises from NIGHT_HEIGHT, below the
// horizon, to DUSK_HEIGHT, just above it, so that the day holds
// until the sun is all but down and the dusk falls after it. The sky's two
// ends go from their night colors to their day ones alongside, and the
// horizon toward SUNSET while the sun is within SUNSET_HEIGHT of it, so
// that sunrise and sunset glow. Each of the six face directions n is lit
// by how squarely it faces the sun by day and the moon by night,
//
//   level = L (SUN_AMBIENT l + SUN_DIRECT c max(n · s, 0)
//              + MOON_DIRECT (1 − D) max(−n · s, 0))
//
// for each of red, green and blue, capped at full. D is the daylight, from
// 0 at night to 1 by day, so the moon does not shine by day, and l the
// light of the hour, which dims the light from the sky. c is the sun's own
// light, which passes through ever more air as the sun sinks: white above
// SUN_WHITE_HEIGHT, reddening toward LOW_SUN below it, weakening below
// SUN_FADE_HEIGHT and gone at the horizon, so a low sun lights the faces
// toward it orange and the sky alone lights the world once it is down. L is
// the face's own light, lower for the sides along z than along x, so that
// a block's edges read even where the sun lights two faces alike, and a
// bottom's matching the sides along x. Every direction has colors of its
// own, so the side of a block toward the sun or the moon is brighter than
// the side away from it.
//
// Blocks cast shadows. From where a ray meets a face turned toward the sun,
// a second walk sets out toward the sun from just inside the face, so its
// first step crosses the face and leaves against it are tested like any
// other, through water and leaf holes as light goes, for SHADOW_DISTANCE; a
// block on the way leaves the face with the sky's light alone. That is the
// light of the face opposite it, which the sun does not reach, and of the
// same face light L, so a face in shadow takes the opposite face's colors,
// and shadows need no colors of their own. A top in shadow takes a
// bottom's, whose light is set to match the sides along x for it. The moon
// casts none, and under water, where the palette has no such pairs, none
// are cast.
//
// Blocks are their colors at the level, fogged toward the horizon's color
// of the moment as smoothstep(FOG_START, MAX_DISTANCE, t) of the distance t
// along the ray, so the fog holds off until FOG_START. Six directions of
// every color leave room in the palette for two fog steps, and the fog,
// like the sky, is rounded to its nearest step rather than dithered between
// them. The water's light streaks pulse between SHIMMER_LOW and
// SHIMMER_HIGH of the way from its dark blue to its light one, faint
// against the rest, so the sea shimmers. The hotbar keeps a copy of the
// block colors that the night does not touch, so it reads in the dark.
//
// Controls: the mouse looks, W A S D walk, Left Ctrl sprints, Left Shift
// sneaks, Space jumps or swims up, Space twice takes off or lands, Space
// and Left Shift climb and sink while flying or swimming, the left button
// breaks, the right button places, both repeating while held, 1 to 5 pick
// the block to place from the hotbar, and T hurries the day.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrogfx.h"
#include "lib/retromath.h"
#include "lib/retromouse.h"
#include "lib/retrocamera.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define WORLD_SIZE 512
#define WORLD_HEIGHT 48
#define SEA_LEVEL 14
#define GROUND_BASE 4 // the lowest the ground goes
#define GROUND_RANGE 30 // blocks from the lowest ground to the highest
#define NOISE_SCALE 48.0f // blocks per feature of the coarsest octave

#define TREE_CELL 7 // at most one tree to a square this many blocks wide
#define TREE_CHANCE 0.55f // of a square on grass having its tree
#define TREE_MIN_TRUNK 4
#define TREE_MAX_TRUNK 6

#define FOCAL 200.0f // pixels; sets the field of view
#define MAX_DISTANCE 128.0f // blocks a ray walks before it sees the sky
#define FOG_START 80.0f
#define UNDERWATER_DISTANCE 32.0f // and the same under water
#define UNDERWATER_FOG_START 4.0f

#define PLAYER_RADIUS 0.3f
#define PLAYER_HEIGHT 1.8f
#define EYE_HEIGHT 1.62f
#define SNEAK_EYE_HEIGHT 1.27f // the eye, lowered while sneaking
#define EYE_SPEED 4.0f // blocks a second the eye moves between the two
#define SNEAK_FACTOR 0.3f // times as fast while sneaking
#define WALK_SPEED 4.3f // blocks a second
#define SWIM_SPEED 2.0f
#define GRAVITY 32.0f // blocks a second, a second
#define WATER_GRAVITY 4.0f
#define JUMP_SPEED 9.0f // rises 9² / (2 × 32) = 1.27 blocks
#define SWIM_UP_SPEED 3.0f
#define CLIMB_SPEED 4.0f // blocks a second, climbing out of the water
#define CLIMB_REACH 0.1f // blocks past the wall a climb looks for room to stand
#define SPRINT_FACTOR 1.6f // times as fast while Left Ctrl is held
#define FLY_SPEED 10.9f // blocks a second, flying
#define FLY_CLIMB_SPEED 7.5f // blocks a second up or down, flying
#define DOUBLE_TAP 0.3 // seconds within which a second Space takes off or lands
#define MAX_FALL_SPEED 40.0f // two thirds of a block a step, so a fall never passes through one
#define CONTACT_GAP 0.001f // left between the box and a block it is pushed back from
#define REACH 5.0f // blocks from the eye a block can be broken or placed
#define SHADOW_DISTANCE 32.0f // blocks a shadow ray walks toward the sun
#define SHADOW_OFFSET 0.001f // into the face, so a shadow ray's first step crosses it
#define REPEAT_DELAY 0.25 // seconds between breaks or placings while a button is held

#define WATER_DELAY 0.25 // seconds between steps of flowing water
#define SAND_DELAY 0.05 // seconds between the cells falling sand drops
#define QUEUE_SIZE 65536 // cells waiting for a step of water or sand

#define DAY_LENGTH 240.0 // seconds for the sun to go round once
#define DAY_START 0.45 // radians round from sunrise at the start, mid morning
#define FAST_TIME 20.0 // times as fast as the day runs while T is held
#define SUNRISE_AZIMUTH 0.5f // radians around from +x toward +z
#define SUN_NOON_ELEVATION 1.1f // radians above the horizon at the top
#define SUN_SIZE 0.08f // the tangent of half the angle across
#define SUN_CORE_SIZE 0.6f // of the sun that is its pale core
#define MOON_SIZE 0.06f
#define NIGHT_LIGHT 0.22f // of the day's light left at night
#define DUSK_HEIGHT 0.05f // the sun's height h at which the light begins to fail
#define NIGHT_HEIGHT -0.25f // and at which it is night
#define SUNSET_HEIGHT 0.2f // the sun's height either side of the horizon within which it glows
#define SUN_AMBIENT 0.75f // of a face's light it has facing away from the sun
#define SUN_DIRECT 0.4f // and the most the sun adds to it, facing it square on
#define MOON_DIRECT 0.2f // and the moon at night
#define SUN_FADE_HEIGHT 0.15f // the sun's height h under which its direct light weakens, gone at the horizon
#define SUN_WHITE_HEIGHT 0.35f // and under which it reddens toward LOW_SUN
#define STAR_COUNT 320 // over the whole sphere, half of them above the horizon

#define CLOUD_HEIGHT 60.0f // above the highest block the player can stand on
#define CLOUD_CELL 8.0f // blocks across a cloud cell
#define CLOUD_SCALE 0.3f // noise features per cell, the size of a cloud
#define CLOUD_COVER 0.6f // noise above which a cell is cloud
#define CLOUD_SPEED 1.5 // blocks a second, drifting along x
#define CLOUD_DISTANCE 200.0f // past which the cloud layer is sky

#define SHIMMER_SPEED 2.5 // radians a second
#define SHIMMER_LOW 0.4f // of the way from the water's dark blue to its light one
#define SHIMMER_HIGH 0.8f // that the streaks pulse between
#define OUTLINE_WIDTH 1.0f // pixels

#define SLOT_SIZE 20 // a texture and a two-pixel frame

#define TEXTURE_SIZE 16
#define TEXEL_HOLE 255 // a leaves texel the ray passes through

enum Block { AIR, GRASS, DIRT, SAND, WATER, LOG, LEAVES, BEDROCK, BLOCKS };

enum Texture { TEX_GRASS_TOP, TEX_GRASS_SIDE, TEX_DIRT, TEX_SAND, TEX_WATER, TEX_LOG_SIDE, TEX_LOG_TOP, TEX_LEAVES, TEX_BEDROCK, TEXTURES };

enum Color { GRASS1, GRASS2, GRASS3, DIRT1, DIRT2, DIRT3, BARK1, BARK2, WOOD1, WOOD2, LEAF1, LEAF2, SAND1, SAND2, WATER1, WATER2, BEDROCK1, BEDROCK2, COLORS };

enum Light { LIGHT_TOP, LIGHT_BOTTOM, LIGHT_POS_X, LIGHT_NEG_X, LIGHT_POS_Z, LIGHT_NEG_Z, LIGHTS };

static const RETRO_Palette ColorTable[COLORS] = {
	{ 74, 122, 38 }, { 96, 156, 52 }, { 122, 186, 70 },
	{ 102, 70, 44 }, { 134, 96, 63 }, { 158, 118, 82 },
	{ 68, 52, 30 }, { 104, 82, 48 },
	{ 160, 130, 80 }, { 186, 156, 100 },
	{ 44, 100, 28 }, { 66, 134, 40 },
	{ 210, 196, 140 }, { 226, 214, 162 },
	{ 44, 84, 186 }, { 60, 104, 206 },
	{ 36, 36, 36 }, { 92, 92, 92 },
};

static const float LightLevel[LIGHTS] = { 1.0f, 0.8f, 0.8f, 0.8f, 0.62f, 0.62f };

// The way each face direction points, and the direction opposite, whose
// colors a face in shadow takes
static const vec3 FaceNormal[LIGHTS] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
static const int Opposite[LIGHTS] = { LIGHT_BOTTOM, LIGHT_TOP, LIGHT_NEG_X, LIGHT_POS_X, LIGHT_NEG_Z, LIGHT_POS_Z };

// The textures of each block's top, sides and bottom
static const int BlockTexture[BLOCKS][3] = {
	{ 0, 0, 0 }, // AIR, never drawn
	{ TEX_GRASS_TOP, TEX_GRASS_SIDE, TEX_DIRT },
	{ TEX_DIRT, TEX_DIRT, TEX_DIRT },
	{ TEX_SAND, TEX_SAND, TEX_SAND },
	{ TEX_WATER, TEX_WATER, TEX_WATER },
	{ TEX_LOG_TOP, TEX_LOG_SIDE, TEX_LOG_TOP },
	{ TEX_LEAVES, TEX_LEAVES, TEX_LEAVES },
	{ TEX_BEDROCK, TEX_BEDROCK, TEX_BEDROCK },
};

#define DAY_HORIZON (RETRO_Palette){ 180, 210, 240 }
#define DAY_ZENITH (RETRO_Palette){ 70, 120, 215 }
#define NIGHT_HORIZON (RETRO_Palette){ 24, 30, 56 }
#define NIGHT_ZENITH (RETRO_Palette){ 6, 8, 24 }
#define SUNSET (RETRO_Palette){ 244, 140, 72 }
#define SUNSET_STRENGTH 0.75f
#define LOW_SUN (RETRO_Palette){ 255, 140, 70 } // the sun's direct light at the horizon
#define WATER_FOG (RETRO_Palette){ 24, 56, 130 }
#define DAY_CLOUD (RETRO_Palette){ 242, 244, 248 }
#define NIGHT_CLOUD (RETRO_Palette){ 34, 38, 58 }
#define STAR (RETRO_Palette){ 230, 232, 255 }

#define SKY_START 0
#define SKY_SHADES 15
#define FOG_SHADES 2
#define UNDERWATER_LIGHTS 2 // tops, and every other face
#define UNDERWATER_FOG_SHADES (LIGHTS * FOG_SHADES / UNDERWATER_LIGHTS)
#define BLOCK_START (SKY_START + SKY_SHADES)
#define HOTBAR_START (BLOCK_START + COLORS * LIGHTS * FOG_SHADES)
#define OUTLINE_COLOR (HOTBAR_START + COLORS)
#define CROSSHAIR_COLOR (OUTLINE_COLOR + 1)
#define SUN_RIM_COLOR (OUTLINE_COLOR + 2)
#define SUN_CORE_COLOR (OUTLINE_COLOR + 3)
#define MOON_COLOR (OUTLINE_COLOR + 4)
#define CLOUD_COLOR (OUTLINE_COLOR + 5)
#define STAR_COLOR (OUTLINE_COLOR + 6)
#define OUTLINE (RETRO_Palette){ 20, 20, 20 }
#define CROSSHAIR (RETRO_Palette){ 240, 240, 240 }
#define SUN_RIM (RETRO_Palette){ 255, 226, 110 }
#define SUN_CORE (RETRO_Palette){ 255, 250, 210 }
#define MOON (RETRO_Palette){ 214, 220, 232 }

static unsigned char World[WORLD_HEIGHT][WORLD_SIZE][WORLD_SIZE];
static unsigned char Textures[TEXTURES][TEXTURE_SIZE][TEXTURE_SIZE];

// The smooth height the ground is cut from, in blocks
static float GroundHeight(float x, float z)
{
	float noise = 0.55f * RETRO_ValueNoise(x / NOISE_SCALE, z / NOISE_SCALE)
		+ 0.3f * RETRO_ValueNoise(x * 2 / NOISE_SCALE + 17, z * 2 / NOISE_SCALE + 31)
		+ 0.15f * RETRO_ValueNoise(x * 4 / NOISE_SCALE + 53, z * 4 / NOISE_SCALE + 7);
	return GROUND_BASE + GROUND_RANGE * smoothstep(0.2f, 0.85f, noise);
}

static void SetBlock(int x, int y, int z, int block)
{
	if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_HEIGHT || z < 0 || z >= WORLD_SIZE) return;
	if (block == LEAVES && World[y][z][x] != AIR) return;
	World[y][z][x] = block;
}

static void PlantTree(int x, int y, int z, int trunk)
{
	int top = y + trunk;
	for (int ly = top - 2; ly <= top + 1; ly++) {
		int radius = ly < top ? 2 : 1;
		for (int dz = -radius; dz <= radius; dz++) {
			for (int dx = -radius; dx <= radius; dx++) {
				bool corner = abs(dx) == radius && abs(dz) == radius;
				if (corner && (ly == top + 1 || RETRO_HashUnit(x + dx, z + dz, ly) < 0.6f)) continue;
				SetBlock(x + dx, ly, z + dz, LEAVES);
			}
		}
	}
	for (int ly = y; ly < top; ly++) {
		World[ly][z][x] = LOG;
	}
}

static void CreateWorld(void)
{
	for (int z = 0; z < WORLD_SIZE; z++) {
		for (int x = 0; x < WORLD_SIZE; x++) {
			int h = (int)GroundHeight(x, z);
			int top = h > SEA_LEVEL + 1 ? GRASS : SAND;
			World[0][z][x] = BEDROCK;
			for (int y = 1; y < h; y++) World[y][z][x] = DIRT;
			World[h][z][x] = top;
			for (int y = h + 1; y <= SEA_LEVEL; y++) World[y][z][x] = WATER;
		}
	}

	for (int cz = 0; cz < WORLD_SIZE / TREE_CELL; cz++) {
		for (int cx = 0; cx < WORLD_SIZE / TREE_CELL; cx++) {
			// A hash stream each for whether the cell has a tree, where in the
			// cell it stands, and how tall it grows
			if (RETRO_HashUnit(cx, cz, 0) > TREE_CHANCE) continue;
			int x = cx * TREE_CELL + 2 + RETRO_Hash(cx, cz, 1) % (TREE_CELL - 4);
			int z = cz * TREE_CELL + 2 + RETRO_Hash(cx, cz, 2) % (TREE_CELL - 4);
			int h = (int)GroundHeight(x, z);
			if (World[h][z][x] != GRASS) continue;
			int trunk = TREE_MIN_TRUNK + RETRO_Hash(cx, cz, 3) % (TREE_MAX_TRUNK - TREE_MIN_TRUNK + 1);
			if (h + trunk + 2 >= WORLD_HEIGHT) continue;
			PlantTree(x, h + 1, z, trunk);
		}
	}
}

// One of n colors from first on, picked by a texel's hash. Each texture hashes
// in a stream of its own, and a hash a whole column shares takes the row
// above the texture, -1, which no texel uses.
static unsigned char PickColor(int texture, int x, int y, int first, int n)
{
	return first + RETRO_Hash(x, y, texture) % n;
}

static void CreateTextures(void)
{
	for (int y = 0; y < TEXTURE_SIZE; y++) {
		for (int x = 0; x < TEXTURE_SIZE; x++) {
			Textures[TEX_GRASS_TOP][y][x] = PickColor(TEX_GRASS_TOP, x, y, GRASS1, 3);
			Textures[TEX_DIRT][y][x] = PickColor(TEX_DIRT, x, y, DIRT1, 3);
			Textures[TEX_SAND][y][x] = PickColor(TEX_SAND, x, y, SAND1, 2);

			// Bedrock is mottled in two-texel blotches
			Textures[TEX_BEDROCK][y][x] = PickColor(TEX_BEDROCK, x / 2, y / 2, BEDROCK1, 2);

			// Grass hangs a ragged two to four texels over the dirt
			int fringe = 2 + RETRO_Hash(x, -1, TEX_GRASS_SIDE) % 3;
			Textures[TEX_GRASS_SIDE][y][x] = y < fringe ? PickColor(TEX_GRASS_SIDE, x, y, GRASS1, 3) : PickColor(TEX_GRASS_SIDE, x, y, DIRT1, 3);

			// Water is mostly the darker blue, with short light streaks
			Textures[TEX_WATER][y][x] = RETRO_Hash(x / 3, y, TEX_WATER) % 5 == 0 ? WATER2 : WATER1;

			// Bark runs in vertical stripes, a little broken up
			bool stripe = RETRO_Hash(x, -1, TEX_LOG_SIDE) % 3 == 0;
			bool knot = RETRO_Hash(x, y, TEX_LOG_SIDE) % 7 == 0;
			Textures[TEX_LOG_SIDE][y][x] = stripe != knot ? BARK1 : BARK2;

			// The cut end shows bark around the edge and growth rings inside
			int ring = (int)MAX(fabsf(x - 7.5f), fabsf(y - 7.5f));
			Textures[TEX_LOG_TOP][y][x] = ring == 7 ? BARK1 : (ring & 1 ? WOOD1 : WOOD2);

			float leaf = RETRO_HashUnit(x, y, TEX_LEAVES);
			Textures[TEX_LEAVES][y][x] = leaf < 0.2f ? TEXEL_HOLE : (leaf < 0.6f ? LEAF1 : LEAF2);
		}
	}
}

// The blocks 1 to 5 select
static const int Hotbar[] = { GRASS, DIRT, SAND, LOG, LEAVES };
#define HOTBAR_SLOTS (int)(sizeof(Hotbar) / sizeof(Hotbar[0]))

// What a walk through the grid found
struct RayHit {
	int block;
	int x, y, z; // the cell hit
	int lastx, lasty, lastz; // the cell the ray left to reach it
	int light; // the way the face points, as a Light
	float u, v; // where across the face, from 0 to 1
	int texel;
	float t;
};

static vec3 Position; // the player's feet
static float VelocityY;
static RETRO_Look Look;

// The world's level frame, which the look turns: facing +x at a yaw of zero,
// with y up
static const vec3 LevelRight = { 0, 0, -1 };
static const vec3 LevelDown = { 0, -1, 0 };
static const vec3 LevelForward = { 1, 0, 0 };
static bool OnGround;
static int Selected;
static float EyeHeight = EYE_HEIGHT;

// Cells for a step of flowing water or falling sand to look at, each queued
// once at most: queued marks it in the cells' flags with the queue's bit
struct UpdateQueue {
	int cells[QUEUE_SIZE];
	int count;
	unsigned char bit;
};

static UpdateQueue WaterQueue = { {}, 0, 1 };
static UpdateQueue SandQueue = { {}, 0, 2 };
static unsigned char Queued[WORLD_HEIGHT][WORLD_SIZE][WORLD_SIZE];
static vec3 Stars[STAR_COUNT];

// The block at a point, with air outside the world
static int BlockAt(vec3 p)
{
	int x = (int)floorf(p.x), y = (int)floorf(p.y), z = (int)floorf(p.z);
	if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_HEIGHT || z < 0 || z >= WORLD_SIZE) return AIR;
	return World[y][z][x];
}

// Walks the ray cell by cell until it enters a block other than clear, or
// air, or walks maxdistance or leaves the world. With holes, a leaves texel
// that is a hole lets the ray walk on. With surface, a ray rising out of
// water into air stops there, at the surface seen from below.
static bool WalkRay(vec3 origin, vec3 dir, float maxdistance, int clear, bool holes, bool surface, RayHit &hit)
{
	int x = (int)floorf(origin.x), y = (int)floorf(origin.y), z = (int)floorf(origin.z);
	int from = BlockAt(origin);
	int stepx = dir.x < 0 ? -1 : 1, stepy = dir.y < 0 ? -1 : 1, stepz = dir.z < 0 ? -1 : 1;

	// t across one cell along each axis, and t to the first plane crossed
	float deltax = fabsf(1.0f / dir.x), deltay = fabsf(1.0f / dir.y), deltaz = fabsf(1.0f / dir.z);
	float nextx = (dir.x < 0 ? origin.x - x : x + 1 - origin.x) * deltax;
	float nexty = (dir.y < 0 ? origin.y - y : y + 1 - origin.y) * deltay;
	float nextz = (dir.z < 0 ? origin.z - z : z + 1 - origin.z) * deltaz;

	for (;;) {
		hit.lastx = x;
		hit.lasty = y;
		hit.lastz = z;

		// The face entered points back against the step
		float t;
		int light;
		if (nextx < nexty && nextx < nextz) {
			x += stepx;
			t = nextx;
			nextx += deltax;
			light = stepx > 0 ? LIGHT_NEG_X : LIGHT_POS_X;
		} else if (nexty < nextz) {
			y += stepy;
			t = nexty;
			nexty += deltay;
			light = stepy > 0 ? LIGHT_BOTTOM : LIGHT_TOP;
		} else {
			z += stepz;
			t = nextz;
			nextz += deltaz;
			light = stepz > 0 ? LIGHT_NEG_Z : LIGHT_POS_Z;
		}

		if (t > maxdistance) return false;
		if (x < 0 || x >= WORLD_SIZE || z < 0 || z >= WORLD_SIZE || y < 0) return false;
		if (y >= WORLD_HEIGHT) {
			if (stepy > 0) return false;
			continue;
		}

		// The surface from below is lit as a top: it is the sunlit water
		int block = World[y][z][x];
		bool rising = surface && block == AIR && from == WATER && light == LIGHT_BOTTOM;
		from = block;
		if (rising) {
			block = WATER;
			light = LIGHT_TOP;
		} else if (block == AIR || block == clear) {
			continue;
		}

		// Where the ray meets the face, as texture coordinates across it
		vec3 p = origin + dir * t;
		float u, v;
		int side;
		if (light == LIGHT_TOP || light == LIGHT_BOTTOM) {
			u = p.x - x;
			v = p.z - z;
			side = light == LIGHT_TOP ? 0 : 2;
		} else if (light == LIGHT_POS_X || light == LIGHT_NEG_X) {
			u = p.z - z;
			v = y + 1 - p.y;
			side = 1;
		} else {
			u = p.x - x;
			v = y + 1 - p.y;
			side = 1;
		}
		int tu = CLAMP((int)(u * TEXTURE_SIZE), 0, TEXTURE_SIZE);
		int tv = CLAMP((int)(v * TEXTURE_SIZE), 0, TEXTURE_SIZE);
		int texel = Textures[BlockTexture[block][side]][tv][tu];
		if (holes && texel == TEXEL_HOLE) continue;

		hit.block = block;
		hit.x = x;
		hit.y = y;
		hit.z = z;
		hit.light = light;
		hit.u = u;
		hit.v = v;
		hit.texel = texel;
		hit.t = t;
		return true;
	}
}

// Whether a cell stops the player. The world's edges and floor do, and the
// sky above it does not.
static bool Solid(int x, int y, int z)
{
	if (x < 0 || x >= WORLD_SIZE || z < 0 || z >= WORLD_SIZE || y < 0) return true;
	if (y >= WORLD_HEIGHT) return false;
	int block = World[y][z][x];
	return block != AIR && block != WATER;
}

// Whether the player's box, standing at p, overlaps a solid cell
static bool Collides(vec3 p)
{
	int x1 = (int)floorf(p.x - PLAYER_RADIUS), x2 = (int)floorf(p.x + PLAYER_RADIUS);
	int z1 = (int)floorf(p.z - PLAYER_RADIUS), z2 = (int)floorf(p.z + PLAYER_RADIUS);
	int y1 = (int)floorf(p.y), y2 = (int)floorf(p.y + PLAYER_HEIGHT);
	for (int y = y1; y <= y2; y++) {
		for (int z = z1; z <= z2; z++) {
			for (int x = x1; x <= x2; x++) {
				if (Solid(x, y, z)) return true;
			}
		}
	}
	return false;
}

// Whether a solid cell lies under any part of the player's box, standing at p
static bool Supported(vec3 p)
{
	int x1 = (int)floorf(p.x - PLAYER_RADIUS), x2 = (int)floorf(p.x + PLAYER_RADIUS);
	int z1 = (int)floorf(p.z - PLAYER_RADIUS), z2 = (int)floorf(p.z + PLAYER_RADIUS);
	int y = (int)floorf(p.y) - 1;
	for (int z = z1; z <= z2; z++) {
		for (int x = x1; x <= x2; x++) {
			if (Solid(x, y, z)) return true;
		}
	}
	return false;
}

// Whether a wall the player swims into can be climbed: a little further
// along the move, the box fits standing a block above the water's surface
static bool Climbable(vec3 p, vec3 move)
{
	int y = (int)floorf(p.y);
	if (BlockAt(vec3{ p.x, y + 0.5f, p.z }) != WATER) y--;
	while (BlockAt(vec3{ p.x, y + 1.5f, p.z }) == WATER) y++;
	vec3 ahead = normalize(vec3{ move.x, 0, move.z }) * CLIMB_REACH;
	return !Collides(vec3{ p.x + ahead.x, y + 2 + CONTACT_GAP, p.z + ahead.z });
}

// Where the look faces, tilted up or down
static vec3 ViewDirection(void)
{
	RETRO_Camera view;
	RETRO_AimLook(&view, Look, LevelRight, LevelDown, LevelForward);
	return view.forward;
}

// How far dir is off the center of a sun or moon at center, as the larger
// of the tangents along the square's two edges
static float BodyExtent(vec3 dir, vec3 center)
{
	float facing = dot(dir, center);
	if (facing <= 0) return INFINITY;
	vec3 across = normalize(cross(vec3{ 0, 1, 0 }, center));
	vec3 along = cross(center, across);
	return MAX(fabsf(dot(dir, across)), fabsf(dot(dir, along))) / facing;
}

// The sky seen along dir, the nearest shade of its gradient
static unsigned char SkyColor(vec3 dir)
{
	float level = CLAMP01(dir.y * 2.5f);
	return SKY_START + (int)(level * (SKY_SHADES - 1) + 0.5f);
}

// A color lit by light, a level from 0 to 1 for each channel
static RETRO_Palette LightColor(RETRO_Palette color, vec3 light)
{
	return { (unsigned char)(color.r * light.x + 0.5f), (unsigned char)(color.g * light.y + 0.5f), (unsigned char)(color.b * light.z + 0.5f) };
}

static unsigned char BlockColor(int texel, int light, int fogstep)
{
	return BLOCK_START + (texel * LIGHTS + light) * FOG_SHADES + fogstep;
}

// The same entries under water, laid out as UNDERWATER_LIGHTS lights of
// UNDERWATER_FOG_SHADES fog steps
static unsigned char UnderwaterColor(int texel, int light, int fogstep)
{
	return BLOCK_START + (texel * UNDERWATER_LIGHTS + light) * UNDERWATER_FOG_SHADES + fogstep;
}

static void DrawCrosshair(void)
{
	int cx = RETRO_WIDTH / 2, cy = RETRO_HEIGHT / 2;
	RETRO_DrawHline(cx - 4, cx + 4, cy, CROSSHAIR_COLOR);
	RETRO_DrawVline(cx, cy - 4, cy + 4, CROSSHAIR_COLOR);
}

// A slot per hotbar block, showing its side, the selected one framed
static void DrawHotbar(void)
{
	int left = (RETRO_WIDTH - HOTBAR_SLOTS * SLOT_SIZE) / 2;
	int top = RETRO_HEIGHT - SLOT_SIZE - 4;
	for (int slot = 0; slot < HOTBAR_SLOTS; slot++) {
		int x0 = left + slot * SLOT_SIZE, y0 = top;
		RETRO_DrawRectangle(x0, y0, x0 + SLOT_SIZE - 1, y0 + SLOT_SIZE - 1, slot == Selected ? CROSSHAIR_COLOR : OUTLINE_COLOR);
		int texture = BlockTexture[Hotbar[slot]][1];
		for (int ty = 0; ty < TEXTURE_SIZE; ty++) {
			for (int tx = 0; tx < TEXTURE_SIZE; tx++) {
				int texel = Textures[texture][ty][tx];
				unsigned char color = texel == TEXEL_HOLE ? OUTLINE_COLOR : HOTBAR_START + texel;
				RETRO_PutPixel(x0 + 2 + tx, y0 + 2 + ty, color);
			}
		}
	}
}

static void Enqueue(UpdateQueue &queue, int x, int y, int z)
{
	if (x < 0 || x >= WORLD_SIZE || y < 0 || y >= WORLD_HEIGHT || z < 0 || z >= WORLD_SIZE) return;
	if ((Queued[y][z][x] & queue.bit) || queue.count == QUEUE_SIZE) return;
	Queued[y][z][x] |= queue.bit;
	queue.cells[queue.count++] = (y * WORLD_SIZE + z) * WORLD_SIZE + x;
}

// A block has changed: the water beside it may flow and the sand above it
// may fall, so the cell and its six neighbors are looked at again
static void BlockChanged(int x, int y, int z)
{
	static const int Neighbor[7][3] = { { 0, 0, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for (const int *d : Neighbor) {
		Enqueue(WaterQueue, x + d[0], y + d[1], z + d[2]);
		Enqueue(SandQueue, x + d[0], y + d[1], z + d[2]);
	}
}

static void SetAndUpdate(int x, int y, int z, int block)
{
	World[y][z][x] = block;
	BlockChanged(x, y, z);
}

// One step of a queue: each cell queued before it began is taken off and
// handed to step, and what step queues waits for the next
static void StepQueue(UpdateQueue &queue, void (*step)(int x, int y, int z))
{
	int count = queue.count;
	for (int i = 0; i < count; i++) {
		int cell = queue.cells[i];
		int x = cell % WORLD_SIZE, z = cell / WORLD_SIZE % WORLD_SIZE, y = cell / (WORLD_SIZE * WORLD_SIZE);
		Queued[y][z][x] &= ~queue.bit;
		step(x, y, z);
	}
	queue.count -= count;
	memmove(queue.cells, queue.cells + count, queue.count * sizeof(int));
}

// Water falls into air below it, or failing that spreads into air beside
// it, and never runs dry, as the sea it comes from does not
static void FlowWater(int x, int y, int z)
{
	if (World[y][z][x] != WATER) return;
	if (y > 0 && World[y - 1][z][x] == AIR) {
		SetAndUpdate(x, y - 1, z, WATER);
		return;
	}
	static const int Side[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	for (const int *d : Side) {
		int nx = x + d[0], nz = z + d[1];
		if (nx >= 0 && nx < WORLD_SIZE && nz >= 0 && nz < WORLD_SIZE && World[y][nz][nx] == AIR) SetAndUpdate(nx, y, nz, WATER);
	}
}

// Sand with air or water below it drops a cell and leaves air behind, which
// the water around fills if it sank through water. A cell the player
// stands in holds it up, and it is looked at again until they move
static void FallSand(int x, int y, int z)
{
	if (World[y][z][x] != SAND || y == 0) return;
	int below = World[y - 1][z][x];
	if (below != AIR && below != WATER) return;
	World[y - 1][z][x] = SAND;
	if (Collides(Position)) {
		World[y - 1][z][x] = below;
		Enqueue(SandQueue, x, y, z);
		return;
	}
	BlockChanged(x, y - 1, z);
	SetAndUpdate(x, y, z, AIR);
}

void DEMO_FixedUpdate(RETRO_Time time)
{
	float dt = time.delta;

	// The water and the sand step at their own pace
	static double nextwater = 0, nextsand = 0;
	if (time.total >= nextwater) {
		nextwater = time.total + WATER_DELAY;
		StepQueue(WaterQueue, FlowWater);
	}
	if (time.total >= nextsand) {
		nextsand = time.total + SAND_DELAY;
		StepQueue(SandQueue, FallSand);
	}

	// Space twice in quick succession takes off, or lands
	static bool flying = false;
	static double lastspace = -1;
	if (RETRO_KeyPressed(SDL_SCANCODE_SPACE)) {
		if (lastspace >= 0 && time.total - lastspace < DOUBLE_TAP) {
			flying = !flying;
			VelocityY = 0;
			lastspace = -1;
		} else {
			lastspace = time.total;
		}
	}

	for (int slot = 0; slot < HOTBAR_SLOTS; slot++) {
		if (RETRO_KeyPressed((SDL_Scancode)(SDL_SCANCODE_1 + slot))) Selected = slot;
	}

	// Walk along the ground, whichever way the player looks, or fly or swim
	// along the view, up or down as it is tilted
	bool swimming = !flying && BlockAt(Position + vec3{ 0, PLAYER_HEIGHT / 2, 0 }) == WATER;
	vec3 forward = flying || swimming ? ViewDirection() : vec3{ cosf(Look.yaw), 0, sinf(Look.yaw) };
	vec3 right = { sinf(Look.yaw), 0, -cosf(Look.yaw) };
	vec3 move = { 0, 0, 0 };
	if (RETRO_KeyState(SDL_SCANCODE_W)) move = move + forward;
	if (RETRO_KeyState(SDL_SCANCODE_S)) move = move - forward;
	if (RETRO_KeyState(SDL_SCANCODE_D)) move = move + right;
	if (RETRO_KeyState(SDL_SCANCODE_A)) move = move - right;
	if (length(move) > 0) move = normalize(move);

	bool space = RETRO_KeyState(SDL_SCANCODE_SPACE);
	bool shift = RETRO_KeyState(SDL_SCANCODE_LSHIFT);
	bool sneaking = shift && !flying && !swimming;
	if (flying) {
		VelocityY = (space ? FLY_CLIMB_SPEED : 0.0f) - (shift ? FLY_CLIMB_SPEED : 0.0f);
	} else if (swimming) {
		VelocityY = MAX(VelocityY - WATER_GRAVITY * dt, -SWIM_UP_SPEED);
		if (length(move) > 0) VelocityY = 0; // a stroke holds the player up, so they swim where they look
		if (space) VelocityY = SWIM_UP_SPEED;
		if (shift) VelocityY = -SWIM_UP_SPEED;
	} else {
		VelocityY = MAX(VelocityY - GRAVITY * dt, -MAX_FALL_SPEED);
		if (space && OnGround) VelocityY = JUMP_SPEED;
	}
	// The feet in water hold the player to swimming speed, so bobbing at the
	// surface, in and out of swimming, never makes it a walk
	bool wet = !flying && (swimming || BlockAt(Position) == WATER);
	float speed = flying ? FLY_SPEED : (wet ? SWIM_SPEED : WALK_SPEED);
	if (sneaking) {
		speed *= SNEAK_FACTOR;
	} else if (RETRO_KeyState(SDL_SCANCODE_LCTRL)) {
		speed *= SPRINT_FACTOR;
	}
	move = move * (speed * dt);

	// The eye sinks while sneaking and rises again after
	float eyetarget = sneaking ? SNEAK_EYE_HEIGHT : EYE_HEIGHT;
	EyeHeight = EyeHeight < eyetarget ? MIN(EyeHeight + EYE_SPEED * dt, eyetarget) : MAX(EyeHeight - EYE_SPEED * dt, eyetarget);

	// Sneaking on the ground, a step that would leave nothing under the feet
	// is not taken, so the player can lean out over an edge but not fall off
	bool guarded = sneaking && OnGround;

	// One axis at a time, each pushed back to the face of the block it ran into
	bool blocked = false;
	Position.x += move.x;
	if (Collides(Position)) {
		blocked = true;
		Position.x = move.x > 0 ? floorf(Position.x + PLAYER_RADIUS) - PLAYER_RADIUS - CONTACT_GAP : floorf(Position.x - PLAYER_RADIUS) + 1 + PLAYER_RADIUS + CONTACT_GAP;
	} else if (guarded && !Supported(Position)) {
		Position.x -= move.x;
	}
	Position.z += move.z;
	if (Collides(Position)) {
		blocked = true;
		Position.z = move.z > 0 ? floorf(Position.z + PLAYER_RADIUS) - PLAYER_RADIUS - CONTACT_GAP : floorf(Position.z - PLAYER_RADIUS) + 1 + PLAYER_RADIUS + CONTACT_GAP;
	} else if (guarded && !Supported(Position)) {
		Position.z -= move.z;
	}

	// Swimming into a wall with Space held climbs it, if it can be climbed,
	// steadily while the feet are in the water or just above it, and stops
	// as soon as the wall is cleared, so the player steps out onto the first
	// block high enough
	bool wading = BlockAt(Position) == WATER || BlockAt(Position - vec3{ 0, 1, 0 }) == WATER;
	bool climbing = !flying && space && blocked && wading && Climbable(Position, move);
	if (climbing) VelocityY = CLIMB_SPEED;

	OnGround = false;
	float rise = VelocityY * dt + move.y;

	// Swimming lifts the middle of the body no higher than the surface, so
	// the player floats there rather than bobbing in and out of the water,
	// and only a climb takes them out of it
	if (swimming && !climbing && rise > 0 && BlockAt(Position + vec3{ 0, PLAYER_HEIGHT / 2 + 1, 0 }) != WATER) {
		float middle = Position.y + PLAYER_HEIGHT / 2;
		float surface = floorf(middle) + 1 - CONTACT_GAP;
		if (middle + rise > surface) {
			rise = MAX(surface - middle, 0.0f);
			VelocityY = 0;
		}
	}
	Position.y += rise;
	if (Collides(Position)) {
		if (rise < 0) {
			Position.y = floorf(Position.y) + 1;
			OnGround = true;
		} else {
			Position.y = floorf(Position.y + PLAYER_HEIGHT) - PLAYER_HEIGHT - CONTACT_GAP;
		}
		VelocityY = 0;
	}

	// Coming down onto the ground lands
	if (OnGround) flying = false;
}

void DEMO_Render(RETRO_Time time)
{
	// Look, once a frame so that turning is as smooth as the display
	RETRO_MouseState mouse = RETRO_GetMouseState();
	RETRO_MouseLook(&Look, mouse);

	// Aim from the level frame, facing +x with y up
	RETRO_Camera camera;
	camera.pos = Position + vec3{ 0, EyeHeight, 0 };
	camera.lens.focalx = FOCAL;
	camera.lens.focaly = FOCAL;
	RETRO_AimLook(&camera, Look, LevelRight, LevelDown, LevelForward);
	vec3 eye = camera.pos;
	vec3 forward = camera.forward;

	// Break and place at the block under the crosshair: at once on a click,
	// then every REPEAT_DELAY while the button is held
	static double nextrepeat = 0;
	bool breaking = mouse.leftbutton && (mouse.leftcount == 1 || time.total >= nextrepeat);
	bool placing = !breaking && mouse.rightbutton && (mouse.rightcount == 1 || time.total >= nextrepeat);
	if (breaking || placing) nextrepeat = time.total + REPEAT_DELAY;

	RayHit target;
	bool targeting = WalkRay(eye, forward, REACH, WATER, false, false, target);
	if (targeting && breaking && target.block != BEDROCK) {
		SetAndUpdate(target.x, target.y, target.z, AIR);
		targeting = WalkRay(eye, forward, REACH, WATER, false, false, target);
	} else if (targeting && placing && target.lasty < WORLD_HEIGHT) {
		int x = target.lastx, y = target.lasty, z = target.lastz;
		int previous = World[y][z][x];
		World[y][z][x] = Hotbar[Selected];
		if (Collides(Position)) {
			World[y][z][x] = previous;
		} else {
			BlockChanged(x, y, z);
		}
		targeting = WalkRay(eye, forward, REACH, WATER, false, false, target);
	}

	bool underwater = BlockAt(eye) == WATER;
	float distance = underwater ? UNDERWATER_DISTANCE : MAX_DISTANCE;
	float fogstart = underwater ? UNDERWATER_FOG_START : FOG_START;
	int clear = underwater ? WATER : AIR;

	// The sun round its circle, and the light and sky it gives. The day has
	// a clock of its own, so that holding T can hurry it along
	static double daytime = 0;
	daytime += time.delta * (RETRO_KeyState(SDL_SCANCODE_T) ? FAST_TIME : 1.0);
	float angle = fmod(DAY_START + daytime * 2 * M_PI / DAY_LENGTH, 2 * M_PI);
	vec3 sunrise = { cosf(SUNRISE_AZIMUTH), 0, sinf(SUNRISE_AZIMUTH) };
	vec3 noon = { -sunrise.z * cosf(SUN_NOON_ELEVATION), sinf(SUN_NOON_ELEVATION), sunrise.x * cosf(SUN_NOON_ELEVATION) };
	vec3 sun = sunrise * cosf(angle) + noon * sinf(angle);

	// The sun's height over the world's edge as the eye sees it, which sinks
	// below the level the higher the eye stands
	float dip = atan2f(MAX(eye.y - SEA_LEVEL, 0.0f), MAX_DISTANCE);
	float height = sinf(asinf(sun.y) + dip);

	float daylight = smoothstep(NIGHT_HEIGHT, DUSK_HEIGHT, height);
	float light = mix(NIGHT_LIGHT, 1.0f, daylight);
	float sunset = (1.0f - smoothstep(0.0f, SUNSET_HEIGHT, fabsf(height))) * SUNSET_STRENGTH;
	RETRO_Palette horizon = mix(mix(NIGHT_HORIZON, DAY_HORIZON, daylight), SUNSET, sunset);
	RETRO_Palette zenith = mix(NIGHT_ZENITH, DAY_ZENITH, daylight);

	// Under water, the sky and the fog are both the water's own blue
	if (underwater) horizon = zenith = RETRO_ShadeColor(WATER_FOG, light);
	RETRO_CreateGradientPalette(SKY_START, SKY_START + SKY_SHADES, horizon, zenith);
	RETRO_SetColor(CLOUD_COLOR, mix(mix(NIGHT_CLOUD, DAY_CLOUD, daylight), SUNSET, sunset * 0.5f));
	RETRO_SetColor(STAR_COLOR, mix(zenith, STAR, 1.0f - daylight));

	// The sun's direct light, through ever more air as it sinks: weaker and
	// redder toward the horizon, and gone once it is down
	float sunstrength = smoothstep(0.0f, SUN_FADE_HEIGHT, height);
	float sunwhite = smoothstep(0.0f, SUN_WHITE_HEIGHT, height);
	vec3 lowsun = { LOW_SUN.r / 255.0f, LOW_SUN.g / 255.0f, LOW_SUN.b / 255.0f };
	vec3 sunlight = mix(lowsun, vec3{ 1, 1, 1 }, sunwhite) * sunstrength;

	// Each face direction lit, channel by channel, by the sky, by how
	// squarely it faces the sun by day, and the moon, opposite it, by night
	vec3 level[LIGHTS];
	for (int face = 0; face < LIGHTS; face++) {
		float sunfacing = MAX(dot(FaceNormal[face], sun), 0.0f);
		float moonfacing = MAX(-dot(FaceNormal[face], sun), 0.0f) * (1.0f - daylight);
		vec3 sum = vec3{ 1, 1, 1 } * (SUN_AMBIENT * light + MOON_DIRECT * moonfacing) + sunlight * (SUN_DIRECT * sunfacing);
		level[face] = min(sum * LightLevel[face], 1.0f);
	}

	// The water's light streaks pulse a little brighter and back, never far
	// from the blue around them, so the sea shimmers without speckling
	float shimmer = 0.5f + 0.5f * sinf(time.total * SHIMMER_SPEED);
	RETRO_Palette base[COLORS];
	for (int color = 0; color < COLORS; color++) base[color] = ColorTable[color];
	base[WATER2] = mix(ColorTable[WATER1], ColorTable[WATER2], mix(SHIMMER_LOW, SHIMMER_HIGH, shimmer));

	if (underwater) {
		// Tops at their own light, every other face at the sides' mean
		vec3 underwaterlevel[UNDERWATER_LIGHTS] = { level[LIGHT_TOP], (level[LIGHT_POS_X] + level[LIGHT_NEG_X] + level[LIGHT_POS_Z] + level[LIGHT_NEG_Z]) / 4 };
		for (int color = 0; color < COLORS; color++) {
			for (int group = 0; group < UNDERWATER_LIGHTS; group++) {
				RETRO_Palette lit = LightColor(base[color], underwaterlevel[group]);
				for (int fog = 0; fog < UNDERWATER_FOG_SHADES; fog++) {
					RETRO_SetColor(UnderwaterColor(color, group, fog), mix(lit, horizon, (float)fog / UNDERWATER_FOG_SHADES));
				}
			}
		}
	} else {
		for (int color = 0; color < COLORS; color++) {
			for (int face = 0; face < LIGHTS; face++) {
				RETRO_Palette lit = LightColor(base[color], level[face]);
				for (int fog = 0; fog < FOG_SHADES; fog++) {
					RETRO_SetColor(BlockColor(color, face, fog), mix(lit, horizon, (float)fog / FOG_SHADES));
				}
			}
		}
	}

	float clouddrift = time.total * CLOUD_SPEED;

	// Each star in front of the eye marks the one pixel it projects to
	bool starmap[RETRO_HEIGHT][RETRO_WIDTH] = {};
	for (vec3 star : Stars) {
		vec3 view = RETRO_ViewDirection(&camera, star);
		if (star.y <= 0 || view.z <= 0) continue;
		vec2 p = RETRO_ProjectViewPoint(camera.lens, view).pos;
		int sx = (int)floorf(p.x);
		int sy = (int)floorf(p.y);
		if (RETRO_OnScreen(sx, sy)) starmap[sy][sx] = true;
	}

	for (int sy = 0; sy < RETRO_HEIGHT; sy++) {
		for (int sx = 0; sx < RETRO_WIDTH; sx++) {
			// Through the pixel's center, so that a block edge lined up with the
			// view falls between pixels rather than on one
			vec3 dir = normalize(RETRO_ViewRay(&camera, { sx + 0.5f, sy + 0.5f }));

			RayHit hit;
			unsigned char color = SkyColor(dir);
			bool missed = !WalkRay(eye, dir, distance, clear, true, underwater, hit);
			bool sky = true; // the pixel shows the sky's color, missed or fogged out
			if (!missed) {
				bool ontarget = targeting && hit.x == target.x && hit.y == target.y && hit.z == target.z;
				float width = OUTLINE_WIDTH * hit.t * dot(dir, forward) / FOCAL;
				bool edge = MIN(MIN(hit.u, 1.0f - hit.u), MIN(hit.v, 1.0f - hit.v)) < width;
				float fog = smoothstep(fogstart, distance, hit.t);
				int fogshades = underwater ? UNDERWATER_FOG_SHADES : FOG_SHADES;
				int fogstep = (int)(fog * fogshades + 0.5f);
				if (ontarget && edge) {
					color = OUTLINE_COLOR;
					sky = false;
				} else if (underwater && fogstep < fogshades) {
					color = UnderwaterColor(hit.texel, hit.light == LIGHT_TOP ? 0 : 1, fogstep);
					sky = false;
				} else if (fogstep < fogshades) {
					// A face toward the sun that a block hides it from is in shadow
					int face = hit.light;
					vec3 normal = FaceNormal[face];
					if (sunstrength > 0 && dot(normal, sun) > 0) {
						vec3 p = eye + dir * hit.t - normal * SHADOW_OFFSET;
						RayHit blocker;
						if (WalkRay(p, sun, SHADOW_DISTANCE, WATER, true, false, blocker)) face = Opposite[face];
					}
					color = BlockColor(hit.texel, face, fogstep);
					sky = false;
				}
			}
			if (sky && !underwater) {
				if (missed && dir.y > 0 && daylight < 1 && starmap[sy][sx]) color = STAR_COLOR;

				// Wherever the sky's color shows, below the horizon too, so that
				// they set behind the last of the sea or land to be seen
				float sunextent = BodyExtent(dir, sun);
				if (sunextent < SUN_SIZE) color = sunextent < SUN_SIZE * SUN_CORE_SIZE ? SUN_CORE_COLOR : SUN_RIM_COLOR;
				if (BodyExtent(dir, -sun) < MOON_SIZE) color = MOON_COLOR;
			}

			// A flat layer of cloud cells, in front of the eye out to
			// CLOUD_DISTANCE, over whatever the ray reaches beyond it
			float cloudt = (CLOUD_HEIGHT - eye.y) / dir.y;
			if (!underwater && cloudt > 0 && cloudt < CLOUD_DISTANCE && (missed || cloudt < hit.t)) {
				vec3 p = eye + dir * cloudt;
				int cellx = (int)floorf((p.x + clouddrift) / CLOUD_CELL), cellz = (int)floorf(p.z / CLOUD_CELL);
				if (RETRO_ValueNoise(cellx * CLOUD_SCALE, cellz * CLOUD_SCALE) > CLOUD_COVER) color = CLOUD_COLOR;
			}
			RETRO_PutPixel(sx, sy, color);
		}
	}

	DrawCrosshair();
	DrawHotbar();
}

void DEMO_Initialize(void)
{
	CreateWorld();
	CreateTextures();

	// Even over the sphere: height uniform in [-1, 1], angle uniform around
	for (int star = 0; star < STAR_COUNT; star++) {
		float height = 2 * RETRO_HashUnit(star, 1) - 1;
		float around = 2 * M_PI * RETRO_HashUnit(star, 2);
		float radius = sqrtf(1 - height * height);
		Stars[star] = { radius * cosf(around), height, radius * sinf(around) };
	}

	// Start on the grass nearest the middle of the world, clear of trees
	for (int radius = 0; radius < WORLD_SIZE / 2; radius++) {
		bool found = false;
		for (int z = WORLD_SIZE / 2 - radius; z <= WORLD_SIZE / 2 + radius && !found; z++) {
			for (int x = WORLD_SIZE / 2 - radius; x <= WORLD_SIZE / 2 + radius && !found; x++) {
				int h = (int)GroundHeight(x, z);
				if (World[h][z][x] == GRASS && World[h + 1][z][x] == AIR && World[h + 2][z][x] == AIR) {
					Position = { x + 0.5f, h + 1.0f, z + 0.5f };
					found = true;
				}
			}
		}
		if (found) break;
	}

	RETRO_SetSpacePause(false);
	RETRO_SetMouseMode(true);

	RETRO_SetColor(OUTLINE_COLOR, OUTLINE);
	RETRO_SetColor(CROSSHAIR_COLOR, CROSSHAIR);
	RETRO_SetColor(SUN_RIM_COLOR, SUN_RIM);
	RETRO_SetColor(SUN_CORE_COLOR, SUN_CORE);
	RETRO_SetColor(MOON_COLOR, MOON);
	for (int color = 0; color < COLORS; color++) {
		RETRO_SetColor(HOTBAR_START + color, RETRO_ShadeColor(ColorTable[color], LightLevel[LIGHT_POS_X]));
	}
}
