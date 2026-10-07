//
// Block world
//
// A world of textured blocks to walk: grassy hills, sandy shores, a flat
// sea and oak trees with leafy crowns, drawn one ray per pixel.
//
// The world is a grid of WORLD_SIZE × WORLD_HEIGHT × WORLD_SIZE cells, each
// holding a block type or air. The ground is value noise summed over three
// octaves and cut into whole blocks: a column of height h is dirt topped
// with grass, or with sand where h is within a block of the sea, and the
// cells between a low column and SEA_LEVEL are water. Trees stand one to a
// TREE_CELL square at most, at a hashed spot within it, on grass only: a
// trunk of logs with a crown of leaves, two wide layers and two narrow ones
// above them, the wide layers' corners hashed away.
//
// The camera stands at o, and pixel (x, y) casts the ray through its center
//
//   d = normalize(F f + (x + ½ − W/2) r + (H/2 − y − ½) u)
//
// with f, r and u the camera's forward, right and up, so that a block edge
// lined up with the view falls between pixels rather than on one. The ray
// walks the grid cell by cell (a DDA), as raycast.cpp does across its map
// but in all three axes: for each it keeps the t of the next grid plane it
// will cross, steps over whichever is nearest, and stops at the first block
// it enters. The axis of the last plane crossed is the face hit, and the
// hit point o + t d, taken modulo one block across that face, picks the
// texel of the block's top, side or bottom texture. A leaves texel that is
// a hole lets the ray walk on, so a crown is seen through.
//
// Each face is lit by its axis alone, top brightest and the two side axes
// darker by different amounts, so a block's edges read without a light
// source. Past FOG_START the color is mixed toward the horizon, in
// FOG_SHADES steps dithered across each other, and a ray that walks
// MAX_DISTANCE without a hit, or that leaves the world, sees the sky: a
// gradient from the horizon up to the zenith.
//
// The palette is a sky gradient followed by every texture color at every
// face light and fog step, so a pixel's color is an index computed straight
// from those three.
//
// The camera is the player's eye, EYE_HEIGHT above their feet, turned by
// the mouse: yaw about the vertical and pitch up or down, short of
// straight. The mouse is read once a frame, not once a simulation step, so
// turning is as smooth as the display. W and S walk along the heading and
// A and D to the side, a diagonal no faster than a straight line.
//
// The player is a box PLAYER_RADIUS to each side and PLAYER_HEIGHT tall
// that every block stops, water too, so the player walks on the sea. Each
// step moves the box along x, then z, then y, and a move that ends
// overlapping a block is pushed back to that block's face, so the player
// slides along a wall rather than stopping dead. Gravity pulls the box
// down at GRAVITY until a block stops it, and only then can Space jump,
// rising at JUMP_SPEED: high enough to climb one block, not two. They
// start on the grass nearest the middle of the world.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retromath.h"
#include "lib/retromouse.h"
#include "lib/retrocamera.h"
#include "lib/retropalette.h"
#include "lib/retrovector.h"

#define WORLD_SIZE 256
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
#define MAX_DISTANCE 64.0f // blocks a ray walks before it sees the sky
#define FOG_START 24.0f

#define PLAYER_RADIUS 0.3f
#define PLAYER_HEIGHT 1.8f
#define EYE_HEIGHT 1.62f
#define WALK_SPEED 4.3f // blocks a second
#define GRAVITY 32.0f // blocks a second, a second
#define JUMP_SPEED 9.0f // rises 9² / (2 × 32) = 1.27 blocks
#define MAX_FALL_SPEED 40.0f // two thirds of a block a step, so a fall never passes through one
#define CONTACT_GAP 0.001f // left between the box and a block it is pushed back from
#define MOUSE_SENSITIVITY 0.003f // radians per mouse unit
#define MAX_PITCH 1.5f // radians, short of straight up or down

#define TEXTURE_SIZE 16
#define TEXEL_HOLE 255 // a leaves texel the ray passes through

enum Block { AIR, GRASS, DIRT, SAND, WATER, LOG, LEAVES, BLOCKS };

enum Texture { TEX_GRASS_TOP, TEX_GRASS_SIDE, TEX_DIRT, TEX_SAND, TEX_WATER, TEX_LOG_SIDE, TEX_LOG_TOP, TEX_LEAVES, TEXTURES };

enum Color { GRASS1, GRASS2, GRASS3, DIRT1, DIRT2, DIRT3, BARK1, BARK2, WOOD1, WOOD2, LEAF1, LEAF2, SAND1, SAND2, WATER1, WATER2, COLORS };

