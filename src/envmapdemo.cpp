//
// Environment map
//
// One mesh, two ways of reading a map by the surface normal, and the
// lighting one of them stands in for. Tab steps through the three.
//
// Matcap (RETRO_POLY_MATCAP): the map is a shiny ball under a light, baked
// once. A pixel with unit normal N reads it at W/2 + radius * Nxy, so the map
// is a lighting response and the highlight stays put on screen while the
// mesh turns under it. C steps through three such maps, all of the
// library's default material on the same pink:
//
// The round map, RETRO_CreateRoundAnglePhongMap and
// RETRO_CreateAnglePhongPalette in 6 bits, steps evenly in angle with the
// distance from its middle. It is read in a disk of radius 90 rather than the
// whole map, which gives a tight highlight on a dark ramp, and a normal at
// the silhouette lands short of the map's rim all the way round.
//
// The diamond map, RETRO_CreateDiamondAnglePhongMap through the same palette,
// is the one maskdemo lights the mask with. It steps evenly in angle along
// each axis instead, which matches the round map along the axes but bends its
// rim in along the diagonals, into a diamond. At maskdemo's radius of 90 a
// diagonal silhouette lands on that rim, where the ramp drops to black within
// a texel and the cube's corners speckle, so it is read at 85 here; the
// corners still come out darker than the sides.
//
// The phong map, RETRO_CreatePhongMap over RETRO_CreateMaterialPalette, is
// the lighting itself, read across the whole map, so a normal turned past
// the silhouette lands on the dark rim. It is 1024 texels wide, since at 256
// the lookup steps a whole shade a texel near the highlight, and those steps
// ring it.
//
// Phong (RETRO_POLY_PHONG): what the matcap bakes, worked out at every pixel
// instead. The interpolated normal is lit by a light from the viewer,
// L = (0, 0, -1), through the palette the chosen map is read through, and
// that map is drawn in the corner, for comparison. Against the phong map
// the two draw alike; the angle maps' highlight is the tighter, since their
// lookup stretches the angle. Nothing is looked up, so the light could move;
// the matcap's could not.
//
// Environment (RETRO_POLY_ENVIRONMENT): the map is a picture of the
// surroundings, a Blinn/Newell sphere map of the ray reflected about N. The
// lookup is W (1/2 + Nx/2), the whole map, and a normal past the silhouette
// folds back inward instead of darkening, since a mirror still reflects
// something there. With perspective on, each pixel reflects its own view ray
// from the eye rather than the view axis, which matcap has no notion of.
//
// The meshes show where that matters. The cube's corners carry the corner
// diagonals, so its normals sweep across each face as though it were
// rounded. The flat cube's corners carry the face's own normal: under matcap,
// and under environment without perspective, each face is one colour, and
// only a reflected view ray gives it a picture, the slice of the room a flat
// mirror there would show. The mask is curved throughout, and on it the two
// ways of reflecting differ little.
//
// The map being read is drawn as an inset in the top right corner, so a
// patch of the surface can be matched to the part of the map it samples
// without the map drowning the mesh in its own colours. Euler angles live on
// 2π. The mask starts at ax = −π/2, az = π so its face is upright.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"
#include "lib/retrofont.h"
#include "lib/retropalette.h"

#define ROTATION_SPEED 1 // radians a second, about each axis
#define ENVMAP_SIZE 256 // the environment map is square, and this is its side in pixels
#define ANGLEMAP_SIZE 256 // the round and diamond maps are square, and this is their side in pixels
#define ANGLEMAP_FACE vec3{ 0.86f, 0.23f, 0.59f } // the material, as intensities; RETRO_DEEPPINK
#define ROUNDMAP_RADIUS 90 // texels from the round map's middle to where a normal at the silhouette reads it
#define DIAMONDMAP_RADIUS 85 // texels from the diamond map's middle to where a normal at the silhouette reads it
#define PHONGMAP_SIZE 1024 // the phong map is square, and this is its side in pixels
#define INSET_SIZE 64 // side in pixels of the map's inset
#define INSET_MARGIN 10 // pixels from the inset to the screen's edges, as the text keeps

