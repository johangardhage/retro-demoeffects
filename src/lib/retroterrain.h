//
// Retro terrain library
//
// A height field, the lens it is looked through, and the two looks the
// effects here take of it. The map and the lens are the same either way;
// what differs is how the camera stands and how a point on the ground
// reaches the screen.
//
// World x runs across the map, z along it, and height is up. A stored
// height is a byte; scale is the world units that byte is worth.
//
// RETRO_Terrain is the map: two planes of bytes, their size, whether the
// edges wrap, and that scale. RETRO_TerrainView is the lens, and also how
// finely and how far the ground is paid for. A point in the camera's own
// frame is RETRO_TerrainEye - right, up, forward - and
// RETRO_ProjectTerrainView is the pinhole. Both looks produce an Eye;
// neither writes the pinhole again.
//
// The wrapping look is a yaw and a horizon. RETRO_Camera walks the torus.
// RETRO_TerrainHeadingBasis turns a heading into forward and right;
// RETRO_TerrainCameraOffset is that turn read back, still on the ground,
// so a wedge can throw a cell out before its height is read. Voxel columns
// take a frustum slice at each depth instead of a pinhole. PageUp slides
// the horizon, which tilts the picture without pitching the camera, so
// those columns still work.
//
// The island look is a finite patch on a turntable, seen from outside and
// pitched down. RETRO_Island is the pose; RETRO_TerrainIslandFrame is that
// pose taken once per draw. RETRO_TerrainIslandEye is the turntable and
// the pitch. Left and Right turn the patch, not the camera.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//

#ifndef _RETROTERRAIN_H_
#define _RETROTERRAIN_H_

#include "retro.h"

// How far the wrapping look draws, in map cells. A wide view has to reach
// this far to fill itself: it flattens the hills that used to hide where
// the ground stops, so the draw distance goes from a cost nobody sees to
// an edge against the sky.
#define RETRO_TERRAIN_DISTANCE 800

//
// The map
//
// Two planes of bytes and the size they are read at. It points at them
// rather than copying them, so a map loaded from a file and one the effect
// built for itself are described the same way and neither is moved to get
// here. The planes and not the images they came from: what the sampling
// wants is the bytes, and reaching them through an image every time is a
// lookup in a list to arrive somewhere fixed. Naming them also leaves
// nothing positional to get wrong - which map is which is said outright,
// instead of following from the order two loads happened to run in.
//
// wrap is what the edges do. A wrapping map is a torus, which is what lets
// a camera travel in one direction forever. A finite patch has a last cell,
// and a sample past it is that cell, not the other side of the island.
//
// width and height are the map in cells, across x and along z. Camera
// height is something else: altitude above the map's zero.
//
// Only one terrain is described at a time. An effect that wants a second
// map reads it itself; what is here is the sampling every effect over a
// height field repeats.
//
struct {
	unsigned char *heightmap = NULL;	// One byte of altitude per cell
	unsigned char *colormap = NULL;		// The color painted over it, one byte per cell
	int width = 0;						// Map size in cells, across x
	int height = 0;						// and along z
	bool wrap = true;					// Both axes wrap; a finite patch sets this false
	float scale = 1.0f;					// World units per stored height value
} RETRO_Terrain;

//
// The lens, and how the ground is paid for
//
// One screen, so one of these. The lens is the focals and the horizon: they
// are what RETRO_ProjectTerrainView reads. step and distance are draw policy,
// not a lens: they say how finely and how far a mesh or a column walk spends
// itself. How wide a cell may sit, in side per depth, and still be kept is
// derived from the lens, not stored beside it. horizonspeed is input, and
// lives here because it moves the lens.
//
// The defaults are the wrapping look: wide, near enough a right angle across,
// and taller than it is wide. The pixels are square - the window letterboxes
// a 320x240 framebuffer - so a shorter vertical focal is not an aspect
// correction. It is a taller view: about a hundred degrees down against
// ninety across, which flattens the hills and puts more ground below the
// horizon. The focal lengths are what decide how fast walking feels, more
// than the speed does: a narrow view shows little of the ground streaming
// past, so the same cells per second read as a trudge through one and a run
// through the other. The island look writes its own, longer: the patch is
// small and the camera is close, and a wide angle would show mostly the
// ground between here and there.
//
// Sliding the horizon tilts the picture without pitching the camera. That is
// what PageUp does for the wrapping look, and why voxel columns can still
// treat each screen row as a constant-depth slice of the ground. A real
// pitch is the island's, and is on RETRO_Island, not here.
//
struct {
	int step = 6;										// Mesh spacing, in map cells. Draw policy
	int distance = RETRO_TERRAIN_DISTANCE;				// How far the wrapping look draws, in map cells
	float nearplane = 3.0f;								// Nearest depth worth drawing
	float focalx = RETRO_WIDTH * 0.5f;					// Focal length across, half the width for a 90 degree view
	float focaly = RETRO_HEIGHT * 0.42f;				// and down: shorter, so the view is taller than it is wide
	float horizon = RETRO_HEIGHT * 0.43f;				// Where eye level lands on the screen
	float horizonspeed = 90.0f;							// PageUp/PageDown, screen rows per second
} RETRO_TerrainView;

//
// A projected point on the screen
//
// q is 1/depth, which is what a mesh interpolates across a face so
// perspective stays correct. Larger q is nearer. A point at or behind the
// eye comes back with q at or below zero for the caller to drop.
//
struct RETRO_TerrainPoint {
	float sx, sy, q;
};