enum Light { LIGHT_TOP, LIGHT_SIDE_X, LIGHT_SIDE_Z, LIGHTS };

static const RETRO_Palette ColorTable[COLORS] = {
	{ 74, 122, 38 }, { 96, 156, 52 }, { 122, 186, 70 },
	{ 102, 70, 44 }, { 134, 96, 63 }, { 158, 118, 82 },
	{ 68, 52, 30 }, { 104, 82, 48 },
	{ 160, 130, 80 }, { 186, 156, 100 },
	{ 44, 100, 28 }, { 66, 134, 40 },
	{ 210, 196, 140 }, { 226, 214, 162 },
	{ 44, 84, 186 }, { 60, 104, 206 },
};

static const float LightLevel[LIGHTS] = { 1.0f, 0.8f, 0.62f };

// The textures of each block's top, sides and bottom
static const int BlockTexture[BLOCKS][3] = {
	{ 0, 0, 0 }, // AIR, never drawn
	{ TEX_GRASS_TOP, TEX_GRASS_SIDE, TEX_DIRT },
	{ TEX_DIRT, TEX_DIRT, TEX_DIRT },
	{ TEX_SAND, TEX_SAND, TEX_SAND },
	{ TEX_WATER, TEX_WATER, TEX_WATER },
	{ TEX_LOG_TOP, TEX_LOG_SIDE, TEX_LOG_TOP },
	{ TEX_LEAVES, TEX_LEAVES, TEX_LEAVES },
};

#define SKY_HORIZON (RETRO_Palette){ 180, 210, 240 }
#define SKY_ZENITH (RETRO_Palette){ 70, 120, 215 }

#define SKY_START 0
#define SKY_SHADES 16
#define FOG_SHADES 5
#define BLOCK_START (SKY_START + SKY_SHADES)

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
				if (corner && (ly == top + 1 || RETRO_HashUnit(x + dx * 7 + ly, z + dz * 13) < 0.6f)) continue;
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
			for (int y = 0; y < h; y++) World[y][z][x] = DIRT;
			World[h][z][x] = top;
			for (int y = h + 1; y <= SEA_LEVEL; y++) World[y][z][x] = WATER;
		}
	}

	for (int cz = 0; cz < WORLD_SIZE / TREE_CELL; cz++) {
		for (int cx = 0; cx < WORLD_SIZE / TREE_CELL; cx++) {
			if (RETRO_HashUnit(cx, cz + 1000) > TREE_CHANCE) continue;
			int x = cx * TREE_CELL + 2 + RETRO_Hash(cx, cz + 2000) % (TREE_CELL - 4);
			int z = cz * TREE_CELL + 2 + RETRO_Hash(cx, cz + 3000) % (TREE_CELL - 4);
			int h = (int)GroundHeight(x, z);
			if (World[h][z][x] != GRASS) continue;
			int trunk = TREE_MIN_TRUNK + RETRO_Hash(cx, cz + 4000) % (TREE_MAX_TRUNK - TREE_MIN_TRUNK + 1);
			if (h + trunk + 2 >= WORLD_HEIGHT) continue;
			PlantTree(x, h + 1, z, trunk);
		}
	}
}

// One of n colors from first on, picked by a texel's hash
static unsigned char PickColor(int texture, int x, int y, int first, int n)
{
	return first + RETRO_Hash(x + texture * TEXTURE_SIZE, y) % n;
}

