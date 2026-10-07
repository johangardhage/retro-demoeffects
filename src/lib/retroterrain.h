//
// Retro terrain library
//
// A height field, the lens it is looked through, and two ways of standing
// over it. World x runs across the map, z along it, and height is up.
//
//   The map      RETRO_Terrain: two planes of bytes, read nearest, filtered
//                or on the drawn triangles, shaded by RETRO_TerrainShade
//   The lens     RETRO_TerrainLens, the camera lens every look ends in: a
//                point in the camera's frame, right, down and forward of the
//                eye, onto the screen. RETRO_TerrainView, how much is drawn
//   Wrapping     RETRO_TerrainCamera: a yaw over a torus, flown at a fixed
//                speed or as RETRO_Vehicle, drawn as a mesh with models
//                standing on it, as dots or as columns
//   Island       RETRO_Island: a finite patch on a turntable, seen from
//                outside and pitched down
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROTERRAIN_H_
#define _RETROTERRAIN_H_

#include "retro.h"
#include "retrovector.h"
#include "retropoly.h"
#include "retromath.h"
#include "retrocamera.h"
#include "retroshadetable.h"

// How far the wrapping look draws, in map cells. The wide lens flattens the
// hills, so nothing hides where the ground stops: this has to reach far
// enough to meet the sky.
#define RETRO_TERRAIN_DISTANCE 800

// *******************************************************************
// The map
// *******************************************************************

//
// The current terrain
//
// It points at the planes rather than copying them, so a loaded map and one
// an effect built for itself are described the same way. width and height
// are the map in cells across x and along z; a stored height is a byte, and
// scale is the world units that byte is worth.
//
// A wrapping map is a torus, which lets a camera travel one way forever. A
// finite patch has a last cell, and a sample past it is that cell, not the
// other side of the map.
//
inline struct {
	unsigned char *heightmap = NULL;	// One byte of altitude per cell
	unsigned char *colormap = NULL;		// The color painted over it, one byte per cell
	int width = 0;						// Map size in cells, across x
	int height = 0;						// and along z
	bool wrap = true;					// Both axes wrap; a finite patch sets this false
	float scale = 1.0f;					// World units per stored height value
} RETRO_Terrain;

// Point the sampling at these planes. wrap false is a finite patch.
inline void RETRO_SetTerrain(int width, int height, float scale, unsigned char *heightmap, unsigned char *colormap, bool wrap = true)
{
	RETRO_Terrain.width = width;
	RETRO_Terrain.height = height;
	RETRO_Terrain.scale = scale;
	RETRO_Terrain.heightmap = heightmap;
	RETRO_Terrain.colormap = colormap;
	RETRO_Terrain.wrap = wrap;
}

//
// Load both maps and describe the terrain they make
//
// The size is taken from the images, never asked for, and the two must
// agree: a size the file does not have would read every row at the wrong
// stride, and nothing downstream could tell.
//
// The heights are the height map's palette indices, not its colors, so its
// palette has to rise with the index: gray, darkest first. A converter that
// reorders the palette leaves a file that loads and draws but scrambles
// every height, so that is refused.
//
// The default scale leaves a stored byte as the world height.
//
inline void RETRO_LoadTerrain(const char *colorfile, const char *heightfile, float scale = 1.0f, bool wrap = true)
{
	RETRO_Image *colormap = RETRO_LoadImage(colorfile, true);
	RETRO_Image *heightmap = RETRO_LoadImage(heightfile);
	if (colormap->width != heightmap->width || colormap->height != heightmap->height) {
		RETRO_RageQuit("Terrain color and height maps must be the same size\n");
	}
	for (int i = 0; i < RETRO_COLORS; i++) {
		RETRO_Palette c = heightmap->palette[i];
		if (c.r != c.g || c.g != c.b || (i > 0 && c.r < heightmap->palette[i - 1].r)) {
			RETRO_RageQuit("%s: a height map's palette must be gray, darkest first\n", heightfile);
		}
	}
	RETRO_SetTerrain(heightmap->width, heightmap->height, scale, heightmap->data, colormap->data, wrap);
}

// The middle of the map, at its zero: halfway from the first sample to the
// last along each side.
inline vec3 RETRO_TerrainCenter(void)
{
	return { (RETRO_Terrain.width - 1) * 0.5f, 0, (RETRO_Terrain.height - 1) * 0.5f };
}

// Whether wrapping a coordinate can be a mask: the map wraps and both sides
// are powers of two. Asked of the loaded map, so swapping the asset answers
// for the new one.
inline bool RETRO_TerrainWrapsByMask(void)
{
	int width = RETRO_Terrain.width;
	int height = RETRO_Terrain.height;

	return RETRO_Terrain.wrap && width > 0 && (width & (width - 1)) == 0 && height > 0 && (height & (height - 1)) == 0;
}

// A coordinate folded onto a side of size cells the way the map does: wrapped
// on a torus, held on the last cell of a finite patch.
inline int RETRO_TerrainFold(int n, int size)
{
	return RETRO_Terrain.wrap ? WRAP(n, size) : CLAMP(n, 0, size);
}

// The cell under (x, z), folded
inline int RETRO_TerrainIndex(float x, float z)
{
	return RETRO_TerrainFold((int)floorf(z), RETRO_Terrain.height) * RETRO_Terrain.width + RETRO_TerrainFold((int)floorf(x), RETRO_Terrain.width);
}

// The stored height at a cell, unscaled
inline unsigned char RETRO_TerrainSample(float x, float z)
{
	return RETRO_Terrain.heightmap[RETRO_TerrainIndex(x, z)];
}

// The color painted on a cell
inline unsigned char RETRO_TerrainColor(float x, float z)
{
	return RETRO_Terrain.colormap[RETRO_TerrainIndex(x, z)];
}