enum { ASSET_ENVMAP };
enum { MATCAP_ROUND, MATCAP_DIAMOND, MATCAP_PHONG };

struct Mesh {
	const char *filename, *name;
	float scale, ax, az; // projection scale, and the pose the rotation starts from
};

static const Mesh Meshes[] = {
	{"assets/cube.obj", "CUBE", RETRO_PROJECTION_SCALE, 0, 0},
	{"assets/cubeflat.obj", "FLAT CUBE", RETRO_PROJECTION_SCALE, 0, 0},
	{"assets/mask.obj", "MASK", 0.5f, -M_PI / 2, M_PI}
};

static const int MeshCount = sizeof(Meshes) / sizeof(Meshes[0]);

static unsigned char RoundMapData[ANGLEMAP_SIZE * ANGLEMAP_SIZE];
static unsigned char DiamondMapData[ANGLEMAP_SIZE * ANGLEMAP_SIZE];
static RETRO_Palette AngleMapPalette[RETRO_COLORS];
static unsigned char PhongMapData[PHONGMAP_SIZE * PHONGMAP_SIZE];

struct Matcap {
	const char *name;
	unsigned char *data;
	int size, radius; // side of the square map, and texels from its middle to where a normal at the silhouette reads it
};

static const Matcap Matcaps[] = {
	{"ROUND MAP", RoundMapData, ANGLEMAP_SIZE, ROUNDMAP_RADIUS},
	{"DIAMOND MAP", DiamondMapData, ANGLEMAP_SIZE, DIAMONDMAP_RADIUS},
	{"PHONG MAP", PhongMapData, PHONGMAP_SIZE, PHONGMAP_SIZE / 2}
};

static const int MatcapCount = sizeof(Matcaps) / sizeof(Matcaps[0]);

static RETRO_POLY_TYPE RenderType = RETRO_POLY_MATCAP;
static int MeshIndex = 0;
static int MatcapIndex = MATCAP_ROUND;
static bool Perspective = true;
static bool Paused = false;
static double RotationTime = 0; // seconds the mesh has turned for, which stops while paused

static void SelectMode(void)
{
	for (int i = 0; i < MeshCount; i++) {
		Model3D *model = RETRO_Get3DModel(i);
		if (RenderType == RETRO_POLY_ENVIRONMENT) {
			model->envmap = RETRO_ImageData(ASSET_ENVMAP);
			model->envmapwidth = ENVMAP_SIZE;
			model->envmapradius = ENVMAP_SIZE / 2;
		} else {
			model->envmap = Matcaps[MatcapIndex].data;
			model->envmapwidth = Matcaps[MatcapIndex].size;
			model->envmapradius = Matcaps[MatcapIndex].radius;
		}
		model->envmapheight = model->envmapwidth;
		model->envmapperspective = Perspective;
	}

	if (RenderType == RETRO_POLY_ENVIRONMENT) {
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_ENVMAP));
	} else if (MatcapIndex == MATCAP_PHONG) {
		RETRO_CreatePlasticPalette();
	} else {
		RETRO_Set6bitPalette(AngleMapPalette);
	}
}

