//
// Voxel landscape, over a photographed map
//
// A wrapping height map, drawn front to back one depth slice at a time. At
// depth z the lens meets the ground in a segment whose midpoint is z straight
// ahead and whose half width is z * (W/2) / focalx, the same tan as the
// pinhole, so its two ends are
//
//   left  = z (forward - slope * right)
//   right = z (forward + slope * right)
//
// where slope is that half width at unit depth, and forward and right are the
// two directions a heading faces, which the terrain library turns out of it.
// Each column samples the map on that segment.
// Indices are a floor and a mask, not a cast toward zero: (-1, 0) is the last
// texel, not 0. The camera lives on the same torus; the heading lives in [0, 2pi).
//
// A height difference dh at depth z is a pinhole
//
//   y = horizon + dh * focal / z
//
// y grows down, so a peak (dh < 0) sits above the horizon. Slices are painted
// down to the highest y already filled (hiddeny), so nearer ground occludes
// farther ground. dz grows with z, so far slices are coarser.
//
// The map, the lens and the camera all come from the terrain library, so this
// effect describes only what is its own: the frustum-to-ground segment above,
// and the column walk that fills it.
//
// This is the classic technique unsoftened: a cell read whole and a strip
// filled with one tone. It holds up here because a photograph brings its own
// grain to cover a seam, and because 1024 cells across a 320 pixel screen is a
// cell to a few pixels. voxellandscape.cpp and voxellandscape2.cpp walk the
// same segment over a map they generate, a quarter the size and slope shaded
// with no texture in it, and have to filter and shade to make that ground
// read.
//
// Left/Right turn and Up/Down move along the viewing direction. W/S are
// alternate forward/back controls and A/D strafe. Tab toggles a flycam, in
// which R and F raise and lower the camera. PageUp and PageDown move the
// horizon, which tilts the view up and down.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retroterrain.h"

#define VOXEL_LOD 0.005f // added to dz each slice, so far samples thin out

void DEMO_Render(double deltatime)
{
	RETRO_UpdateTerrainCamera(deltatime);

	unsigned char *colormap = RETRO_Terrain.colormap;
	unsigned char *heightmap = RETRO_Terrain.heightmap;
	unsigned char *buffer = RETRO_FrameBuffer();

	// The column walk below reads these once per pixel, and there are a few
	// hundred thousand of those a frame. Taken into locals the compiler can hold
	// them in registers; left as fields of a library global it has to assume the
	// walk might change them and load each one again every time.
	//
	// The two masks are how a sample is folded back into the map. Both sides are
	// powers of two, which DEMO_Initialize insists on, so the fold is an and
	// rather than the division a modulo by a size read from the image would be.
	int mapxmask = RETRO_Terrain.width - 1;
	int mapzmask = RETRO_Terrain.height - 1;
	int mapstride = RETRO_Terrain.width;
	float focal = RETRO_TerrainView.focaly;
	float horizon = RETRO_TerrainView.horizon;
	float distance = RETRO_TerrainView.distance;
	float cameraheight = RETRO_Camera.height;

	// The slice is the lens's frustum on the ground. The two ends keep that
	// bearing at every depth, so the heading and the slope are taken once here
	// and only scaled by z below
	RETRO_TerrainBasis basis = RETRO_TerrainHeadingBasis(RETRO_Camera.heading);
	RETRO_TerrainSlice slice = RETRO_TerrainViewSlice(basis);

	int hiddeny[RETRO_WIDTH];
	for (int i = 0; i < RETRO_WIDTH; i++) {
		hiddeny[i] = RETRO_HEIGHT;
	}
	float deltaz = 1.0f;

	// Draw from front to back
	for (float z = 1.0f; z < distance; z += deltaz) {
		float plx = z * slice.leftx;
		float plz = z * slice.leftz;
		float prx = z * slice.rightx;
		float prz = z * slice.rightz;

		float dx = (prx - plx) / RETRO_WIDTH;
		float dz = (prz - plz) / RETRO_WIDTH;

		plx += RETRO_Camera.x;
		plz += RETRO_Camera.z;
		float invz = focal / z;
		for (int x = 0; x < RETRO_WIDTH; x++) {
			// floor and not a cast: the walk runs negative wherever the camera
			// looks back across the map's origin, and truncating toward zero
			// would fold the cell either side of it onto the same sample
			int mapoffset = ((int)floorf(plz) & mapzmask) * mapstride + ((int)floorf(plx) & mapxmask);
			int heightonscreen = (int)((cameraheight - heightmap[mapoffset]) * invz + horizon);
			if (heightonscreen < 0) {
				heightonscreen = 0;
			}
			unsigned char color = colormap[mapoffset];
			for (int y = heightonscreen; y < hiddeny[x]; y++) {
				buffer[y * RETRO_WIDTH + x] = color;
			}
			if (heightonscreen < hiddeny[x]) {
				hiddeny[x] = heightonscreen;
			}
			plx += dx;
			plz += dz;
		}
		deltaz += VOXEL_LOD;
	}
}

void DEMO_Initialize(void)
{
	// The stored height is the world height here: the column walk reads the map
	// byte straight out, so describing the terrain at unit scale keeps what the
	// camera rides on and what the columns are drawn from the same number
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");

	// The column walk wraps a sample per pixel and does it by masking, so it
	// needs the map's sides to be powers of two. Said once here, where a map
	// that is not fails at startup, rather than found out as a walk reading
	// down the wrong rows
	if (!RETRO_TerrainWrapsByMask()) {
		RETRO_RageQuit("Terrain map sides must be powers of two\n");
	}

	// Sky, in an entry the color map never uses, the same colour the other
	// photographed landscapes open on
	RETRO_SetColor(0, 20, 24, 42);

	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