// The ground height at a cell, in world units
inline float RETRO_TerrainHeight(float x, float z)
{
	return RETRO_TerrainSample(x, z) * RETRO_Terrain.scale;
}

//
// A wrapping height field built by midpoint displacement, and a slope-shaded
// color map to go with it
//
// The map has to be square with power-of-two sides: diamond-square steps by
// halves of one length, and the column walks that read it wrap by masking.
// It is zeroed first, since the first pass reads heightmap[0] as its seed.
//
// Midpoint displacement fills the byte range far more evenly than a
// photograph does, so 0.6 of a stored byte here is the same ground as one of
// a photograph's, and worldscale then brings it into the caller's world.
// Flying that world at the matching size is RETRO_ScaleTerrainWorld.
//
inline void RETRO_BuildDisplacementTerrain(unsigned char *heightmap, unsigned char *colormap, int width, int height, float worldscale)
{
	if (width != height || width <= 0 || (width & (width - 1)) != 0) {
		RETRO_RageQuit("Displacement terrain must be square with power-of-two sides\n");
	}

	memset(heightmap, 0, (size_t)width * (size_t)height);
	RETRO_SetTerrain(width, height, 0.6f * worldscale, heightmap, colormap);

	for (int p = width; p > 1; p /= 2) {
		int p2 = p / 2;
		int k = p * 8 + 20;
		int k2 = k / 2;

		for (int z = 0; z < height; z += p) {
			for (int x = 0; x < width; x += p) {
				int a = RETRO_TerrainSample(x, z);
				int b = RETRO_TerrainSample(x, z + p);
				int c = RETRO_TerrainSample(x + p, z);
				int d = RETRO_TerrainSample(x + p, z + p);

				heightmap[RETRO_TerrainIndex(x + p2, z)] = CLAMP256(((a + c) / 2) + (RANDOM(k) - k2));
				heightmap[RETRO_TerrainIndex(x + p2, z + p2)] = CLAMP256(((a + b + c + d) / 4) + (RANDOM(k) - k2));
				heightmap[RETRO_TerrainIndex(x, z + p2)] = CLAMP256(((a + b) / 2) + (RANDOM(k) - k2));
			}
		}
	}

	for (int k = 0; k < 5; k++) {
		for (int z = 0; z < height; z++) {
			for (int x = 0; x < width; x++) {
				heightmap[RETRO_TerrainIndex(x, z)] = (RETRO_TerrainSample(x, z + 1) + RETRO_TerrainSample(x + 1, z) + RETRO_TerrainSample(x, z - 1) + RETRO_TerrainSample(x - 1, z)) / 4;
			}
		}
	}

	for (int z = 0; z < height; z++) {
		for (int x = 0; x < width; x++) {
			colormap[RETRO_TerrainIndex(x, z)] = CLAMP256(128 + (RETRO_TerrainSample(x + 1, z + 1) - RETRO_TerrainSample(x, z)) * 6);
		}
	}
}

//
// The four stored heights around (x, z), and where it sits between them
//
// The corners fold the way the map does: wrapping mixes a sample past the
// edge with the other side of the torus, clamping repeats the last cell.
//
struct RETRO_TerrainCorners {
	float h00, h10;		// Stored heights along the nearer row, x then x + 1
	float h01, h11;		// and the row after
	float fx, fz;		// Position inside the cell, each in [0, 1)
};

inline RETRO_TerrainCorners RETRO_TerrainCornersAt(float x, float z)
{
	unsigned char *heightmap = RETRO_Terrain.heightmap;
	int width = RETRO_Terrain.width;
	int height = RETRO_Terrain.height;
	int ix = floorf(x);
	int iz = floorf(z);

	int x0 = RETRO_TerrainFold(ix, width);
	int x1 = RETRO_TerrainFold(ix + 1, width);
	int z0 = RETRO_TerrainFold(iz, height) * width;
	int z1 = RETRO_TerrainFold(iz + 1, height) * width;

	RETRO_TerrainCorners corners;
	corners.h00 = heightmap[z0 + x0];
	corners.h10 = heightmap[z0 + x1];
	corners.h01 = heightmap[z1 + x0];
	corners.h11 = heightmap[z1 + x1];
	corners.fx = x - ix;
	corners.fz = z - iz;
	return corners;
}

// The ground height between cells, bilinearly filtered, in world units: what
// a camera riding the ground reads, since it must not step at a cell edge.
inline float RETRO_TerrainHeightLinear(float x, float z)
{
	RETRO_TerrainCorners c = RETRO_TerrainCornersAt(x, z);
	float top = mix(c.h00, c.h10, c.fx);
	float bottom = mix(c.h01, c.h11, c.fx);
	return mix(top, bottom, c.fz) * RETRO_Terrain.scale;
}

// The ground height on the drawn surface, in world units. A one-cell mesh
// splits each cell along the diagonal from (x, z) to (x + 1, z + 1); the
// bilinear patch bulges off those two flat triangles, this does not, so
// anything laid on it sits on what is drawn.
inline float RETRO_TerrainHeightTriangle(float x, float z)
{
	RETRO_TerrainCorners c = RETRO_TerrainCornersAt(x, z);
	float height;
	if (c.fx >= c.fz) {
		height = c.h00 + c.fx * (c.h10 - c.h00) + c.fz * (c.h11 - c.h10);
	} else {
		height = c.h00 + c.fz * (c.h01 - c.h00) + c.fx * (c.h11 - c.h01);
	}
	return height * RETRO_Terrain.scale;
}