void DEMO_Render(double time, double deltatime)
{
	// Handle keys
	if (RETRO_KeyPressed(SDL_SCANCODE_TAB)) {
		if (RenderType == RETRO_POLY_MATCAP) {
			RenderType = RETRO_POLY_PHONG;
		} else if (RenderType == RETRO_POLY_PHONG) {
			RenderType = RETRO_POLY_ENVIRONMENT;
		} else {
			RenderType = RETRO_POLY_MATCAP;
		}
		SelectMode();
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_M)) {
		MeshIndex = (MeshIndex + 1) % MeshCount;
		SelectMode();
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_P) && RenderType == RETRO_POLY_ENVIRONMENT) {
		Perspective = (Perspective == false);
		SelectMode();
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_C)) {
		MatcapIndex = (MatcapIndex + 1) % MatcapCount;
		SelectMode();
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_SPACE)) {
		Paused = (Paused == false);
	}

	const Mesh *mesh = &Meshes[MeshIndex];
	Model3D *model = RETRO_Get3DModel(MeshIndex);

	// Calculate rotation
	if (Paused == false) {
		RotationTime += deltatime;
	}
	float ax = fmod(mesh->ax + RotationTime * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(RotationTime * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(mesh->az + RotationTime * ROTATION_SPEED, 2 * M_PI);

	// Draw mesh
	RETRO_RotateModel(ax, ay, az, model);
	RETRO_ProjectModel(mesh->scale, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, model);
	RETRO_RenderModel(RenderType, RETRO_SHADE_NONE, model);

	// Draw map, the one read or, under phong, the matcap it replaces. Index 0 is
	// skipped, which leaves what is beneath it
	RETRO_DrawSprite(RETRO_WIDTH - INSET_MARGIN - INSET_SIZE / 2, INSET_MARGIN + INSET_SIZE / 2, INSET_SIZE, INSET_SIZE, model->envmapwidth, model->envmapheight, model->envmap, 0);

	// Draw state
	if (RenderType == RETRO_POLY_MATCAP) {
		RETRO_PutString("MATCAP", 10, 10, 255);
	} else if (RenderType == RETRO_POLY_PHONG) {
		RETRO_PutString("PHONG", 10, 10, 255);
	} else {
		RETRO_PutString("ENVIRONMENT", 10, 10, 255);
	}
	RETRO_PutString(mesh->name, 10, 20, 255);
	if (RenderType == RETRO_POLY_ENVIRONMENT) {
		RETRO_PutString(Perspective ? "PERSPECTIVE ON" : "PERSPECTIVE OFF", 10, 30, 255);
	} else {
		RETRO_PutString("PERSPECTIVE N/A", 10, 30, 255);
	}
	char mapname[32];
	snprintf(mapname, sizeof(mapname), "%s %dX%d", RenderType == RETRO_POLY_ENVIRONMENT ? "PHOTO MAP" : Matcaps[MatcapIndex].name, model->envmapwidth, model->envmapheight);
	RETRO_PutString(mapname, 10, 40, 255);
	RETRO_PutString("SPACE PAUSE", 10, 212, 255);
	RETRO_PutString("TAB MODE  M MESH  P PERSPECTIVE  C MAP", 10, 222, 255);
}

void DEMO_Initialize(void)
{
	// Load assets
	RETRO_LoadImage("assets/mask_envmap_256x256.pcx");
	RETRO_SetFont(RETRO_FONT_VGA_8X8);

	// Init maps
	RETRO_CreateRoundAnglePhongMap(RoundMapData, ANGLEMAP_SIZE, ANGLEMAP_SIZE);
	RETRO_CreateDiamondAnglePhongMap(DiamondMapData, ANGLEMAP_SIZE, ANGLEMAP_SIZE);
	RETRO_CreateAnglePhongPalette(ANGLEMAP_FACE, RETRO_K_SPECULAR, RETRO_K_FALLOFF, AngleMapPalette, 63);
	RETRO_CreatePhongMap(PhongMapData, PHONGMAP_SIZE, PHONGMAP_SIZE);

	// Load meshes
	for (const Mesh &mesh : Meshes) {
		Model3D *model = RETRO_Load3DModel(mesh.filename);
		model->c = RETRO_PHONG_OFFSET;
		model->shades = RETRO_PHONG_SHADES;
	}
	SelectMode();

	// Set up light source
	RETRO_InitializeLightSource(0, 0, -1);
}
