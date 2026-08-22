//
// Voxel landscape, Gouraud shaded
//
// A 256x256 wrapping height map, built by midpoint displacement rather than
// loaded, and drawn front to back one depth slice at a time. At depth z the
// lens meets the ground in a segment whose midpoint is z straight ahead and
// whose half width is z * (W/2) / focalx, the same tan as the pinhole, so its
// two ends are
//
//   left  = z (forward - slope * right)
//   right = z (forward + slope * right)
//
// where slope is that half width at unit depth, and forward and right are the
// two directions a heading faces, which the terrain library turns out of it.
// Each column samples the map on that segment,
// bilinearly: this map is 256 cells across where the photographed ones are
// 1024, so a sample covers four times the screen a sample over one of those
// does, and read cell by cell the ground arrives in visible blocks. Indices are
// a floor and a mask, not a cast toward zero: (-1, 0) is the last texel, not 0.
// The camera lives on the same torus; the heading lives in [0, 2pi).
//
// A height difference dh at depth z is a pinhole
//
//   y = horizon + dh * focal / z
//
// y grows down, so a peak (dh < 0) sits above the horizon. Slices are painted
// down to the highest y already filled (hiddeny), so nearer ground occludes
// farther ground. dz grows with z, so far slices are coarser.
//
// What is painted between two slices is a strip of ground seen edge on, and it
// is shaded from the colour the last slice sampled to the colour this one did
// rather than filled with the second of those. That is the whole difference
// between this and voxellandscape.cpp, which fills. A filled strip is one
// flat tone and a slope arrives as a stack of them, exactly as flat shading
// a face differs from Gouraud shading it, and the near strips stand tall
// enough to show it.
//
// The lens and the camera are the terrain library's, so what is this effect's
// own is the frustum-to-ground segment above and the column walk that fills it.
// voxellandscape3.cpp walks the same segment the classic way, a cell read
// whole and a strip filled with one tone, which is what a photographed map
// can carry: its own grain covers a seam. Nothing here does. The colour is
// a slope shade with no texture in it, and the map is a quarter the size,
// so both the cell and the strip arrive large - hence the filtering above
// and the shading here.
//
// The world here is a quarter of the one those maps make: 256 cells across
// where they are 1024. Every length is scaled to suit - the height the ground
// is read at, the height the eye rides at, how fast it flies, how far it sees
// and how closely the slices are spaced - and a pinhole cannot tell a world
// from a quarter of one seen from a quarter of the height, so through the same
// lens this renders the same landscape as the demos over the larger maps, and
// is crossed in the same time.
//
// Midpoint displacement fills the byte range far more evenly than a photograph
// does, so a stored height here is not a world height and the terrain carries
// the scale between them. The column walk still works in stored heights: the
// eye is taken into the map's own units once a frame and the focal length
// scaled with it, which is the same projection with the multiply lifted out of
// a few hundred thousand pixels.
//
// Left/Right turn and Up/Down move along the viewing direction. W/S are
// alternate forward/back controls and A/D strafe. Tab toggles a flycam, in
// which R and F raise and lower the camera. PageUp and PageDown move the
// horizon, which tilts the view up and down. Colour is an 8-bit grey ramp.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"
#include "lib/retroterrain.h"

#define MAP_WIDTH 256
#define MAP_HEIGHT 256

// How much of a world this one is. The map is 256 cells across where the
// photographed ones are 1024, and every length below is the library's own
// default taken down by this rather than a number written out again.
#define VOXEL_WORLD_SCALE 0.25f

// Where the first slice stands and how much dz gains each slice, so far samples
// thin out. Nearer than that first step the ground projects far below the
// screen anyway, and a slice at zero has no focal length to be divided by. A
// quarter of a world is crossed in a quarter of the steps, so both come down
// with it and the same number of slices still covers the ground.
#define VOXEL_NEAR (1.0f * VOXEL_WORLD_SCALE)
#define VOXEL_LOD (0.005f * VOXEL_WORLD_SCALE)

