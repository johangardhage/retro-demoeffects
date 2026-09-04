//
// Inward torus: an inward-facing OBJ shell rendered by the retro 3D pipeline.
// The torus UVs and texture are procedural; fish paths and timing approximate
// the original. Vertex lighting is interpolated by the Gouraud texture mapper.
//
// Author: Johan Gardhage <johan.gardhage@gmail.com>
//
#include "lib/retro.h"
#include "lib/retromain.h"
#include "lib/retrorender.h"

#define ROTATION_SPEED 0.42f
#define START_ROTATION 1.80f
#define RING_RADIUS (5271.0f / 2920.0f)
#define CAMERA_DISTANCE (RING_RADIUS + 0.80f)
#define FISH_COUNT 9
#define FISH_SCALE 0.70f
#define FISH_SWIM_SPEED 0.55f
#define FISH_START_PHASE 0.35f
#define FISH_ANIMATION_SPEED 1.8f
#define FISH_ANIMATION_OFFSET 0.13f
#define FISH_LANE_OFFSET 0.28f
#define FISH_DRIFT 0.08f
#define FISH_FRAMES 17
#define CAMERA_FOV 0.82f
#define MODEL_SCALE 100.0f
#define TEXTURE_SIZE 1024
#define MATERIAL_SHADES 64
#define FISH_PALETTE_START (MATERIAL_COUNT * MATERIAL_SHADES)
#define BAND_REPEATS 10
#define ROWS_PER_REPEAT 3
#define TEETH_PER_RING 32

enum Material {
	MATERIAL_GREEN,
	MATERIAL_BLUE,
	MATERIAL_COUNT
};

static Model3D *Torus;
static Model3D *Fish;
static unsigned char Texture[TEXTURE_SIZE * TEXTURE_SIZE];
static unsigned char Shades[RETRO_MAX_SHADING_COLORS];

static void RenderTorus(float phase, float focal)
{
	// Spin the texture around the ring, then roll its axis in the screen.
	// This keeps the camera inside the tube throughout the rotation.
	RETRO_RotateModel(phase * 0.61f, 0.0f, phase, Torus);
	RETRO_TranslateModel(0.0f, 0.0f, CAMERA_DISTANCE - focal / MODEL_SCALE, Torus);
	RETRO_ProjectModel(MODEL_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Torus, focal);
	RETRO_RenderModel(RETRO_POLY_TEXTURE, RETRO_SHADE_GOURAUD, Torus);
}

static void RenderFish(double time, float phase, float focal)
{
	// The final pose repeats pose zero so the last-to-first blend is smooth.
	// Reuse one model instance; the shared depth buffer resolves every fish
	// against the torus and previously drawn fish.
	for (int i = 0; i < FISH_COUNT; i++) {
		float swim = time * FISH_SWIM_SPEED + FISH_START_PHASE + i * (2.0f * M_PI / FISH_COUNT);
		float cycle = time * FISH_ANIMATION_SPEED + i * FISH_ANIMATION_OFFSET;
		RETRO_MorphModel(cycle - floorf(cycle), Fish);
		RETRO_InitializeFaceNormals(Fish);
		RETRO_InitializeVertexNormals(Fish);
		RETRO_RotateModel(swim + M_PI * 0.5f, 0.0f, phase, Fish);
		float ringy = RING_RADIUS * sinf(swim);
		float ringz = -RING_RADIUS * cosf(swim);
		// Two shallow lanes leave the central bands clear more often.
		float lane = (i & 1) ? -FISH_LANE_OFFSET : FISH_LANE_OFFSET;
		float drift = lane + FISH_DRIFT * sinf(swim * 2.0f + i);
		RETRO_TranslateModel(drift * cosf(phase) - ringy * sinf(phase),
			drift * sinf(phase) + ringy * cosf(phase),
			ringz + CAMERA_DISTANCE - focal / MODEL_SCALE, Fish);
		RETRO_ProjectModel(MODEL_SCALE, RETRO_WIDTH / 2, RETRO_HEIGHT / 2, Fish, focal);
		RETRO_RenderModel(RETRO_POLY_GOURAUD, RETRO_SHADE_NONE, Fish, false);
	}
}