//
// The sun the ground is lit by
//
// A direction toward the light, not necessarily unit: Shade divides the length
// out, the way it does the normal's. The elevation is what decides how much of
// a ramp the ground can reach: a sun overhead lights every gentle slope alike,
// and a height field is mostly gentle slopes, so the shading collapses onto the
// bright end. This one stands about forty degrees up, far enough off vertical
// to tell a slope facing it from one turned away. A future edit of the
// direction therefore cannot silently rescale the whole ramp.
//
struct {
	float x = -0.50f;
	float y = 0.62f;
	float z = -0.60f;
} RETRO_TerrainLight;

//
// Which way a heading faces, and which way is its right
//
// The pair is the view frame every renderer starts from - right +x, forward -z -
// turned by the heading about the upright axis:
//
//   R(h) . (-z) = (-sin h, -cos h)   forward
//   R(h) . (+x) = ( cos h, -sin h)   right
//
// so a heading is an ordinary right-handed yaw and zero looks along decreasing
// map z. Everything below that works in camera terms is this turn or its
// inverse. It is written here once because it is the kind of thing that fails
// quietly: a sign the wrong way round mirrors the world, and a mirrored world
// renders perfectly and steers backwards.
//
struct RETRO_TerrainBasis {
	float forwardx, forwardz;	// Where the heading looks, on the ground
	float rightx, rightz;		// and its right, so a heading is a view frame
};

//
// The wrapping frustum on the ground at unit depth
//
// Voxel columns are not a pinhole. At depth z the lens meets the ground in
// a segment whose ends are z times these two points. Scale by z for the
// slice at that depth; the column walks do that and nothing else with them.
//
struct RETRO_TerrainSlice {
	float leftx, leftz;
	float rightx, rightz;
};

//
// A ground offset in the wrapping camera's own terms
//
// The yaw read the other way: how far to the right of the camera the offset
// lies, and how far in front. Altitude is not mixed in - a yaw leaves height
// alone - so a wedge test can throw a cell out before its height is read.
// Behind the eye comes back as a negative depth rather than being folded
// round to a positive one. The island look has no equivalent: its pitch
// mixes altitude into depth, so the camera-space point is an Eye from the
// start.
//
struct RETRO_TerrainOffset {
	float side, depth;
};

//
// A point in the camera's own frame
//
// side is right of the eye, height is up, depth is forward. Both looks
// produce one of these and the pinhole reads nothing else.
//
// Wrapping arrives from a yaw: side and depth are the ground Offset turned
// by the heading, height is the world altitude minus the eye. The island
// arrives from a turntable and a pitch, which mix altitude into depth, so
// a peak in front of the camera is nearer than the ground under it.
//
struct RETRO_TerrainEye {
	float side, height, depth;
};

