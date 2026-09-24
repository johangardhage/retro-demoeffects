//
// Environment map
//
// One mesh, two ways of reading a map by the surface normal, and the
// lighting one of them stands in for. Tab steps through the three.
//
// Matcap (RETRO_POLY_MATCAP): the map is a shiny ball under a light, baked
// once. A pixel with unit normal N reads it at W/2 + radius * Nxy, so the map
// is a lighting response and the highlight stays put on screen while the
// mesh turns under it. C switches between two such maps, both of the
// library's default material on the same pink:
//
// The angle map, RETRO_CreateAnglePhongMap and RETRO_CreateAnglePhongPalette
// in 6 bits, is the one maskdemo lights the mask with. It is read in a disk
// of radius 90 rather than the whole map, which with the map's even steps in
// angle gives a tight highlight on a dark ramp; a normal at the silhouette
// lands short of the map's own rim.
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
// that map is drawn behind the mesh, for comparison. Against the phong map
// the two draw alike; the angle map's highlight is the tighter, since its
// lookup stretches the angle. Nothing is looked up, so the light could move;
// the matcap's could not.
//
// Environment (RETRO_POLY_ENVIRONMENT): the map is a picture of the
// surroundings, a Blinn/Newell sphere map of the ray reflected about N. The
// lookup is W (1/2 + Nx/2), the whole map, and a normal past the silhouette
// folds back inward instead of darkening, since a mirror still reflects
// something there. With perspective on, each vertex reflects its own view
// ray from the eye rather than the view axis, which matcap has no notion of.
//
// The meshes show where that matters. The cube's corners carry the corner
// diagonals, so its normals sweep across each face as though it were
// rounded. The flat cube's corners carry the face's own normal: under matcap,
// and under environment without perspective, each face is one colour, and
// only a reflected view ray gives it a picture, the slice of the room a flat
// mirror there would show. The mask is curved throughout, and on it the two
// ways of reflecting differ little.
//
// The map being read is drawn behind the mesh, squeezed to the screen's
// height, so a patch of the surface can be matched to the part of the map it
// samples. Euler angles live on 2π. The mask starts at ax = −π/2, az = π so
// its face is upright.
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
#define ANGLEMAP_SIZE 256 // the angle map is square, and this is its side in pixels
#define ANGLEMAP_FACE vec3{ 0.86f, 0.23f, 0.59f } // the material, as intensities; RETRO_DEEPPINK
#define ANGLEMAP_RADIUS 90 // texels from the angle map's middle to where a normal at the silhouette reads it
#define PHONGMAP_SIZE 1024 // the phong map is square, and this is its side in pixels

enum { ASSET_ENVMAP };

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

static RETRO_POLY_TYPE RenderType = RETRO_POLY_MATCAP;
static int MeshIndex = 0;
static bool Perspective = true;
static bool AngleMap = true;

static unsigned char AngleMapData[ANGLEMAP_SIZE * ANGLEMAP_SIZE];
static RETRO_Palette AngleMapPalette[RETRO_COLORS];
static unsigned char PhongMapData[PHONGMAP_SIZE * PHONGMAP_SIZE];

static void SelectMode(void)
{
	for (int i = 0; i < MeshCount; i++) {
		Model3D *model = RETRO_Get3DModel(i);
		if (RenderType == RETRO_POLY_ENVIRONMENT) {
			model->envmap = RETRO_ImageData(ASSET_ENVMAP);
			model->envmapwidth = ENVMAP_SIZE;
			model->envmapradius = ENVMAP_SIZE / 2;
		} else if (AngleMap) {
			model->envmap = AngleMapData;
			model->envmapwidth = ANGLEMAP_SIZE;
			model->envmapradius = ANGLEMAP_RADIUS;
		} else {
			model->envmap = PhongMapData;
			model->envmapwidth = PHONGMAP_SIZE;
			model->envmapradius = PHONGMAP_SIZE / 2;
		}
		model->envmapheight = model->envmapwidth;
		model->envmapperspective = Perspective;
	}

	if (RenderType == RETRO_POLY_ENVIRONMENT) {
		RETRO_Set6bitPalette(RETRO_ImagePalette(ASSET_ENVMAP));
	} else if (AngleMap) {
		RETRO_Set6bitPalette(AngleMapPalette);
	} else {
		RETRO_CreatePlasticPalette();
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
	if (RETRO_KeyPressed(SDL_SCANCODE_P)) {
		Perspective = (Perspective == false);
		SelectMode();
	}
	if (RETRO_KeyPressed(SDL_SCANCODE_C)) {
		AngleMap = (AngleMap == false);
		SelectMode();
	}

	const Mesh *mesh = &Meshes[MeshIndex];
	Model3D *model = RETRO_Get3DModel(MeshIndex);

	// Calculate rotation
	float ax = fmod(mesh->ax + time * ROTATION_SPEED, 2 * M_PI);
	float ay = fmod(time * ROTATION_SPEED, 2 * M_PI);
	float az = fmod(mesh->az + time * ROTATION_SPEED, 2 * M_PI);

	// Draw map, the one read or, under phong, the matcap it replaces. Index 0 is
	// skipped, which leaves the cleared frame's own 0
	RETRO_DrawSprite(RETRO_WIDTH / 2, RETRO_HEIGHT / 2, RETRO_HEIGHT, RETRO_HEIGHT, model->envmapwidth, model->envmapheight, model->envmap, 0);

	// Draw mesh
	RETRO_RotateModel(ax, ay, az, model);
	RETRO_ProjectModel(mesh->scale, RETRO_WIDTH / 2.0, RETRO_HEIGHT / 2.0, model);
	RETRO_RenderModel(RenderType, RETRO_SHADE_NONE, model);

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
		RETRO_PutString(AngleMap ? "ANGLE MAP" : "PHONG MAP", 10, 30, 255);
	}
	RETRO_PutString("TAB MODE  M MESH  P PERSPECTIVE  C MAP", 10, 222, 255);
}

void DEMO_Initialize(void)
{
	// Load assets
	RETRO_LoadImage("assets/mask_envmap_256x256.pcx");
	RETRO_SetFont(RETRO_FONT_VGA_8X8);

	// Init maps
	RETRO_CreateAnglePhongMap(AngleMapData, ANGLEMAP_SIZE, ANGLEMAP_SIZE);
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