//
// Either plane of a wrapping, power-of-two map, bilinearly filtered
//
// The voxel hot path: unscaled, folded by a mask, and with each corner read
// once. A column walk must have checked RETRO_TerrainWrapsByMask. floor and
// not a cast, so a walk looking back across the map's origin does not fold
// the cells either side of it onto one sample.
//
inline float RETRO_TerrainSampleLinear(const unsigned char *map, float x, float z)
{
	int width = RETRO_Terrain.width;
	int height = RETRO_Terrain.height;
	int ix = floorf(x);
	int iz = floorf(z);
	float fx = x - ix;
	float fz = z - iz;

	int x0 = ix & (width - 1);
	int x1 = (ix + 1) & (width - 1);
	int z0 = (iz & (height - 1)) * width;
	int z1 = ((iz + 1) & (height - 1)) * width;

	float top = mix(map[z0 + x0], map[z0 + x1], fx);
	float bottom = mix(map[z1 + x0], map[z1 + x1], fx);
	return mix(top, bottom, fz);
}

// The ground's upward normal at (x, z), by central differences over step.
// Not unit length. Taken at a map vertex it is the same from every cell
// that shares the vertex, so neighbors light a shared edge alike.
inline vec3 RETRO_TerrainNormal(float x, float z, float step = 1)
{
	return { RETRO_TerrainHeight(x - step, z) - RETRO_TerrainHeight(x + step, z), 2.0f * step, RETRO_TerrainHeight(x, z - step) - RETRO_TerrainHeight(x, z + step) };
}

//
// The shade a surface facing n takes under a sun toward light, in [0, shades)
//
// Neither vector need be unit. A height field's normals all point up, so the
// darkest lambert it can reach is a vertical wall turned from the sun, at
// -sqrt(1 - y^2) for a unit light. The ramp spans that band rather than
// [-1, 1], and is not clipped at the terminator, so a face turned away
// darkens instead of dropping to black. A sun well off vertical tells a slope
// facing it from one turned away; one overhead lights every gentle slope
// alike.
//
inline int RETRO_TerrainShade(vec3 n, vec3 sun, int shades)
{
	float llength = length(sun);
	float light = dot(n, sun) / (length(n) * llength);
	float uy = sun.y / llength;
	float darkest = -sqrtf(1.0f - uy * uy);
	return CLAMP((int)((light - darkest) / (1.0f - darkest) * shades), 0, shades);
}

//
// Where a ray first meets the drawn surface
//
// Marches from origin along a unit direction in steps of step world units
// until it is at or below the ground, then halves the last step eight times
// back onto it. A ridge thinner than a step may be passed over. A ray that
// starts under the ground meets it at its origin. It gives up, returning
// false, once it is level or climbing above the highest byte, or has run
// the map's width and height together without a hit.
//
inline bool RETRO_TerrainRayHit(vec3 origin, vec3 direction, float step, vec3 &hit)
{
	float top = 255 * RETRO_Terrain.scale;
	float limit = RETRO_Terrain.width + RETRO_Terrain.height;
	float across = hypotf(direction.x, direction.z);

	float t = 0;
	vec3 p = origin;
	while (p.y > RETRO_TerrainHeightTriangle(p.x, p.z)) {
		if ((direction.y >= 0 && p.y > top) || t * across > limit) return false;
		t += step;
		p = origin + direction * t;
	}
	if (t == 0) {
		hit = origin;
		return true;
	}

	float above = t - step, below = t;
	for (int i = 0; i < 8; i++) {
		float middle = (above + below) * 0.5f;
		vec3 q = origin + direction * middle;
		if (q.y > RETRO_TerrainHeightTriangle(q.x, q.z)) {
			above = middle;
		} else {
			below = middle;
		}
	}
	hit = origin + direction * below;
	return true;
}

// *******************************************************************
// The lens
// *******************************************************************

//
// The lens
//
// The defaults are the wrapping look, and the pixels are square: ninety
// degrees across and about a hundred down. The taller view flattens the
// hills and puts more ground below the horizon, and a wide view is what
// makes walking feel fast - the same cells per second read as a trudge
// through a narrow lens. RETRO_LookDownAtTerrain writes a longer one.
//
// The lens's center row is the horizon, where eye level lands. Sliding it
// tilts the picture without pitching the camera, so voxel columns can still
// treat each screen row as a constant-depth slice.
//
inline RETRO_CameraLens RETRO_TerrainLens = {
	RETRO_WIDTH * 0.5f,								// Focal length across, half the width for a 90 degree view
	RETRO_HEIGHT * 0.42f,							// and down: shorter, so the view is taller than it is wide
	{ RETRO_WIDTH / 2.0f, RETRO_HEIGHT * 0.43f },	// Eye level at the horizon
	3.0f,											// Nearest depth worth drawing
};

//
// How much ground is paid for
//
// step and distance are draw policy: how finely and how far a mesh or a
// column walk spends itself. horizonspeed is input, kept here because it
// moves the lens.
//
inline struct {
	int step = 6;										// Mesh spacing, in map cells
	int distance = RETRO_TERRAIN_DISTANCE;				// How far the wrapping look draws, in map cells
	float horizonspeed = 90.0f;							// PageUp/PageDown, screen rows per second
} RETRO_TerrainView;

// tan of half the view across: a point at this side per depth sits on the
// screen edge.
inline float RETRO_TerrainViewHalfSlope(void)
{
	return (RETRO_WIDTH * 0.5f) / RETRO_TerrainLens.focalx;
}

// The widest side per depth worth keeping: five percent past the lens, so a
// sample on the edge is projected rather than thrown out.
inline float RETRO_TerrainViewCullSlope(void)
{
	return RETRO_TerrainViewHalfSlope() * 1.05f;
}

