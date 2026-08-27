//
// Occlusion-correct dot landscape
//
// The 1024x1024 voxel landscape rendered as a point cloud. Unlike a voxel
// renderer, it never joins samples into columns: every terrain texel is an
// independent dot and the empty space between them remains visible.
//
// That empty space is what a depth buffer alone cannot hide behind. Here each
// dot's ray back to the camera is walked against the height field as well, so
// a dot standing behind a ridge is dropped rather than left showing through
// the gaps between the samples in front of it.
//
// The map wraps, making the camera's world unbounded. Dot density decreases
// smoothly with distance using a world-anchored pattern, avoiding transition
// rings and preventing dots from swimming when the camera moves.
//
// Left/Right turn and Up/Down move along the new viewing direction. W/S are
// alternate forward/back controls and A/D provide optional strafing. Combined
// movement is normalized, so diagonals have the same speed. By default the
// camera follows the ground; Tab toggles a flycam, in which R and F raise
// and lower the camera. PageUp and PageDown move the horizon, which tilts
// the view up and down.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrocolor.h"
#include "lib/retroterrain.h"

// The farthest view the arrays below have room for. How far the dots are
// actually drawn is the library's draw distance, the same one every landscape
// here is seen at; this is only the widest one the lists can be filled from,
// and DEMO_Initialize refuses to start on a view asking for more.
#define WORLD_MAX_DISTANCE RETRO_TERRAIN_DISTANCE

// Every dot the collector can produce. It scans the square reaching
// WORLD_MAX_DISTANCE around the camera and looks at each cell in it once, so it
// cannot hand back more dots than that square holds. Everything it tests only
// throws cells away, which leaves most of this unused: the list runs to a few
// tens of thousands in practice. That slack is free. A static array of this size
// is a range of addresses and a length in the executable, not bytes in the file
// and not memory; each page becomes real the first time it is written, so the
// pages past the dots the camera found never cost anything.
#define MAX_PROJECTED_DOTS ((2 * WORLD_MAX_DISTANCE + 4) * (2 * WORLD_MAX_DISTANCE + 4))

// The cell a dot came from is what the ray march below walks toward, and the
// ground height there follows from it, so only the cell is carried.
struct ProjectedDot {
	int x, y;					// Screen position
	int worldx, worldz;			// Map cell it was sampled from
	float depth;				// Camera-forward distance, and the order everything below depends on
	unsigned char color;
};

static ProjectedDot ProjectedDots[MAX_PROJECTED_DOTS];
static int ProjectedDotCount = 0;

// Front-to-back order, counted rather than compared
//
// A dot's depth is at most WORLD_MAX_DISTANCE, so counting how many fall in
// each whole unit of it and handing out the places in one sweep orders the lot
// in a pass, with no comparisons. What sorting them by exact depth would decide
// beyond that is which of two dots within the same unit goes first; here that
// is the order the collector met them in, which is fixed by the map rather than
// by whichever way a sort happened to break the tie.
static int DotOrder[MAX_PROJECTED_DOTS];
static int DotsAtDepth[WORLD_MAX_DISTANCE + 1];

static void OrderDotsByDepth(void)
{
	memset(DotsAtDepth, 0, sizeof(DotsAtDepth));
	for (int i = 0; i < ProjectedDotCount; i++) {
		DotsAtDepth[(int)ProjectedDots[i].depth]++;
	}
	for (int depth = 0, place = 0; depth <= WORLD_MAX_DISTANCE; depth++) {
		int dots = DotsAtDepth[depth];
		DotsAtDepth[depth] = place;
		place += dots;
	}
	for (int i = 0; i < ProjectedDotCount; i++) {
		DotOrder[DotsAtDepth[(int)ProjectedDots[i].depth]++] = i;
	}
}