//
// The wrapping look: a yaw, a horizon, and a ride that follows the ground
//
// The pose is what an effect reads; the rest is how it is flown, and is
// here so that driving it is one call. This camera turns. The island look
// is RETRO_Island, which does not: the patch turns under it.
//
// The defaults are for the 1024-cell maps the effects here fly over: fast
// enough to cross one in about a quarter of a minute, and riding high
// enough to see over a ridge without losing the ground. An effect over a
// map of a different size sets its own, since what matters is how long the
// map takes to cross and that follows from how many cells it has.
//
struct RETRO_TerrainCamera {
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

//
// The wrapping camera
//
// An effect flying one wrapping camera never has to name it, the way the
// terrain and the lens are not named either.
//
RETRO_TerrainCamera RETRO_Camera;

//
// The wrapping mesh walk
//
// Drawing a height field as a mesh is the same walk whatever is painted on
// it: the cells within the draw distance, stepped at the mesh spacing, minus
// the ones too far, behind the eye, or off to the side. What an effect does
// with a cell begins at its four corners, so the walk stops short of them
// and hands back only what deciding a cell needs. The island look has no
// mesh walk: it is a finite patch drawn as dots.
//
// The heading is taken once here rather than per cell, and the draw distance
// is kept in the form the test wants it in: squared, so a cell can be measured
// against it without taking a root.
//
struct RETRO_TerrainMesh {
	int step;					// Mesh spacing, in map cells
	int minx, maxx;				// The cells to walk, snapped to the spacing
	int minz, maxz;
	RETRO_TerrainBasis basis;	// The camera's heading, turned once
	float distance2;			// Draw distance squared
};

//
// The island look: a finite patch on a turntable, seen from outside
//
// The camera looks along decreasing z and does not turn; the patch turns
// under it. Left and Right are that turn. Up and Down dolly along the
// viewing axis, between stops that keep the patch in frame: this landscape
// has edges, and the camera is not allowed past them.
//
// pitch is how far it looks down, so the island fills the frame rather
// than sitting as a ridge on the horizon. That is a real rotation of the
// view, not a slide of RETRO_TerrainView.horizon. The pose is this; the
// turn taken once per draw is RETRO_TerrainIslandFrame, the way a heading's
// basis is taken once for the wrapping look.
//
struct RETRO_TerrainIsland {
	float x = 0;				// Look-from, looking toward decreasing z
	float z = 0;
	float height = 0;			// Above the map's zero, in world units
	float pitch = 0.70f;		// Radians down from the horizon
	float rotation = 0;			// The patch's turn about its centre, radians
	float movespeed = 24.0f;
	float turnspeed = 1.35f;
	float nearestz = 0;			// Dolly stops, from the circumcircle and the far side
	float farthestz = 0;
};

//
// The island look taken once
//
// The wrapping equivalent is RETRO_TerrainBasis: a heading turned once so
// every sample does not turn it again. The pose is RETRO_Island; this is
// that pose as a frame - the patch centre, and the sincos of pitch and
// rotation.
//
struct RETRO_TerrainIslandFrame {
	float centerx, centerz;		// Patch centre, the axis the turntable spins about
	float sinpitch, cospitch;
	float sinrot, cosrot;
};

//
// The island camera
//
// An effect looking at one patch never has to name it. RETRO_LookDownAtTerrain
// stands this pose and writes the lens to match; RETRO_UpdateTerrainIsland
// drives it.
//
RETRO_TerrainIsland RETRO_Island;

// Point the sampling at these planes. wrap false is a finite patch: a
// sample past the edge is the last cell, not the other side of the map.
void RETRO_SetTerrain(int width, int height, float scale, unsigned char *heightmap, unsigned char *colormap, bool wrap = true)
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
// Which image holds which map is the sampling's own business, so the loads
// belong with it: taken apart, a caller loading them the other way round would
// read color as altitude and nothing would say so.
//
// How many cells the map has is the map's own business too, and is taken from
// the image rather than asked for. A caller cannot then name a size the file
// does not have, which is a mistake nothing downstream could catch: every
// sample would be read at the wrong stride, off the end of a smaller map.
//
// The two files have to agree on that size. It is taken from the height map,
// and the colour plane is then indexed with that stride: a smaller colour map
// would be read off the end, and nothing downstream would say so.
//
// The scale defaults to leaving the stored byte as the world height, which is
// what an effect wants when it reads the map itself rather than through the
// height calls here, and there is no scale for it to disagree with. The map
// wraps: a file is a torus until a caller says otherwise.
//
void RETRO_LoadTerrain(const char *colorfile, const char *heightfile, float scale = 1.0f)
{
	RETRO_Image *colormap = RETRO_LoadImage(colorfile, true);
	RETRO_Image *heightmap = RETRO_LoadImage(heightfile);
	if (colormap->width != heightmap->width || colormap->height != heightmap->height) {
		RETRO_RageQuit("Terrain color and height maps must be the same size\n");
	}
	RETRO_SetTerrain(heightmap->width, heightmap->height, scale, heightmap->data, colormap->data);
}

// tan of half the view: a point at this side-per-depth sits on the screen
// edge. The voxel slice half-width at depth z is z times this, so the
// column walk and the wrapping pinhole share one number. After LookDown
// the island lens is narrower, and this answers for that lens instead.
float RETRO_TerrainViewHalfSlope(void)
{
	return (RETRO_WIDTH * 0.5f) / RETRO_TerrainView.focalx;
}

// How wide a cell may sit, in side per depth, and still be kept: five
// percent wider than the lens, so a sample on the edge is projected
// rather than thrown out before it is. Derived from the focals, so a
// change of lens - LookDown's, or a caller writing focalx - is this
// number too, and not a copy that would go stale beside it.
float RETRO_TerrainViewCullSlope(void)
{
	return RETRO_TerrainViewHalfSlope() * 1.05f;
}

//
// Whether a sample at this distance survives thinning
//
// Points drawn over a wide area crowd near the eye and thin out with distance
// on their own; keeping every one of them wastes most of the work on samples
// that land on a pixel already covered. Hashing the cell rather than counting
// gives each one the same answer wherever the camera stands, so the pattern is
// anchored to the ground and does not swim, and the density falls smoothly
// enough that no ring marks where it changed.
//
// A wrapping map hashes the cell on the torus, not the collector's unwrapped
// loop coordinate. The camera itself wraps each frame, and without that fold
// the scan range would jump by a map width at the seam and every cell would
// be re-addressed. The same ground behind the camera and a map-width ahead -
// both in view on a torus whose draw distance is most of its size - would
// also carry two patterns at once.
//
// The distance is taken squared. It only ever appears squared here, so asking
// for the root would be asking the caller to undo a step it had already done:
// the falloff is squared once instead, and no sample pays for one.
//
bool RETRO_KeepTerrainDot(int x, int z, float distance2, float falloff = 110.0f)
{
	if (RETRO_Terrain.wrap) {
		x = WRAP(x, RETRO_Terrain.width);
		z = WRAP(z, RETRO_Terrain.height);
	}
	unsigned int hash = (unsigned int)x * 374761393u + (unsigned int)z * 668265263u;
	hash = (hash ^ (hash >> 13)) * 1274126177u;
	float random = (hash & 65535u) / 65535.0f;
	float density = 1.0f / (1.0f + distance2 / (falloff * falloff));
	return random < density;
}

//
// Whether a per-pixel wrap can be a mask
//
// Wrapping a coordinate is a mask when the map wraps and both sides are
// powers of two, and a division when they are not, and an effect wrapping
// one per pixel cares which. Asking here rather than writing the map's size
// out as a constant is the difference between an effect that takes its sizes
// from the image and one that has been told what they will be: swap the
// asset and this answers for the new one, or says no.
//
bool RETRO_TerrainWrapsByMask(void)
{
	int width = RETRO_Terrain.width;
	int height = RETRO_Terrain.height;

	return RETRO_Terrain.wrap && width > 0 && (width & (width - 1)) == 0 && height > 0 && (height & (height - 1)) == 0;
}

// The stored cell under (x, z), unscaled. A wrapping map takes any
// coordinate as a cell; a finite one holds a sample past the edge on the
// last cell. Nearest, not filtered: HeightLinear is the continuous read.
int RETRO_TerrainIndex(float x, float z)
{
	int width = RETRO_Terrain.width;
	int height = RETRO_Terrain.height;
	if (RETRO_Terrain.wrap) {
		return WRAP(z, height) * width + WRAP(x, width);
	}
	return CLAMP(z, 0, height) * width + CLAMP(x, 0, width);
}

// The stored height at a cell, unscaled. RETRO_TerrainHeight is this in
// world units.
unsigned char RETRO_TerrainSample(float x, float z)
{
	return RETRO_Terrain.heightmap[RETRO_TerrainIndex(x, z)];
}

//
// The ground height at a cell, in world units
//
float RETRO_TerrainHeight(float x, float z)
{
	return RETRO_TerrainSample(x, z) * RETRO_Terrain.scale;
}

//
// The ground height between cells, bilinearly filtered
//
// A camera riding the ground needs a height that varies continuously with it,
// which the cell values on their own do not: crossing a cell boundary would
// step. The scale is applied once at the end rather than to each corner, so the
// filter runs on the stored values and costs one multiply.
//
// The four corners fold the way the map does. Wrapping mixes a sample past the
// edge with the other side of the torus; clamping repeats the last cell, which
// is what a letter sitting on the shore of a finite island wants instead of
// the opposite coast.
//
float RETRO_TerrainHeightLinear(float x, float z)
{
	unsigned char *heightmap = RETRO_Terrain.heightmap;
	int width = RETRO_Terrain.width;
	int height = RETRO_Terrain.height;
	int ix = floorf(x);
	int iz = floorf(z);
	float fx = x - ix;
	float fz = z - iz;

	int x0, x1, z0, z1;
	if (RETRO_Terrain.wrap) {
		x0 = WRAP(ix, width);
		x1 = WRAP(ix + 1, width);
		z0 = WRAP(iz, height) * width;
		z1 = WRAP(iz + 1, height) * width;
	} else {
		x0 = CLAMP(ix, 0, width);
		x1 = CLAMP(ix + 1, 0, width);
		z0 = CLAMP(iz, 0, height) * width;
		z1 = CLAMP(iz + 1, 0, height) * width;
	}

	float h00 = heightmap[z0 + x0];
	float h10 = heightmap[z0 + x1];
	float h01 = heightmap[z1 + x0];
	float h11 = heightmap[z1 + x1];

	float top = h00 + fx * (h10 - h00);
	float bottom = h01 + fx * (h11 - h01);
	return (top + fz * (bottom - top)) * RETRO_Terrain.scale;
}

//
// A wrapping map read between its cells, bilinearly filtered
//
// This is the voxel hot path: both planes are read the same way, neither is
// a height until the caller says so, and the fold is a mask. HeightLinear
// is the other filter - it honours wrap versus clamp, and it applies the
// scale - and is what a camera riding the ground uses. A column walk that
// has already insisted the sides are powers of two can spend a mask per
// pixel instead.
//
// The four corners are read once each and the two rows folded once each:
// written as four samples the two left-hand corners appear twice over, and
// every sample repeats the fold on its own. floor and not a cast: a walk
// runs negative wherever the camera looks back across the map's origin,
// and truncating toward zero would fold the cell either side of it onto
// the same sample.
//
float RETRO_TerrainSampleLinear(const unsigned char *map, float x, float z)
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