//
// Both looks put a point in the camera's frame - right, down and forward of
// the eye - and the lens reads nothing else. Wrapping turns a ground offset
// by the heading; the island pitches, mixing altitude into depth, so a peak
// in front of the camera is nearer than the ground under it. Depth is z, and
// a point at or behind the eye projects with q at or below zero, for the
// caller to drop.
//

// Whether a point in the camera's frame is past the near plane and inside
// the cull wedge: worth projecting on its own, as a dot. A polygon is cut by
// RETRO_ClipProjectViewPolygon instead.
inline bool RETRO_TerrainEyeInView(vec3 eye)
{
	return eye.z > RETRO_TerrainLens.nearplane && fabsf(eye.x) <= eye.z * RETRO_TerrainViewCullSlope();
}

// *******************************************************************
// The wrapping look
// *******************************************************************

//
// The wrapping camera: a position on the torus and a heading
//
// The defaults suit a 1024-cell map: crossed in about a quarter of a minute,
// riding high enough to see over a ridge without losing the ground. A map
// of another size scales them with RETRO_ScaleTerrainWorld.
//
struct RETRO_TerrainWrappingCamera {
	float x = 0;				// Position in map cells, wrapping with the map
	float z = 0;
	float height = 0;			// Above the map's zero, in world units
	float heading = 0;			// Radians, kept in [0, 2pi)
	bool flycam = false;		// Free flight rather than following the ground

	float movespeed = 66.0f;	// Cells per second
	float turnspeed = 1.2f;		// Radians per second
	float flyspeed = 30.0f;		// World units per second, and only in flycam
	float eye = 49.0f;			// Ride height above the ground
	float clearance = 10.0f;	// Closest to the ground the eye may come
	float follow = 0.10f;		// Seconds the ride height takes to close most of a step
};

inline RETRO_TerrainWrappingCamera RETRO_TerrainCamera;

//
// Which way a heading faces, and which way is its right
//
// The view frame - right +x, forward -z - turned by the heading about the
// upright axis:
//
//   R(h) . (-z) = (-sin h, -cos h)   forward
//   R(h) . (+x) = ( cos h, -sin h)   right
//
// so a heading is a right-handed yaw and zero looks along decreasing z. It
// is written once because a sign the wrong way round mirrors the world, and
// a mirrored world renders perfectly and steers backward.
//
struct RETRO_TerrainBasis {
	vec2 forward;	// Where the heading looks, on the ground (x, z)
	vec2 right;		// and its right
};

inline RETRO_TerrainBasis RETRO_TerrainHeadingBasis(float heading)
{
	float sina = sinf(heading);
	float cosa = cosf(heading);

	RETRO_TerrainBasis basis;
	basis.forward = { -sina, -cosa };
	basis.right = { cosa, -sina };
	return basis;
}

//
// The wrapping frustum on the ground at unit depth
//
// Voxel columns are not a pinhole. At depth z the lens meets the ground in
// the segment from z * left to z * right.
//
struct RETRO_TerrainSlice {
	vec2 left;
	vec2 right;
};

inline RETRO_TerrainSlice RETRO_TerrainViewSlice(const RETRO_TerrainBasis &basis)
{
	float slope = RETRO_TerrainViewHalfSlope();
	RETRO_TerrainSlice slice;
	slice.left = basis.forward - basis.right * slope;
	slice.right = basis.forward + basis.right * slope;
	return slice;
}

//
// An offset from the wrapping camera - across x, up, and along z - in its
// own frame
//
// A yaw leaves height alone, so a cell can be judged on its side and depth
// before its height is read. Behind the eye is a negative depth.
//
inline vec3 RETRO_TerrainCameraEye(vec3 offset, const RETRO_TerrainBasis &basis)
{
	vec2 ground = { offset.x, offset.z };
	return { dot(ground, basis.right), -offset.y, dot(ground, basis.forward) };
}

// A world point in the wrapping camera's frame, for a polygon that is to be
// cut at the near plane before it is projected
inline vec3 RETRO_TerrainPointEye(float x, float z, float height, const RETRO_TerrainBasis &basis)
{
	return RETRO_TerrainCameraEye({ x - RETRO_TerrainCamera.x, height - RETRO_TerrainCamera.height, z - RETRO_TerrainCamera.z }, basis);
}

//
// Whether a dot at this squared distance survives thinning
//
// Dots crowd near the eye; most of them would land on pixels already
// covered. Hashing the cell gives it the same answer wherever the camera
// stands, so the pattern is anchored to the ground and does not swim, and
// density falls off smoothly enough that no ring shows. A wrapping map
// hashes the cell folded onto the torus, so it keeps its pattern as the
// camera crosses the seam.
//
inline bool RETRO_KeepTerrainDot(int x, int z, float distance2, float falloff = 110.0f)
{
	if (RETRO_Terrain.wrap) {
		x = WRAP(x, RETRO_Terrain.width);
		z = WRAP(z, RETRO_Terrain.height);
	}
	float random = RETRO_HashUnit(x, z);
	float density = 1.0f / (1.0f + distance2 / (falloff * falloff));
	return random < density;
}

//
// A wrapping cell through the wedge, the thinning and the pinhole
//
// (dx, dz) is the cell's offset from the camera and radius2 its squared
// distance. Most cells around the camera are behind or beside it, and the
// wedge throws them out more cheaply than the hash. Returns whether the dot
// lands on the screen, with its eye for a caller that needs the depth.
//
inline bool RETRO_ProjectTerrainDot(int x, int z, float dx, float dz, float radius2, const RETRO_TerrainBasis &basis, vec3 *eye, PolygonPoint *point)
{
	vec3 cell = RETRO_TerrainCameraEye({ dx, 0, dz }, basis);
	if (!RETRO_TerrainEyeInView(cell)) return false;
	if (!RETRO_KeepTerrainDot(x, z, radius2)) return false;

	cell.y = -(RETRO_TerrainHeight(x, z) - RETRO_TerrainCamera.height);
	*eye = cell;
	*point = RETRO_ProjectViewPoint(RETRO_TerrainLens, cell);
	return RETRO_OnScreen(point->pos.x, point->pos.y);
}