static void CreateTextures(void)
{
	for (int y = 0; y < TEXTURE_SIZE; y++) {
		for (int x = 0; x < TEXTURE_SIZE; x++) {
			Textures[TEX_GRASS_TOP][y][x] = PickColor(TEX_GRASS_TOP, x, y, GRASS1, 3);
			Textures[TEX_DIRT][y][x] = PickColor(TEX_DIRT, x, y, DIRT1, 3);
			Textures[TEX_SAND][y][x] = PickColor(TEX_SAND, x, y, SAND1, 2);

			// Grass hangs a ragged two to four texels over the dirt
			int fringe = 2 + RETRO_Hash(x, TEX_GRASS_SIDE) % 3;
			Textures[TEX_GRASS_SIDE][y][x] = y < fringe ? PickColor(TEX_GRASS_SIDE, x, y, GRASS1, 3) : PickColor(TEX_GRASS_SIDE, x, y, DIRT1, 3);

			// Water is mostly the darker blue, with short light streaks
			Textures[TEX_WATER][y][x] = RETRO_Hash(x / 3, y + TEX_WATER * TEXTURE_SIZE) % 5 == 0 ? WATER2 : WATER1;

			// Bark runs in vertical stripes, a little broken up
			bool stripe = RETRO_Hash(x, TEX_LOG_SIDE) % 3 == 0;
			bool knot = RETRO_Hash(x + TEX_LOG_SIDE * TEXTURE_SIZE, y) % 7 == 0;
			Textures[TEX_LOG_SIDE][y][x] = stripe != knot ? BARK1 : BARK2;

			// The cut end shows bark around the edge and growth rings inside
			int ring = (int)MAX(fabsf(x - 7.5f), fabsf(y - 7.5f));
			Textures[TEX_LOG_TOP][y][x] = ring == 7 ? BARK1 : (ring & 1 ? WOOD1 : WOOD2);

			float leaf = RETRO_HashUnit(x + TEX_LEAVES * TEXTURE_SIZE, y);
			Textures[TEX_LEAVES][y][x] = leaf < 0.2f ? TEXEL_HOLE : (leaf < 0.6f ? LEAF1 : LEAF2);
		}
	}
}

static vec3 Position; // the player's feet
static float VelocityY;
static float Yaw, Pitch;
static bool OnGround;

// The sky seen along dir, dithered across its gradient
static unsigned char SkyColor(vec3 dir, float dither)
{
	float level = CLAMP01(dir.y * 2.5f);
	return SKY_START + MIN((int)(level * (SKY_SHADES - 1) + dither), SKY_SHADES - 1);
}