	float top = map[z0 + x0] + fx * (map[z0 + x1] - map[z0 + x0]);
	float bottom = map[z1 + x0] + fx * (map[z1 + x1] - map[z1 + x0]);
	return top + fz * (bottom - top);
}

// The color painted on a cell. Nearest, and folded the way the map wraps.
unsigned char RETRO_TerrainColor(float x, float z)
{
	return RETRO_Terrain.colormap[RETRO_TerrainIndex(x, z)];
}

//
// A smaller copy of the current terrain, sampled at the centre of each region
//
// How big that region is comes from the loaded map rather than from a size
// named here: naming one the file does not have would read every row at the
// wrong stride, and nothing downstream could tell.
//
// The copy is written into the buffers it is handed. It does not become the
// current terrain: an island that wants to be the map points sampling at
// the copy afterwards, and says it does not wrap.
//
void RETRO_DownsampleTerrain(unsigned char *heightmap, unsigned char *colormap, int width, int height)
{
	int scalex = RETRO_Terrain.width / width;
	int scalez = RETRO_Terrain.height / height;

	for (int z = 0; z < height; z++) {
		for (int x = 0; x < width; x++) {
			int sourcex = x * scalex + scalex / 2;
			int sourcez = z * scalez + scalez / 2;
			int index = z * width + x;
			heightmap[index] = RETRO_TerrainSample(sourcex, sourcez);
			colormap[index] = RETRO_TerrainColor(sourcex, sourcez);
		}
	}
}

//
// The shade a surface facing this way takes, as an index into `shades` levels
//
// Neither vector need be unit: both lengths divide out, which lets a caller
// pass a cross product or a central difference straight in, and lets a
// direction written on RETRO_TerrainLight stay a direction rather than a
// scale on the ramp.
//
// Lambert lands in [-1, 1], but a height field cannot use the whole of it: its
// normals all point upward, and the darkest such a normal can come is a
// vertical wall turned from the sun, at -sqrt(1 - y^2) for a unit light. Mapping
// that band, and not [-1, 1], spends the ramp on shades the ground can actually
// take instead of reserving most of it for directions no face here ever faces.
// The band is still mapped whole rather than clipped at the terminator, so a
// face turned away darkens instead of dropping to flat black.
//
int RETRO_TerrainShade(float nx, float ny, float nz, int shades)
{
	float nlength = sqrtf(nx * nx + ny * ny + nz * nz);
	float lx = RETRO_TerrainLight.x;
	float ly = RETRO_TerrainLight.y;
	float lz = RETRO_TerrainLight.z;
	float llength = sqrtf(lx * lx + ly * ly + lz * lz);
	float light = (nx * lx + ny * ly + nz * lz) / (nlength * llength);
	float uy = ly / llength;
	float darkest = -sqrtf(1.0f - uy * uy);
	return CLAMP((int)((light - darkest) / (1.0f - darkest) * shades), 0, shades);
}