//
// The wrapping mesh walk
//
// The cells within the draw distance, snapped to the mesh spacing. What is
// painted on a cell is the effect's; this is only which cells are worth
// asking about. The heading is turned once, and the distance kept squared.
//
struct RETRO_TerrainMesh {
	int step;					// Mesh spacing, in map cells
	int minx, maxx;				// The cells to walk, snapped to the spacing
	int minz, maxz;
	RETRO_TerrainBasis basis;	// The camera's heading, turned once
	float distance2;			// Draw distance squared
};

inline RETRO_TerrainMesh RETRO_BuildTerrainMesh(void)
{
	int step = RETRO_TerrainView.step;
	int distance = RETRO_TerrainView.distance;

	RETRO_TerrainMesh mesh;
	mesh.step = step;
	mesh.minx = (int)floorf((RETRO_TerrainCamera.x - distance) / step) * step;
	mesh.maxx = (int)ceilf((RETRO_TerrainCamera.x + distance) / step) * step;
	mesh.minz = (int)floorf((RETRO_TerrainCamera.z - distance) / step) * step;
	mesh.maxz = (int)ceilf((RETRO_TerrainCamera.z + distance) / step) * step;
	if (!RETRO_Terrain.wrap) {
		// A finite patch has no cells past its edges, and the walk has no
		// part cells, so the spacing must divide it or the far strip is lost
		if ((RETRO_Terrain.width - 1) % step != 0 || (RETRO_Terrain.height - 1) % step != 0) {
			RETRO_RageQuit("Terrain mesh spacing %d does not divide a %dx%d patch\n", step, RETRO_Terrain.width, RETRO_Terrain.height);
		}
		mesh.minx = MAX(mesh.minx, 0);
		mesh.minz = MAX(mesh.minz, 0);
		mesh.maxx = MIN(mesh.maxx, RETRO_Terrain.width - step);
		mesh.maxz = MIN(mesh.maxz, RETRO_Terrain.height - step);
	}
	mesh.basis = RETRO_TerrainHeadingBasis(RETRO_TerrainCamera.heading);
	mesh.distance2 = (float)distance * distance;
	return mesh;
}

//
// Whether the mesh cell at (x, z) is worth projecting
//
// Judged by its center against the cull wedge widened by the mesh spacing,
// so a cell straddling the screen edge does not blink out. The near plane is
// widened by half a cell's diagonal: under a low eye, a cell centered behind
// it can still reach the bottom of the screen. Whoever draws the cell clips
// what is behind the eye.
//
inline bool RETRO_TerrainCellVisible(const RETRO_TerrainMesh &mesh, int x, int z)
{
	float centerx = x + mesh.step / 2.0f - RETRO_TerrainCamera.x;
	float centerz = z + mesh.step / 2.0f - RETRO_TerrainCamera.z;
	if (centerx * centerx + centerz * centerz > mesh.distance2) return false;

	vec3 eye = RETRO_TerrainCameraEye({ centerx, 0, centerz }, mesh.basis);
	float halfdiagonal = mesh.step * (float)M_SQRT1_2;
	return eye.z + halfdiagonal >= RETRO_TerrainLens.nearplane && fabsf(eye.x) <= eye.z * RETRO_TerrainViewCullSlope() + mesh.step;
}

// A world point in the wrapping camera's frame, seen from basis, as the
// corner of a polygon to be cut at the near plane and projected
inline RETRO_CameraVertex RETRO_TerrainCameraVertex(vec3 p, const RETRO_TerrainBasis &basis)
{
	RETRO_CameraVertex vertex = {};
	vertex.eye = RETRO_TerrainPointEye(p.x, p.z, p.y, basis);
	return vertex;
}

// The same corner with texture coordinates, and lit in color, the light
// carried as the levels of a table lit by RETRO_TintColor
inline RETRO_CameraVertex RETRO_TerrainTintVertex(vec3 p, vec2 uv, vec3 light, const RETRO_ShadeTable &table, const RETRO_TerrainBasis &basis)
{
	RETRO_CameraVertex vertex = RETRO_TerrainCameraVertex(p, basis);
	vertex.uv = uv;
	RETRO_TintLevels(table, light, vertex.c, vertex.tint);
	return vertex;
}

// A polygon of such corners through the near plane and onto the screen,
// textured and lit through the table its light was carried for
inline void RETRO_DrawTerrainPolygon(const RETRO_CameraVertex *vertex, int count, unsigned char *texture, int texturewidth, int textureheight, const RETRO_ShadeTable &table)
{
	PolygonPoint polygon[RETRO_CAMERA_MAX_POLYGON + RETRO_CAMERA_CLIP_PLANES];
	int points = RETRO_ClipProjectViewPolygon(RETRO_TerrainLens, vertex, count, polygon);
	if (points < 3) return;
	RETRO_DrawTexMapGouraudPolygon(polygon, points, texture, texturewidth, textureheight, table);
}