// Test the complete camera-to-dot ray against the continuous height field.
// The screen-column horizon is a fast broad-phase test, but cannot account for
// holes between projected samples. Ray sampling closes those holes and also
// handles ridges which happen to project between two screen columns.
static bool TerrainOccludes(const ProjectedDot &dot)
{
	const float sampleStep = 2.0f;
	float height = RETRO_TerrainHeight(dot.worldx, dot.worldz);
	for (float distance = RETRO_TerrainView.nearplane + sampleStep;
		distance < dot.depth - sampleStep; distance += sampleStep) {
		float t = distance / dot.depth;
		float x = RETRO_Camera.x + (dot.worldx - RETRO_Camera.x) * t;
		float z = RETRO_Camera.z + (dot.worldz - RETRO_Camera.z) * t;
		float rayheight = RETRO_Camera.height + (height - RETRO_Camera.height) * t;
		if (RETRO_TerrainHeightLinear(x, z) > rayheight + 1.0f) return true;
	}
	return false;
}

// Project a smoothly distance-thinned grid inside the view radius.
static void CollectTerrainDots(float maxdistance)
{
	RETRO_TerrainBasis basis = RETRO_TerrainHeadingBasis(RETRO_Camera.heading);
	float maxdistance2 = maxdistance * maxdistance;
	int minx = (int)floorf(RETRO_Camera.x - maxdistance);
	int maxx = (int)ceilf(RETRO_Camera.x + maxdistance);
	int minz = (int)floorf(RETRO_Camera.z - maxdistance);
	int maxz = (int)ceilf(RETRO_Camera.z + maxdistance);

	for (int z = minz; z <= maxz; z++) {
		float dz = z - RETRO_Camera.z;
		for (int x = minx; x <= maxx; x++) {
			float dx = x - RETRO_Camera.x;
			float radius2 = dx * dx + dz * dz;
			if (radius2 > maxdistance2) continue;

			RETRO_TerrainOffset offset;
			RETRO_TerrainPoint point;
			if (!RETRO_ProjectTerrainDot(x, z, dx, dz, radius2, basis, &offset, &point)) continue;

			ProjectedDots[ProjectedDotCount++] = { (int)point.sx, (int)point.sy, x, z, offset.depth, RETRO_TerrainColor(x, z) };
		}
	}
}

void DEMO_Render(double deltatime)
{
	RETRO_UpdateTerrainCamera(deltatime);
	ProjectedDotCount = 0;
	CollectTerrainDots(RETRO_TerrainView.distance);

	// A pixel z-buffer only resolves dots that project to the very same pixel.
	// Take them front-to-back instead and maintain the highest visible terrain
	// point in each screen column, so a dot below a nearer ridge is hidden.
	OrderDotsByDepth();
	int columntop[RETRO_WIDTH];
	for (int x = 0; x < RETRO_WIDTH; x++) columntop[x] = RETRO_HEIGHT;
	for (int i = 0; i < ProjectedDotCount; i++) {
		const ProjectedDot &dot = ProjectedDots[DotOrder[i]];
		if (dot.y >= columntop[dot.x]) continue;
		if (TerrainOccludes(dot)) continue;
		RETRO_PutPixel(dot.x, dot.y, dot.color);
		columntop[dot.x] = dot.y;
	}
}

void DEMO_Initialize(void)
{
	RETRO_LoadTerrain("assets/voxel_color_1024x1024.pcx", "assets/voxel_height_1024x1024.pcx");
	RETRO_SetColor(0, RETRO_NIGHTSKY);

	// The two lists above hold a view reaching WORLD_MAX_DISTANCE. Said here,
	// where a wider one stops the demo at startup, rather than found out as a
	// collector writing past the end of them
	if (RETRO_TerrainView.distance > WORLD_MAX_DISTANCE) {
		RETRO_RageQuit("Terrain view distance is beyond what the dot lists hold\n");
	}

	RETRO_PlaceTerrainCamera(RETRO_Terrain.width * 0.5f, (float)RETRO_TERRAIN_DISTANCE);
}