// The wrapping view frame for this heading. Zero looks along decreasing z;
// see RETRO_TerrainBasis.
RETRO_TerrainBasis RETRO_TerrainHeadingBasis(float heading)
{
	float sina = sinf(heading);
	float cosa = cosf(heading);

	RETRO_TerrainBasis basis;
	basis.forwardx = -sina;
	basis.forwardz = -cosa;
	basis.rightx = cosa;
	basis.rightz = -sina;
	return basis;
}

// The wrapping frustum on the ground at unit depth. Scale by z for the
// slice the column walk fills at that depth; see RETRO_TerrainSlice.
RETRO_TerrainSlice RETRO_TerrainViewSlice(const RETRO_TerrainBasis &basis)
{
	float slope = RETRO_TerrainViewHalfSlope();
	RETRO_TerrainSlice slice;
	slice.leftx = basis.forwardx - slope * basis.rightx;
	slice.leftz = basis.forwardz - slope * basis.rightz;
	slice.rightx = basis.forwardx + slope * basis.rightx;
	slice.rightz = basis.forwardz + slope * basis.rightz;
	return slice;
}

// A ground displacement in the wrapping camera's terms. Altitude is not
// mixed in, so a wedge can run before a height is read; see RETRO_TerrainOffset.
RETRO_TerrainOffset RETRO_TerrainCameraOffset(float dx, float dz, const RETRO_TerrainBasis &basis)
{
	RETRO_TerrainOffset offset;
	offset.side = dx * basis.rightx + dz * basis.rightz;
	offset.depth = dx * basis.forwardx + dz * basis.forwardz;
	return offset;
}

//
// The pinhole
//
// Both looks land on these three lines. Wrapping arrives with a yaw, the
// island with a pitch, and neither writes the pinhole again. The caller
// decides what is worth drawing - a wedge, a near plane - then hands the
// Eye in. A point at or behind the eye comes back with q at or below zero
// for them to drop.
//
RETRO_TerrainPoint RETRO_ProjectTerrainView(const RETRO_TerrainEye &eye)
{
	RETRO_TerrainPoint point;
	point.q = 1.0f / eye.depth;
	point.sx = RETRO_WIDTH / 2.0f + RETRO_TerrainView.focalx * eye.side * point.q;
	point.sy = RETRO_TerrainView.horizon - RETRO_TerrainView.focaly * eye.height * point.q;
	return point;
}

//
// The wrapping pinhole for a ground offset already taken
//
// Wrapping dots have the Offset from the wedge test and should not take
// the yaw twice. World height becomes Eye height here, then the pinhole.
//
RETRO_TerrainPoint RETRO_ProjectTerrainOffset(const RETRO_TerrainOffset &offset, float height)
{
	RETRO_TerrainEye eye;
	eye.side = offset.side;
	eye.height = height - RETRO_Camera.height;
	eye.depth = offset.depth;
	return RETRO_ProjectTerrainView(eye);
}

//
// A wrapping cell through the wedge, the thinning, and the pinhole
//
// The two wrapping-dot effects share this: most of the square around the
// camera lies behind or beside it, and four multiplies throw a cell out
// more cheaply than a hash does. How wide the wedge stands comes from the
// lens, so a cell is kept because the view can reach it and not because a
// number here was once measured against a view. The thinning follows, then
// the pinhole, then the screen. The caller still walks the square - it is
// the one that knows what to do with a surviving sample - and is handed
// back the Offset it already had from the wedge, so a depth buffer or a
// ray march does not take the yaw again.
//
bool RETRO_ProjectTerrainDot(int x, int z, float dx, float dz, float radius2, const RETRO_TerrainBasis &basis, RETRO_TerrainOffset *offset, RETRO_TerrainPoint *point)
{
	*offset = RETRO_TerrainCameraOffset(dx, dz, basis);
	if (offset->depth <= RETRO_TerrainView.nearplane || fabsf(offset->side) > offset->depth * RETRO_TerrainViewCullSlope()) return false;
	if (!RETRO_KeepTerrainDot(x, z, radius2)) return false;

	*point = RETRO_ProjectTerrainOffset(*offset, RETRO_TerrainHeight(x, z));
	int sx = (int)point->sx;
	int sy = (int)point->sy;
	return sx >= 0 && sx < RETRO_WIDTH && sy >= 0 && sy < RETRO_HEIGHT;
}

//
// A world point through the wrapping camera and its lens
//
// Translation to the eye, a yaw, then the pinhole. A point at or behind
// the eye comes back with q at or below zero for the caller to drop,
// rather than being folded through the eye into a plausible-looking
// position. The island look is RETRO_TerrainIslandEye, then the same
// pinhole.
//
RETRO_TerrainPoint RETRO_ProjectTerrainPoint(float x, float z, float height, const RETRO_TerrainBasis &basis)
{
	return RETRO_ProjectTerrainOffset(RETRO_TerrainCameraOffset(x - RETRO_Camera.x, z - RETRO_Camera.z, basis), height);
}

//
// The wrapping pinhole for a point sitting on the ground.
RETRO_TerrainPoint RETRO_ProjectTerrainVertex(float x, float z, const RETRO_TerrainBasis &basis)
{
	return RETRO_ProjectTerrainPoint(x, z, RETRO_TerrainHeight(x, z), basis);
}