//
// A model standing on the terrain, as the wrapping camera sees it from basis
//
// The model stands at position, turned by rotation and scaled by scale, in
// map cells. Every corner is lit in color by lighting, in the same world, and
// the model is textured and lit through a table lit by RETRO_TintColor, so
// its light can have no blue but white. The faces are depth tested, not
// sorted, so a model that is not convex needs the depth buffer
//
inline void RETRO_DrawTerrainModel(const Model3D *model, const mat3 &rotation, vec3 position, float scale, const RETRO_Lighting &lighting, unsigned char *texture, int texturewidth, int textureheight, const RETRO_ShadeTable &table, const RETRO_TerrainBasis &basis)
{
	for (int i = 0; i < model->faces; i++) {
		const Face *face = &model->face[i];
		RETRO_CameraVertex polygon[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			vec3 p = position + rotation * model->vertex[face->vertex[j]].pos * scale;
			vec3 light = RETRO_ColorLambert(lighting, p, rotation * model->normal[face->vertexnormal[j]].dir);
			polygon[j] = RETRO_TerrainTintVertex(p, model->uv[face->uv[j]], light, table, basis);
		}
		RETRO_DrawTerrainPolygon(polygon, face->vertices, texture, texturewidth, textureheight, table);
	}
}

// The same model in one flat color, unlit
inline void RETRO_DrawTerrainFlatModel(const Model3D *model, const mat3 &rotation, vec3 position, float scale, unsigned char color, const RETRO_TerrainBasis &basis)
{
	for (int i = 0; i < model->faces; i++) {
		const Face *face = &model->face[i];
		RETRO_CameraVertex vertex[RETRO_MAX_FACEVERTICES];
		for (int j = 0; j < face->vertices; j++) {
			vertex[j] = RETRO_TerrainCameraVertex(position + rotation * model->vertex[face->vertex[j]].pos * scale, basis);
		}
		PolygonPoint polygon[RETRO_CAMERA_MAX_POLYGON + RETRO_CAMERA_CLIP_PLANES];
		int points = RETRO_ClipProjectViewPolygon(RETRO_TerrainLens, vertex, face->vertices, polygon);
		if (points < 3) continue;
		RETRO_DrawFlatPolygon(polygon, points, color);
	}
}

//
// Stand the wrapping camera on the ground at (x, z), at its ride height
//
// Worth doing before the first frame: the ride height is followed
// gradually, and a camera left at zero would rise into place from under
// the map while the demo is already being watched.
//
inline void RETRO_PlaceTerrainCamera(float x, float z)
{
	RETRO_TerrainCamera.x = x;
	RETRO_TerrainCamera.z = z;
	RETRO_TerrainCamera.height = RETRO_TerrainHeightLinear(x, z) + RETRO_TerrainCamera.eye;
}

//
// Take the wrapping camera and its draw distance down for a world that is
// this much of the default
//
// Every length comes from a fresh camera and RETRO_TERRAIN_DISTANCE, not
// from the live values, so calling this twice does not scale twice. Turn
// speed is an angle, which a change of scale does not touch.
//
inline void RETRO_ScaleTerrainWorld(float worldscale)
{
	RETRO_TerrainWrappingCamera defaults;
	RETRO_TerrainCamera.movespeed = defaults.movespeed * worldscale;
	RETRO_TerrainCamera.flyspeed = defaults.flyspeed * worldscale;
	RETRO_TerrainCamera.eye = defaults.eye * worldscale;
	RETRO_TerrainCamera.clearance = defaults.clearance * worldscale;
	RETRO_TerrainView.distance = (int)(RETRO_TERRAIN_DISTANCE * worldscale);
}

// Carry the wrapping camera back onto the torus, keeping the fraction: a
// step shorter than a cell would otherwise be truncated away every frame.
inline void RETRO_WrapTerrainCamera(void)
{
	if (RETRO_Terrain.wrap) {
		RETRO_TerrainCamera.x = mod(RETRO_TerrainCamera.x, (float)RETRO_Terrain.width);
		RETRO_TerrainCamera.z = mod(RETRO_TerrainCamera.z, (float)RETRO_Terrain.height);
	}
}

//
// Drive the wrapping camera from the keyboard and settle it on the ground
//
// Left/Right turn and Up/Down move along the view. W/S repeat forward and
// back, A/D strafe, and Tab toggles the flycam, in which R and F raise and
// lower. A diagonal is no faster than a straight line. Tab and not Space:
// the main loop holds the demo still while Space is down.
//
// PageUp and PageDown slide the horizon, held on the screen: past either
// edge there would be nothing but sky or ground to steer by.
//
// The ride height follows the ground exponentially, with the rate taken from
// the timestep, so cresting a ridge does not snap the eye at any frame rate.
// The clearance stops a fast descent putting the eye inside the hill.
//
inline void RETRO_UpdateTerrainCamera(float timestep)
{
	float distance = timestep * RETRO_TerrainCamera.movespeed;
	float rotation = timestep * RETRO_TerrainCamera.turnspeed;

	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) RETRO_TerrainCamera.flycam = !RETRO_TerrainCamera.flycam;
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) RETRO_TerrainCamera.heading += rotation;
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) RETRO_TerrainCamera.heading -= rotation;

	float forward = 0;
	float strafe = 0;
	if (RETRO_KeyState(SDL_SCANCODE_UP) || RETRO_KeyState(SDL_SCANCODE_W)) forward += 1;
	if (RETRO_KeyState(SDL_SCANCODE_DOWN) || RETRO_KeyState(SDL_SCANCODE_S)) forward -= 1;
	if (RETRO_KeyState(SDL_SCANCODE_A)) strafe -= 1;
	if (RETRO_KeyState(SDL_SCANCODE_D)) strafe += 1;
	float length = hypotf(forward, strafe);
	if (length > 0) {
		forward /= length;
		strafe /= length;
		RETRO_TerrainBasis basis = RETRO_TerrainHeadingBasis(RETRO_TerrainCamera.heading);
		vec2 move = (basis.forward * forward + basis.right * strafe) * distance;
		RETRO_TerrainCamera.x += move.x;
		RETRO_TerrainCamera.z += move.y;
	}
	if (RETRO_TerrainCamera.flycam && RETRO_KeyState(SDL_SCANCODE_R)) RETRO_TerrainCamera.height += timestep * RETRO_TerrainCamera.flyspeed;
	if (RETRO_TerrainCamera.flycam && RETRO_KeyState(SDL_SCANCODE_F)) RETRO_TerrainCamera.height -= timestep * RETRO_TerrainCamera.flyspeed;

	if (RETRO_KeyState(SDL_SCANCODE_PAGEUP)) RETRO_TerrainLens.center.y += timestep * RETRO_TerrainView.horizonspeed;
	if (RETRO_KeyState(SDL_SCANCODE_PAGEDOWN)) RETRO_TerrainLens.center.y -= timestep * RETRO_TerrainView.horizonspeed;
	RETRO_TerrainLens.center.y = clamp(RETRO_TerrainLens.center.y, 0.0f, (float)RETRO_HEIGHT);

	RETRO_WrapTerrainCamera();
	RETRO_TerrainCamera.heading = mod(RETRO_TerrainCamera.heading, (float)(2 * M_PI));

	if (!RETRO_TerrainCamera.flycam) {
		float ground = RETRO_TerrainHeightLinear(RETRO_TerrainCamera.x, RETRO_TerrainCamera.z);
		float target = ground + RETRO_TerrainCamera.eye;
		RETRO_TerrainCamera.height = mix(RETRO_TerrainCamera.height, target, 1.0f - expf(-timestep / RETRO_TerrainCamera.follow));
		if (RETRO_TerrainCamera.height < ground + RETRO_TerrainCamera.clearance) RETRO_TerrainCamera.height = ground + RETRO_TerrainCamera.clearance;
	}
}