static void InitializePalette(void)
{
	// Blue and green also form the two halves of each triangle cell.
	// Give each a full 64-shade ramp; entries 128 onward are for the fish.
	for (int material = 0; material < MATERIAL_COUNT; material++) {
		for (int shade = 0; shade < MATERIAL_SHADES; shade++) {
			float t = (float)shade / (MATERIAL_SHADES - 1);
			// Spend more of the ramp on the highlight to soften its contours.
			float s = t * (2.0f - t);
			float glow = powf(s, 6.0f);
			int r, g, b;
			if (material == MATERIAL_GREEN) {
				r = 18 + 70*s; g = 36 + 125*s; b = 20 + 66*s;
			} else {
				r = 2 + 12*s; g = 32 + 120*s; b = 40 + 128*s;
			}
			// Blend toward white instead of clipping each channel at a
			// different shade, which creates abrupt highlight transitions.
			r += (245 - r) * glow; g += (250 - g) * glow; b += (250 - b) * glow;
			RETRO_SetColor(material * MATERIAL_SHADES + shade, CLAMP256(r), CLAMP256(g), CLAMP256(b));
		}
	}
}

static void InitializeShadeTable(void)
{
	// Inverse of the palette's highlight-weighted shade spacing.
	for (int material = 0; material < MATERIAL_COUNT; material++) {
		for (int shade = 0; shade < RETRO_SHADES; shade++) {
			float s = (float)shade / (RETRO_SHADES - 1);
			float t = 1.0f - sqrtf(1.0f - s);
			Shades[material * RETRO_SHADES + shade] = material * MATERIAL_SHADES
				+ (int)(t * (MATERIAL_SHADES - 1) + 0.5f);
		}
	}
}

static void InitializeTexture(void)
{
	// Bands advance across the tube (OBJ v) and extend around the ring.
	// The triangle teeth repeat along the ring (OBJ u), all leaning the
	// same way. Across the initial view the order is green, blue, teeth.
	for (int y = 0; y < TEXTURE_SIZE; y++) {
		float tube = (y + 0.5f) / TEXTURE_SIZE;
		// At the inner equator (v = 0.5), place the solid blue/green
		// boundary at the centre rather than the edge of a teeth row.
		float row = (tube - 0.5f) * (BAND_REPEATS * ROWS_PER_REPEAT)
			+ BAND_REPEATS * ROWS_PER_REPEAT + 1.0f;
		int band = (int)floorf(row) % ROWS_PER_REPEAT;
		float across = row - floorf(row);
		for (int x = 0; x < TEXTURE_SIZE; x++) {
			float ring = (x + 0.5f) / TEXTURE_SIZE;
			float toothcell = ring * TEETH_PER_RING;
			unsigned char value;
			if (band == 0) {
				value = MATERIAL_BLUE;
			} else if (band == 1) {
				value = MATERIAL_GREEN;
			} else {
				float tooth = toothcell - floorf(toothcell);
				// Start the teeth row in blue against the preceding green
				// band, and finish in green against the next blue band.
				// Reversing these merges the teeth into the solid ribbons.
				bool triangle = across < tooth;
				value = triangle ? MATERIAL_BLUE : MATERIAL_GREEN;
			}
			Texture[y * TEXTURE_SIZE + x] = value;
		}
	}
}

static void InitializeModels(void)
{
	Torus = RETRO_Load3DModel("assets/cybomantorus.obj");
	// The loader supplies UVs in its default 256-texel space. A larger map
	// keeps magnified diagonal borders from turning into chunky stair steps.
	for (int i = 0; i < Torus->uvs; i++) {
		Torus->uv[i].u *= (float)TEXTURE_SIZE / RETRO_TEXMAP_SIZE;
		Torus->uv[i].v *= (float)TEXTURE_SIZE / RETRO_TEXMAP_SIZE;
	}
	Torus->texmap = Texture;
	Torus->texmapwidth = Torus->texmapheight = TEXTURE_SIZE;
	Torus->shadetable = Shades;
	Torus->shades = RETRO_SHADES;

	Fish = RETRO_Load3DModel("assets/fish_00.obj", "assets/fish_%02d.obj", FISH_FRAMES);
	// Scale every morph target once so swimming never restores the old size.
	for (int i = 0; i < Fish->frames * Fish->vertices * 3; i++) {
		Fish->frame[i] *= FISH_SCALE;
	}
	Fish->c = FISH_PALETTE_START;
	Fish->shades = RETRO_COLORS - FISH_PALETTE_START;
}

void DEMO_Render(double time, double deltatime)
{
	float phase = time * ROTATION_SPEED + START_ROTATION;
	float focal = RETRO_HEIGHT * 0.5f / CAMERA_FOV;
	RenderTorus(phase, focal);
	RenderFish(time, phase, focal);
}

void DEMO_Initialize(void)
{
	InitializePalette();
	RETRO_CreatePhongPalette(FISH_PALETTE_START, RETRO_COLORS, RETRO_AZURE, 0.95f, 8.0f);
	InitializeShadeTable();
	InitializeTexture();
	InitializeModels();
	RETRO_InitializeLightSource(0.0f, 0.0f, -1.0f);
}