static unsigned char TraceRay(vec3 origin, vec3 dir, float dither)
{
	int x = (int)floorf(origin.x), y = (int)floorf(origin.y), z = (int)floorf(origin.z);
	int stepx = dir.x < 0 ? -1 : 1, stepy = dir.y < 0 ? -1 : 1, stepz = dir.z < 0 ? -1 : 1;

	// t across one cell along each axis, and t to the first plane crossed
	float deltax = fabsf(1.0f / dir.x), deltay = fabsf(1.0f / dir.y), deltaz = fabsf(1.0f / dir.z);
	float nextx = (dir.x < 0 ? origin.x - x : x + 1 - origin.x) * deltax;
	float nexty = (dir.y < 0 ? origin.y - y : y + 1 - origin.y) * deltay;
	float nextz = (dir.z < 0 ? origin.z - z : z + 1 - origin.z) * deltaz;

	for (;;) {
		float t;
		int face;
		if (nextx < nexty && nextx < nextz) {
			x += stepx;
			t = nextx;
			nextx += deltax;
			face = LIGHT_SIDE_X;
		} else if (nexty < nextz) {
			y += stepy;
			t = nexty;
			nexty += deltay;
			face = LIGHT_TOP;
		} else {
			z += stepz;
			t = nextz;
			nextz += deltaz;
			face = LIGHT_SIDE_Z;
		}

		if (t > MAX_DISTANCE) break;
		if (x < 0 || x >= WORLD_SIZE || z < 0 || z >= WORLD_SIZE || y < 0) break;
		if (y >= WORLD_HEIGHT) {
			if (stepy > 0) break;
			continue;
		}

		int block = World[y][z][x];
		if (block == AIR) continue;

		// Where the ray meets the face, as texture coordinates across it
		vec3 p = origin + dir * t;
		float u, v;
		int side;
		if (face == LIGHT_TOP) {
			u = p.x - x;
			v = p.z - z;
			side = stepy < 0 ? 0 : 2;
		} else if (face == LIGHT_SIDE_X) {
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
		if (texel == TEXEL_HOLE) continue;

		// A bottom is in shadow, as dark as the darker side
		int light = side == 2 ? LIGHT_SIDE_Z : face;
		float fog = smoothstep(FOG_START, MAX_DISTANCE, t);
		int fogstep = (int)(fog * FOG_SHADES + dither);
		if (fogstep >= FOG_SHADES) break;
		return BLOCK_START + (texel * LIGHTS + light) * FOG_SHADES + fogstep;
	}

	return SkyColor(dir, dither);
}

// Whether a cell stops the player. Every block does, water too, and so do
// the world's edges and floor; the sky above it does not.
static bool Solid(int x, int y, int z)
{
	if (x < 0 || x >= WORLD_SIZE || z < 0 || z >= WORLD_SIZE || y < 0) return true;
	if (y >= WORLD_HEIGHT) return false;
	return World[y][z][x] != AIR;
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

void DEMO_FixedUpdate(RETRO_Time time)
{
	float dt = time.delta;

	// Walk along the ground, whichever way the player looks
	vec3 forward = { cosf(Yaw), 0, sinf(Yaw) };
	vec3 right = { forward.z, 0, -forward.x };
	vec3 move = { 0, 0, 0 };
	if (RETRO_KeyState(SDL_SCANCODE_W)) move = move + forward;
	if (RETRO_KeyState(SDL_SCANCODE_S)) move = move - forward;
	if (RETRO_KeyState(SDL_SCANCODE_D)) move = move + right;
	if (RETRO_KeyState(SDL_SCANCODE_A)) move = move - right;
	if (length(move) > 0) move = normalize(move) * (WALK_SPEED * dt);

	VelocityY = MAX(VelocityY - GRAVITY * dt, -MAX_FALL_SPEED);
	if (RETRO_KeyState(SDL_SCANCODE_SPACE) && OnGround) VelocityY = JUMP_SPEED;

	// One axis at a time, each pushed back to the face of the block it ran into
	Position.x += move.x;
	if (Collides(Position)) {
		Position.x = move.x > 0 ? floorf(Position.x + PLAYER_RADIUS) - PLAYER_RADIUS - CONTACT_GAP : floorf(Position.x - PLAYER_RADIUS) + 1 + PLAYER_RADIUS + CONTACT_GAP;
	}
	Position.z += move.z;
	if (Collides(Position)) {
		Position.z = move.z > 0 ? floorf(Position.z + PLAYER_RADIUS) - PLAYER_RADIUS - CONTACT_GAP : floorf(Position.z - PLAYER_RADIUS) + 1 + PLAYER_RADIUS + CONTACT_GAP;
	}

	OnGround = false;
	Position.y += VelocityY * dt;
	if (Collides(Position)) {
		if (VelocityY < 0) {
			Position.y = floorf(Position.y) + 1;
			OnGround = true;
		} else {
			Position.y = floorf(Position.y + PLAYER_HEIGHT) - PLAYER_HEIGHT - CONTACT_GAP;
		}
		VelocityY = 0;
	}
}

void DEMO_Render(RETRO_Time time)
{
	// Look, once a frame so that turning is as smooth as the display
	RETRO_MouseState mouse = RETRO_GetMouseState();
	Yaw = fmodf(Yaw - mouse.xrel * MOUSE_SENSITIVITY, 2 * M_PI);
	Pitch = MAX(MIN(Pitch - mouse.yrel * MOUSE_SENSITIVITY, MAX_PITCH), -MAX_PITCH);

	// Aim from a level frame facing +x, y up. The yaw turns left and the
	// pitch looks up, the other way from the camera's turns.
	RETRO_Camera camera;
	RETRO_InitializeCamera(&camera, Position + vec3{ 0, EYE_HEIGHT, 0 });
	camera.lens.focalx = FOCAL;
	camera.lens.focaly = FOCAL;
	RETRO_AimCamera(&camera, { 0, 0, -1 }, { 0, -1, 0 }, { 1, 0, 0 }, -Yaw, -Pitch);

	for (int sy = 0; sy < RETRO_HEIGHT; sy++) {
		for (int sx = 0; sx < RETRO_WIDTH; sx++) {
			vec3 dir = normalize(RETRO_ViewRay(&camera, { sx + 0.5f, sy + 0.5f }));
			RETRO_PutPixel(sx, sy, TraceRay(camera.pos, dir, RETRO_DitherThreshold(sx, sy)));
		}
	}
}

void DEMO_Initialize(void)
{
	CreateWorld();
	CreateTextures();

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

	RETRO_CreateGradientPalette(SKY_START, SKY_START + SKY_SHADES, SKY_HORIZON, SKY_ZENITH);
	for (int color = 0; color < COLORS; color++) {
		for (int light = 0; light < LIGHTS; light++) {
			RETRO_Palette lit = RETRO_ShadeColor(ColorTable[color], LightLevel[light]);
			for (int fog = 0; fog < FOG_SHADES; fog++) {
				RETRO_SetColor(BLOCK_START + (color * LIGHTS + light) * FOG_SHADES + fog, mix(lit, SKY_HORIZON, (float)fog / FOG_SHADES));
			}
		}
	}
}