// *******************************************************************
// The hovering vehicle
// *******************************************************************

//
// A hovering vehicle to fly the wrapping camera with momentum
//
// Up/Down accelerate and friction brings it to rest. Gravity pulls the eye
// down until the ground under the cell it is over pushes it back up and
// lifts the nose; the nose settles back to its resting pitch, and the pitch
// is shown by moving the horizon.
//
// Speeds are in map cells and radians a second. A negative turn speed turns
// the other way, for a world drawn mirrored.
//
struct RETRO_TerrainVehicle {
	float acceleration = 100.0f;	// Speed gained each second a key is held
	float friction = 25.0f;			// Speed lost each second
	float maxspeed = 66.0f;
	float turnspeed = 1.2f;
	float gravity = 40.0f;			// Climb lost each second
	float spring = 22.5f;			// Climb gained each second, per unit of ground above the clearance
	float lift = 10.7f;				// Rate the eye is pushed out of the ground
	float clearance = 49.0f;		// Height the eye rides above the ground
	float floor = 0.0f;				// Lowest the eye can go, on or off the terrain
	float restpitch = 0.17f;		// Radians the nose rests at, looking down
	float pitchkick = 0.01f;		// Raised each second, per unit of ground above the clearance
	float pitchreturn = 0.17f;		// Radians a second, back toward rest

	float speed = 0;				// Forward, now
	float climb = 0;				// Upward
	float pitch = 0.17f;			// Looking down when positive
};

inline RETRO_TerrainVehicle RETRO_Vehicle;

//
// Drive the wrapping camera as RETRO_Vehicle
//
// Hovering, the spring holds the eye a little into the ground, which keeps
// lifting the nose almost as fast as it settles, so the view rides nearly
// level rather than at its resting pitch.
//
inline void RETRO_UpdateTerrainVehicle(float timestep)
{
	RETRO_TerrainVehicle &v = RETRO_Vehicle;
	if (RETRO_KeyState(SDL_SCANCODE_UP)) {
		v.speed = MIN(v.speed + v.acceleration * timestep, v.maxspeed);
	} else if (RETRO_KeyState(SDL_SCANCODE_DOWN)) {
		v.speed = MAX(v.speed - v.acceleration * timestep, -v.maxspeed);
	}
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) RETRO_TerrainCamera.heading += v.turnspeed * timestep;
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) RETRO_TerrainCamera.heading -= v.turnspeed * timestep;
	RETRO_TerrainCamera.heading = mod(RETRO_TerrainCamera.heading, (float)(2 * M_PI));

	// The ground the vehicle is over is the height halfway across its cell,
	// the average of the cell's corners
	int x = (int)floorf(RETRO_TerrainCamera.x), z = (int)floorf(RETRO_TerrainCamera.z);
	if (RETRO_Terrain.wrap || (x >= 0 && z >= 0 && x < RETRO_Terrain.width - 1 && z < RETRO_Terrain.height - 1)) {
		float depth = RETRO_TerrainHeightLinear(x + 0.5f, z + 0.5f) - (RETRO_TerrainCamera.height - v.clearance);
		if (depth > 0) {
			v.climb += depth * v.spring * timestep;
			RETRO_TerrainCamera.height += depth * (1 - expf(-v.lift * timestep));
			v.pitch -= depth * v.pitchkick * timestep;
		}
	}

	v.speed = APPROACH(v.speed, 0, v.friction * timestep);
	v.pitch = APPROACH(v.pitch, v.restpitch, v.pitchreturn * timestep);

	v.climb -= v.gravity * timestep;

	RETRO_TerrainBasis basis = RETRO_TerrainHeadingBasis(RETRO_TerrainCamera.heading);
	RETRO_TerrainCamera.x += basis.forward.x * v.speed * timestep;
	RETRO_TerrainCamera.z += basis.forward.y * v.speed * timestep;
	RETRO_TerrainCamera.height += v.climb * timestep;
	if (RETRO_TerrainCamera.height < v.floor) {
		v.climb = 0;
		RETRO_TerrainCamera.height = v.floor;
	}
	RETRO_WrapTerrainCamera();

	RETRO_TerrainLens.center.y = RETRO_HEIGHT / 2 - RETRO_TerrainLens.focaly * tanf(v.pitch);
}