// The two maps this demo builds for itself. A terrain is described by the bytes
// it is read from and not by where they came from, so these are handed to it
// exactly as a loaded map's would be, and the camera below rides them without
// knowing the difference.
unsigned char HeightMap[MAP_WIDTH * MAP_HEIGHT];
unsigned char ColorMap[MAP_WIDTH * MAP_HEIGHT];

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
	float horizon = RETRO_TerrainView.horizon;
	float distance = RETRO_TerrainView.distance;

	// The eye in stored heights and the focal length in world units per stored
	// height: the walk then subtracts a map byte from a map byte and scales the
	// difference once, instead of scaling every byte it reads. Both sides of the
	// projection are moved together, so it is the picture it always was.
	float focal = RETRO_TerrainView.focaly * RETRO_Terrain.scale;
	float cameraheight = RETRO_Camera.height / RETRO_Terrain.scale;

	// The slice is the lens's frustum on the ground. The two ends keep that
	// bearing at every depth, so the heading and the slope are taken once here
	// and only scaled by z below
	RETRO_TerrainBasis basis = RETRO_TerrainHeadingBasis(RETRO_Camera.heading);
	RETRO_TerrainSlice slice = RETRO_TerrainViewSlice(basis);

	// Where each column has been painted down to, and the colour it last
	// sampled, which is the near edge of the strip the next slice paints
	int hiddeny[RETRO_WIDTH];
	float lastcolor[RETRO_WIDTH];
	for (int i = 0; i < RETRO_WIDTH; i++) {
		hiddeny[i] = RETRO_HEIGHT;
		lastcolor[i] = -1;
	}
	float deltaz = VOXEL_NEAR;

	// Draw from front to back
	for (float z = VOXEL_NEAR; z < distance; z += deltaz) {
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
			int heightonscreen = (int)((cameraheight - RETRO_TerrainSampleLinear(heightmap, plx, plz)) * invz + horizon);
			float color = RETRO_TerrainSampleLinear(colormap, plx, plz);

			if (heightonscreen < hiddeny[x]) {
				// The nearest slice has no strip in front of it to be shaded
				// across, so it takes its own colour at both ends and comes out
				// flat, which is what one edge of ground looks like
				if (lastcolor[x] < 0) {
					lastcolor[x] = color;
				}

				// The strip runs from the row the last slice stopped at, where
				// it is that slice's colour, up to this one, where it is this
				// slice's. Walking down from the top is walking backward along
				// that ramp.
				float colorstep = (color - lastcolor[x]) / (hiddeny[x] - heightonscreen);
				float currentcolor = color;

				// A near strip can stand off the top of the screen. The ramp is
				// carried forward to the first row that fits rather than begun
				// there, so the part drawn keeps the shade it would have had
				// and the strip does not brighten as its top leaves the screen.
				if (heightonscreen < 0) {
					currentcolor += colorstep * heightonscreen;
					heightonscreen = 0;
				}

				for (int y = heightonscreen; y < hiddeny[x]; y++) {
					buffer[y * RETRO_WIDTH + x] = (unsigned char)currentcolor;
					currentcolor -= colorstep;
				}

				hiddeny[x] = heightonscreen;
			}

			// Kept whether the strip was drawn or hidden: it is where the
			// ground is, not what reached the screen
			lastcolor[x] = color;
			plx += dx;
			plz += dz;
		}
		deltaz += VOXEL_LOD;
	}
}

void DEMO_Initialize(void)
{
	// Init palette
	RETRO_CreateGradientPalette(0, RETRO_COLORS, RETRO_BLACK, RETRO_WHITE);

	RETRO_BuildDisplacementTerrain(HeightMap, ColorMap, MAP_WIDTH, MAP_HEIGHT, VOXEL_WORLD_SCALE);
	RETRO_ScaleTerrainWorld(VOXEL_WORLD_SCALE);
	RETRO_PlaceTerrainCamera(0, 0);
}