// The wrapping mesh walk for the current camera and view. What is painted
// on a cell is the effect's; this is only which cells are worth asking.
RETRO_TerrainMesh RETRO_BuildTerrainMesh(void)
{
	int step = RETRO_TerrainView.step;
	int distance = RETRO_TerrainView.distance;

	RETRO_TerrainMesh mesh;
	mesh.step = step;
	mesh.minx = (int)floorf((RETRO_Camera.x - distance) / step) * step;
	mesh.maxx = (int)ceilf((RETRO_Camera.x + distance) / step) * step;
	mesh.minz = (int)floorf((RETRO_Camera.z - distance) / step) * step;
	mesh.maxz = (int)ceilf((RETRO_Camera.z + distance) / step) * step;
	mesh.basis = RETRO_TerrainHeadingBasis(RETRO_Camera.heading);
	mesh.distance2 = (float)distance * distance;
	return mesh;
}

//
// Whether the wrapping cell at x, z is worth projecting
//
// The cell is judged by its centre against a wedge wider than the screen's
// own, widened again by the mesh spacing, so that one straddling the edge
// is drawn rather than blinking out at the border. The test is on the
// ground Offset, not the Eye: a yaw leaves height alone, so the height of
// the cell can wait until the cell has been kept.
//
bool RETRO_TerrainCellVisible(const RETRO_TerrainMesh &mesh, int x, int z)
{
	float centrex = x + mesh.step / 2.0f - RETRO_Camera.x;
	float centrez = z + mesh.step / 2.0f - RETRO_Camera.z;
	if (centrex * centrex + centrez * centrez > mesh.distance2) return false;

	RETRO_TerrainOffset offset = RETRO_TerrainCameraOffset(centrex, centrez, mesh.basis);
	return offset.depth >= RETRO_TerrainView.nearplane && fabsf(offset.side) <= offset.depth * RETRO_TerrainViewCullSlope() + mesh.step;
}

//
// Whether all three corners came back in front of the eye
//
// A corner behind it is projected through the eye and folds onto the screen as
// a plausible-looking shape in the wrong place, so the triangle is dropped. The
// test is on the triangle and not on the cell it was split from: a cell the eye
// sits inside has one half in front of it and one behind, and judging the cell
// would throw both away and leave a hole in the foreground.
//
bool RETRO_TerrainTriangleProjects(float q0, float q1, float q2)
{
	return q0 > 0 && q1 > 0 && q2 > 0;
}

//
// Wrap a position into [0, size), keeping its fractional part
//
// WRAP answers which cell a coordinate falls in and so returns an integer. A
// camera being carried across the torus needs the fraction kept: a step
// shorter than one unit would otherwise be truncated away every frame and
// the movement stall.
//
float RETRO_WrapCoordinate(float coordinate, float size)
{
	coordinate = fmodf(coordinate, size);
	return coordinate < 0 ? coordinate + size : coordinate;
}

//
// Drive the wrapping camera from the keyboard and settle it on the ground
//
// Left/Right turn and Up/Down move along the viewing direction. W/S repeat
// forward and back, A/D strafe, and Tab toggles the flycam, in which R and F
// raise and lower. PageUp and PageDown slide eye level up and down the screen,
// which tilts the view without moving the camera. Combined movement is
// normalized so a diagonal is no faster than a straight line.
//
// Tab and not Space: the main loop holds the whole demo still while Space is
// down, so a camera on that key would freeze the picture it was meant to move
// and only turn over on the release.
//
// Tilting the wrapping view is done by sliding eye level, so it is the
// lens that moves and not the camera. That is not a pitch: voxel columns
// still treat each screen row as a constant-depth slice of the ground.
// It is held on the screen: past either edge the ground is either all sky
// or all ground, and there is nothing to steer by while finding the way
// back.
//
// Following the ground is exponential rather than rigid, so cresting a ridge
// does not snap the eye. The step is taken from the timestep, which keeps the
// approach the same at any frame rate. A floor under it stops a fast descent
// putting the eye inside the hill.
//
void RETRO_UpdateTerrainCamera(float timestep)
{
	float distance = timestep * RETRO_Camera.movespeed;
	float rotation = timestep * RETRO_Camera.turnspeed;

	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) RETRO_Camera.flycam = !RETRO_Camera.flycam;
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) RETRO_Camera.heading += rotation;
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) RETRO_Camera.heading -= rotation;

	float forward = 0;
	float strafe = 0;
	if (RETRO_KeyState(SDL_SCANCODE_UP) || RETRO_KeyState(SDL_SCANCODE_W)) forward += 1;
	if (RETRO_KeyState(SDL_SCANCODE_DOWN) || RETRO_KeyState(SDL_SCANCODE_S)) forward -= 1;
	if (RETRO_KeyState(SDL_SCANCODE_A)) strafe -= 1;
	if (RETRO_KeyState(SDL_SCANCODE_D)) strafe += 1;
	float length = sqrtf(forward * forward + strafe * strafe);
	if (length > 0) {
		forward /= length;
		strafe /= length;
		RETRO_TerrainBasis basis = RETRO_TerrainHeadingBasis(RETRO_Camera.heading);
		RETRO_Camera.x += (basis.forwardx * forward + basis.rightx * strafe) * distance;
		RETRO_Camera.z += (basis.forwardz * forward + basis.rightz * strafe) * distance;
	}
	if (RETRO_Camera.flycam && RETRO_KeyState(SDL_SCANCODE_R)) RETRO_Camera.height += timestep * RETRO_Camera.flyspeed;
	if (RETRO_Camera.flycam && RETRO_KeyState(SDL_SCANCODE_F)) RETRO_Camera.height -= timestep * RETRO_Camera.flyspeed;

	if (RETRO_KeyState(SDL_SCANCODE_PAGEUP)) RETRO_TerrainView.horizon += timestep * RETRO_TerrainView.horizonspeed;
	if (RETRO_KeyState(SDL_SCANCODE_PAGEDOWN)) RETRO_TerrainView.horizon -= timestep * RETRO_TerrainView.horizonspeed;
	if (RETRO_TerrainView.horizon < 0) RETRO_TerrainView.horizon = 0;
	if (RETRO_TerrainView.horizon > RETRO_HEIGHT) RETRO_TerrainView.horizon = RETRO_HEIGHT;

	if (RETRO_Terrain.wrap) {
		RETRO_Camera.x = RETRO_WrapCoordinate(RETRO_Camera.x, RETRO_Terrain.width);
		RETRO_Camera.z = RETRO_WrapCoordinate(RETRO_Camera.z, RETRO_Terrain.height);
	}
	RETRO_Camera.heading = fmodf(RETRO_Camera.heading, (float)(2 * M_PI));
	if (RETRO_Camera.heading < 0) RETRO_Camera.heading += (float)(2 * M_PI);

	if (!RETRO_Camera.flycam) {
		float ground = RETRO_TerrainHeightLinear(RETRO_Camera.x, RETRO_Camera.z);
		float target = ground + RETRO_Camera.eye;
		RETRO_Camera.height += (target - RETRO_Camera.height) * (1.0f - expf(-timestep / RETRO_Camera.follow));
		if (RETRO_Camera.height < ground + RETRO_Camera.clearance) RETRO_Camera.height = ground + RETRO_Camera.clearance;
	}
}