// *******************************************************************
// The island look
// *******************************************************************

//
// A finite patch on a turntable, seen from outside and pitched down
//
// The camera looks along decreasing z and does not turn; the patch turns
// under it. pitch is a real rotation of the view, so the island fills the
// frame rather than sitting as a ridge on the horizon. The camera dollies
// between stops that keep it outside the patch.
//
struct RETRO_TerrainIsland {
	float x = 0;				// Look-from, looking toward decreasing z
	float z = 0;
	float height = 0;			// Above the map's zero, in world units
	float pitch = 0.70f;		// Radians down from the horizon
	float rotation = 0;			// The patch's turn about its center, radians
	float movespeed = 24.0f;
	float turnspeed = 1.35f;
	float nearestz = 0;			// Dolly stops
	float farthestz = 0;
};

inline RETRO_TerrainIsland RETRO_Island;

//
// The island pose taken once per draw: the spin axis, and the sincos of
// pitch and rotation every sample shares
//
struct RETRO_TerrainIslandFrame {
	float centerx, centerz;		// Patch center, the axis the turntable spins about
	float sinpitch, cospitch;
	float sinrot, cosrot;
};

inline RETRO_TerrainIslandFrame RETRO_BuildTerrainIslandFrame(void)
{
	RETRO_TerrainIslandFrame frame;
	vec3 center = RETRO_TerrainCenter();
	frame.centerx = center.x;
	frame.centerz = center.z;
	frame.sinpitch = sinf(RETRO_Island.pitch);
	frame.cospitch = cosf(RETRO_Island.pitch);
	frame.sinrot = sinf(RETRO_Island.rotation);
	frame.cosrot = cosf(RETRO_Island.rotation);
	return frame;
}

//
// Stand outside the patch, looking down so it fills the frame
//
// Writes the lens as well as the pose. The lens is longer than the wrapping
// one - the patch is small and the camera close, and a wide angle would show
// mostly the ground in between - with the horizon a little above the middle
// so there is sky at the top.
//
// The camera stands over the spin axis, (width - 1) / 2, so the island sits
// on the screen center at every rotation, and high enough that the view
// center lands on the patch center. The near stop clears the patch's
// circumradius, not its south edge: the patch turns, and at 45 degrees a
// corner reaches further than the edge.
//
inline void RETRO_LookDownAtTerrain(void)
{
	RETRO_TerrainLens.focalx = RETRO_WIDTH * 0.54f;
	RETRO_TerrainLens.focaly = RETRO_HEIGHT * 0.78f;
	RETRO_TerrainLens.center.y = RETRO_HEIGHT * 0.46f;
	RETRO_TerrainLens.nearplane = 4.0f;

	// A patch large enough that its circumradius reaches past the far stop
	// has the two stops meet there, and the camera starts between them
	vec3 center = RETRO_TerrainCenter();
	RETRO_Island.rotation = 0;
	RETRO_Island.pitch = 0.70f;
	RETRO_Island.nearestz = center.z + hypotf(center.x, center.z) + RETRO_TerrainLens.nearplane;
	RETRO_Island.farthestz = MAX(RETRO_Terrain.height + 70.0f, RETRO_Island.nearestz);
	RETRO_Island.x = center.x;
	RETRO_Island.z = clamp(RETRO_Terrain.height + 35.0f, RETRO_Island.nearestz, RETRO_Island.farthestz);
	RETRO_Island.height = RETRO_TerrainHeight(center.x, center.z) + tanf(RETRO_Island.pitch) * (RETRO_Island.z - center.z);
}

// A map point at world height y in the island camera's frame: the patch
// turned about its center, then the view pitched down.
inline vec3 RETRO_TerrainIslandEye(float x, float y, float z, const RETRO_TerrainIslandFrame &frame)
{
	float localx = x - frame.centerx;
	float localz = z - frame.centerz;
	float worldx = frame.centerx + localx * frame.cosrot - localz * frame.sinrot;
	float worldz = frame.centerz + localx * frame.sinrot + localz * frame.cosrot;

	float horizontal = RETRO_Island.z - worldz;
	float vertical = y - RETRO_Island.height;
	vec3 eye;
	eye.x = worldx - RETRO_Island.x;
	eye.y = -(vertical * frame.cospitch + horizontal * frame.sinpitch);
	eye.z = horizontal * frame.cospitch - vertical * frame.sinpitch;
	return eye;
}

// Drive the island from the keyboard: Left/Right turn the patch, Up/Down
// (or W/S) dolly between the stops RETRO_LookDownAtTerrain set.
inline void RETRO_UpdateTerrainIsland(float timestep)
{
	float distance = timestep * RETRO_Island.movespeed;
	float rotation = timestep * RETRO_Island.turnspeed;
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) RETRO_Island.rotation += rotation;
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) RETRO_Island.rotation -= rotation;
	if (RETRO_KeyState(SDL_SCANCODE_UP) || RETRO_KeyState(SDL_SCANCODE_W)) RETRO_Island.z -= distance;
	if (RETRO_KeyState(SDL_SCANCODE_DOWN) || RETRO_KeyState(SDL_SCANCODE_S)) RETRO_Island.z += distance;
	RETRO_Island.z = clamp(RETRO_Island.z, RETRO_Island.nearestz, RETRO_Island.farthestz);
	RETRO_Island.rotation = mod(RETRO_Island.rotation, (float)(2 * M_PI));
}

#endif