//
// Stand the wrapping camera on the ground at x, z, at its ride height
//
// Worth doing once before the first frame: the follow above closes a gap
// gradually, so a camera left at zero would otherwise rise into place from
// under the map while the demo is already being watched. The island look
// is RETRO_LookDownAtTerrain, which stands outside the patch.
//
void RETRO_PlaceTerrainCamera(float x, float z)
{
	RETRO_Camera.x = x;
	RETRO_Camera.z = z;
	RETRO_Camera.height = RETRO_TerrainHeightLinear(x, z) + RETRO_Camera.eye;
}

//
// Take the wrapping camera and its draw distance down for a world that is
// this much of the default
//
// Speeds and ride height come from a fresh camera so a copied number is not
// one that stops agreeing the day the default moves. Distance comes from
// RETRO_TERRAIN_DISTANCE for the same reason, not from whatever the live
// view already holds: calling this twice would otherwise quarter the view
// twice. The turn speed is left alone, an angle being the one thing a
// change of scale does not touch. The island look is not scaled: it stands
// outside a finite patch and LookDown sets its own pose.
//
void RETRO_ScaleTerrainWorld(float worldscale)
{
	RETRO_TerrainCamera defaults;
	RETRO_Camera.movespeed = defaults.movespeed * worldscale;
	RETRO_Camera.flyspeed = defaults.flyspeed * worldscale;
	RETRO_Camera.eye = defaults.eye * worldscale;
	RETRO_Camera.clearance = defaults.clearance * worldscale;
	RETRO_TerrainView.distance = (int)(RETRO_TERRAIN_DISTANCE * worldscale);
}

//
// The island pose as a frame
//
// The patch centre and the sincos of pitch and rotation are what every
// sample shares. Taken once per draw, the way a heading's basis is taken
// once for the wrapping look. RETRO_TerrainIslandEye then uses this
// rather than taking them per sample.
//
RETRO_TerrainIslandFrame RETRO_BuildTerrainIslandFrame(void)
{
	RETRO_TerrainIslandFrame frame;
	frame.centerx = (RETRO_Terrain.width - 1) * 0.5f;
	frame.centerz = (RETRO_Terrain.height - 1) * 0.5f;
	frame.sinpitch = sinf(RETRO_Island.pitch);
	frame.cospitch = cosf(RETRO_Island.pitch);
	frame.sinrot = sinf(RETRO_Island.rotation);
	frame.cosrot = cosf(RETRO_Island.rotation);
	return frame;
}

//
// Stand outside the patch, looking down so it fills the frame
//
// Writes the lens as well as the pose, replacing the wrapping defaults.
// The wrapping lens is a right angle across, made for a world that goes
// on. This one is longer: the patch is small and the camera is close, and
// a wide angle would show mostly the ground between here and there. The
// horizon sits a little above the middle so the look-down has sky left at
// the top. The cull follows from the new focals, still a little wider
// than the lens, so a sample on the edge is projected rather than thrown
// out before it is.
//
// Height is set so the view centre lands on the patch: tan of the pitch
// times how far the camera stands from the centre, plus the ground there.
// The camera stands on that same centre in x, the mid-point of the
// vertices: (width - 1) / 2, not width / 2. The frame derives the spin
// axis independently of Island.x, and the eye subtracts Island.x after
// the turn, so a mismatch is a constant translation of the whole scene -
// the island sits permanently off the screen centre it was posed to fill,
// at every rotation.
// The near stop is the centre plus the circumradius of the patch plus the
// near plane, not the unrotated south edge: the island turns about its
// centre, and a stop at the south edge would let a 45 degree yaw put the
// camera inside it looking at a corner. The circumradius is the hypotenuse
// of the half-extents, which equals the half-depth times sqrt(2) only when
// the patch is square.
//
void RETRO_LookDownAtTerrain(void)
{
	RETRO_TerrainView.focalx = RETRO_WIDTH * 0.54f;
	RETRO_TerrainView.focaly = RETRO_HEIGHT * 0.78f;
	RETRO_TerrainView.horizon = RETRO_HEIGHT * 0.46f;
	RETRO_TerrainView.nearplane = 4.0f;

	float centerx = (RETRO_Terrain.width - 1) * 0.5f;
	float centerz = (RETRO_Terrain.height - 1) * 0.5f;
	RETRO_Island.x = centerx;
	RETRO_Island.z = RETRO_Terrain.height + 35.0f;
	RETRO_Island.rotation = 0;
	RETRO_Island.pitch = 0.70f;
	RETRO_Island.nearestz = centerz + hypotf(centerx, centerz) + RETRO_TerrainView.nearplane;
	RETRO_Island.farthestz = RETRO_Terrain.height + 70.0f;
	RETRO_Island.height = RETRO_TerrainHeight(centerx, centerz) + tanf(RETRO_Island.pitch) * (RETRO_Island.z - centerz);
}

//
// A map point in the island camera's own frame
//
// The wrapping equivalent is RETRO_TerrainCameraOffset, then world height
// minus the eye. Here the patch spins about its centre and the view is
// pitched, so altitude is mixed into depth and the result is already an
// Eye. The caller decides what is worth drawing - near plane, side per
// depth - then RETRO_ProjectTerrainView, the way wrapping dots test the
// wedge before they project.
//
RETRO_TerrainEye RETRO_TerrainIslandEye(float x, float y, float z, const RETRO_TerrainIslandFrame &frame)
{
	float localx = x - frame.centerx;
	float localz = z - frame.centerz;
	float worldx = frame.centerx + localx * frame.cosrot - localz * frame.sinrot;
	float worldz = frame.centerz + localx * frame.sinrot + localz * frame.cosrot;

	float horizontal = RETRO_Island.z - worldz;
	float vertical = y - RETRO_Island.height;
	RETRO_TerrainEye eye;
	eye.side = worldx - RETRO_Island.x;
	eye.depth = horizontal * frame.cospitch - vertical * frame.sinpitch;
	eye.height = vertical * frame.cospitch + horizontal * frame.sinpitch;
	return eye;
}

//
// Drive the island from the keyboard
//
// Left/Right turn the patch, not the camera. That is the whole difference
// from RETRO_UpdateTerrainCamera, whose Left/Right yaw the eye. Up/Down
// dolly along the viewing axis, and are held between the stops LookDown
// set, so the finite patch stays in frame. There is no horizon slide: the
// pitch is the look.
//
void RETRO_UpdateTerrainIsland(float timestep)
{
	float distance = timestep * RETRO_Island.movespeed;
	float rotation = timestep * RETRO_Island.turnspeed;
	if (RETRO_KeyState(SDL_SCANCODE_LEFT)) RETRO_Island.rotation += rotation;
	if (RETRO_KeyState(SDL_SCANCODE_RIGHT)) RETRO_Island.rotation -= rotation;
	if (RETRO_KeyState(SDL_SCANCODE_UP) || RETRO_KeyState(SDL_SCANCODE_W)) RETRO_Island.z -= distance;
	if (RETRO_KeyState(SDL_SCANCODE_DOWN) || RETRO_KeyState(SDL_SCANCODE_S)) RETRO_Island.z += distance;
	if (RETRO_Island.z < RETRO_Island.nearestz) RETRO_Island.z = RETRO_Island.nearestz;
	if (RETRO_Island.z > RETRO_Island.farthestz) RETRO_Island.z = RETRO_Island.farthestz;
	RETRO_Island.rotation = fmodf(RETRO_Island.rotation, (float)(2.0 * M_PI));
}

//
// A wrapping height field built by midpoint displacement, and the slope-shaded
// colour map that goes with it. The map wraps: this is a torus, not an island.
//
// The map has to be square with power-of-two sides: the diamond-square walk
// steps by halves of one length, and the column walks that read it wrap by
// masking. Said once here, where a size that is not fails at startup, rather
// than found out as a walk reading down the wrong rows.
//
// Midpoint displacement fills the byte range far more evenly than a photograph
// does, so a stored height here is not a world height. 0.6 of a stored byte is
// the same ground as one of those, and worldscale then brings it into this
// world. Flying that world at the matching size is RETRO_ScaleTerrainWorld,
// not this: generating the ground should not change how fast the camera flies.
//
// The field starts flat. The first diamond-square pass reads one seed: on
// a wrapping square map it runs once at p = width, x = y = 0, and all four
// corners fold onto heightmap[0]. A dirty buffer would be a different map
// every run; this zeros it rather than asking the caller to have done so.
//
void RETRO_BuildDisplacementTerrain(unsigned char *heightmap, unsigned char *colormap, int width, int height, float worldscale)
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

		for (int y = 0; y < height; y += p) {
			for (int x = 0; x < width; x += p) {
				int a = heightmap[y * width + x];
				int b = heightmap[WRAP(y + p, height) * width + x];
				int c = heightmap[y * width + WRAP(x + p, width)];
				int d = heightmap[WRAP(y + p, height) * width + WRAP(x + p, width)];

				heightmap[y * width + WRAP(x + p2, width)] = CLAMP256(((a + c) / 2) + (RANDOM(k) - k2));
				heightmap[WRAP(y + p2, height) * width + WRAP(x + p2, width)] = CLAMP256(((a + b + c + d) / 4) + (RANDOM(k) - k2));
				heightmap[WRAP(y + p2, height) * width + x] = CLAMP256(((a + b) / 2) + (RANDOM(k) - k2));
			}
		}
	}

	for (int k = 0; k < 5; k++) {
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				heightmap[y * width + x] = (
					heightmap[WRAP(y + 1, height) * width + x] +
					heightmap[y * width + WRAP(x + 1, width)] +
					heightmap[WRAP(y - 1, height) * width + x] +
					heightmap[y * width + WRAP(x - 1, width)]
				) / 4;
			}
		}
	}

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			colormap[y * width + x] = CLAMP256(128 + (heightmap[WRAP(y + 1, height) * width + WRAP(x + 1, width)] - heightmap[y * width + x]) * 6);
		}
	}
}

#endif
